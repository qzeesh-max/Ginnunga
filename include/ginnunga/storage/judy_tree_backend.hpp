// SPDX-License-Identifier: AGPL-3.0-or-later
// Project Ginnunga: High-Performance C++ Sparse Cache Algorithm Framework
// Copyright (C) 2026 Zeeshan Qazi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <memory>
#include <vector>
#include <array>
#include <bit>
#include <algorithm>
#include "ginnunga/concepts/storage.hpp"

namespace ginnunga::storage {

/**
 * @brief JudyTreeBackend implements a highly compressed adaptive radix trie
 * (inspired by Judy Arrays / Adaptive Radix Tree). It slices integer keys byte-by-byte
 * and utilizes compact branch nodes, minimizing memory footprint while maintaining
 * predictable logarithmic/bounded lookups without hash overhead.
 */
template <std::integral Key, typename Value>
class JudyTreeBackend {
public:
    static constexpr std::size_t KEY_BYTES = sizeof(Key);

    JudyTreeBackend() = default;
    ~JudyTreeBackend() { clear(); }

    JudyTreeBackend(const JudyTreeBackend&) = delete;
    JudyTreeBackend& operator=(const JudyTreeBackend&) = delete;

    JudyTreeBackend(JudyTreeBackend&& other) noexcept
        : root_(std::move(other.root_)),
          size_(other.size_) {
        other.size_ = 0;
    }

    JudyTreeBackend& operator=(JudyTreeBackend&& other) noexcept {
        if (this != &other) {
            clear();
            root_ = std::move(other.root_);
            size_ = other.size_;
            other.size_ = 0;
        }
        return *this;
    }

    Value* find(const Key& key) noexcept {
        if (!root_) return nullptr;
        Node* curr = root_.get();

        for (std::size_t depth = 0; depth < KEY_BYTES; ++depth) {
            std::uint8_t byte = extract_byte(key, depth);
            if (depth == KEY_BYTES - 1) {
                // Leaf level
                return curr->find_leaf_value(byte);
            }
            curr = curr->find_child(byte);
            if (!curr) return nullptr;
        }
        return nullptr;
    }

    const Value* find(const Key& key) const noexcept {
        return const_cast<JudyTreeBackend*>(this)->find(key);
    }

    bool insert(const Key& key, const Value& val) {
        if (!root_) {
            root_ = std::make_unique<Node>();
        }

        Node* curr = root_.get();
        for (std::size_t depth = 0; depth < KEY_BYTES; ++depth) {
            std::uint8_t byte = extract_byte(key, depth);
            if (depth == KEY_BYTES - 1) {
                bool is_new = curr->insert_leaf(byte, val);
                if (is_new) ++size_;
                return true;
            }
            curr = curr->get_or_create_child(byte);
        }
        return false;
    }

    bool erase(const Key& key) {
        if (!root_ || size_ == 0) return false;
        bool erased = erase_recursive(root_.get(), key, 0);
        if (erased) {
            --size_;
            if (size_ == 0) root_.reset();
        }
        return erased;
    }

    void clear() {
        root_.reset();
        size_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept {
        return std::size_t(1) << (sizeof(Key) * 8 - 1);
    }

    [[nodiscard]] std::size_t memory_footprint() const noexcept {
        return root_ ? root_->memory_footprint() + sizeof(*this) : sizeof(*this);
    }

private:
    static std::uint8_t extract_byte(Key key, std::size_t depth) noexcept {
        std::size_t shift = (KEY_BYTES - 1 - depth) * 8;
        return static_cast<std::uint8_t>((static_cast<std::uint64_t>(key) >> shift) & 0xFF);
    }

    struct Node {
        struct ChildEntry {
            std::uint8_t key_byte;
            std::unique_ptr<Node> child;
        };

        struct LeafEntry {
            std::uint8_t key_byte;
            Value value;
        };

        std::vector<ChildEntry> children;
        std::vector<LeafEntry> leaves;

        Node* find_child(std::uint8_t byte) const noexcept {
            for (const auto& entry : children) {
                if (entry.key_byte == byte) return entry.child.get();
            }
            return nullptr;
        }

        Node* get_or_create_child(std::uint8_t byte) {
            for (auto& entry : children) {
                if (entry.key_byte == byte) return entry.child.get();
            }
            children.push_back(ChildEntry{
                .key_byte = byte,
                .child = std::make_unique<Node>()
            });
            return children.back().child.get();
        }

        Value* find_leaf_value(std::uint8_t byte) noexcept {
            for (auto& leaf : leaves) {
                if (leaf.key_byte == byte) return &leaf.value;
            }
            return nullptr;
        }

        bool insert_leaf(std::uint8_t byte, const Value& val) {
            for (auto& leaf : leaves) {
                if (leaf.key_byte == byte) {
                    leaf.value = val;
                    return false; // updated
                }
            }
            leaves.push_back(LeafEntry{.key_byte = byte, .value = val});
            return true; // newly inserted
        }

        bool erase_leaf(std::uint8_t byte) {
            auto it = std::find_if(leaves.begin(), leaves.end(),
                                   [byte](const LeafEntry& l) { return l.key_byte == byte; });
            if (it != leaves.end()) {
                leaves.erase(it);
                return true;
            }
            return false;
        }

        [[nodiscard]] std::size_t memory_footprint() const noexcept {
            std::size_t total = sizeof(Node) +
                (children.capacity() * sizeof(ChildEntry)) +
                (leaves.capacity() * sizeof(LeafEntry));
            for (const auto& c : children) {
                if (c.child) total += c.child->memory_footprint();
            }
            return total;
        }
    };

    bool erase_recursive(Node* node, Key key, std::size_t depth) {
        std::uint8_t byte = extract_byte(key, depth);
        if (depth == KEY_BYTES - 1) {
            return node->erase_leaf(byte);
        }

        for (auto it = node->children.begin(); it != node->children.end(); ++it) {
            if (it->key_byte == byte) {
                bool res = erase_recursive(it->child.get(), key, depth + 1);
                if (it->child->children.empty() && it->child->leaves.empty()) {
                    node->children.erase(it);
                }
                return res;
            }
        }
        return false;
    }

    std::unique_ptr<Node> root_{nullptr};
    std::size_t size_{0};
};

static_assert(concepts::StorageConcept<JudyTreeBackend<uint32_t, int>, uint32_t, int>,
              "JudyTreeBackend must satisfy StorageConcept");

} // namespace ginnunga::storage
