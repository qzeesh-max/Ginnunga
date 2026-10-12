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
#include <vector>
#include <utility>
#include <limits>
#include "ginnunga/concepts/storage.hpp"

namespace ginnunga::storage {

/**
 * @brief SlotArrayMap (Sparse Set) utilizes two arrays:
 * 1. Sparse Array: maps sparse Key to an index in the dense array.
 * 2. Dense Arrays: stores active Keys and Values contiguously in memory.
 * 
 * Guarantees O(1) lookup, O(1) insertion, O(1) swap-and-pop deletion,
 * and cache-line-friendly linear iteration over active elements.
 */
template <std::integral Key, typename Value>
class SlotArrayMap {
public:
    static constexpr std::size_t INVALID_INDEX = std::numeric_limits<std::size_t>::max();

    explicit SlotArrayMap(std::size_t initial_sparse_capacity = 65536)
        : sparse_(initial_sparse_capacity, INVALID_INDEX) {
        dense_keys_.reserve(1024);
        dense_values_.reserve(1024);
    }

    Value* find(const Key& key) noexcept {
        const std::size_t k = static_cast<std::size_t>(key);
        if (k >= sparse_.size()) return nullptr;
        const std::size_t dense_idx = sparse_[k];
        if (dense_idx < dense_keys_.size() && dense_keys_[dense_idx] == key) {
            return &dense_values_[dense_idx];
        }
        return nullptr;
    }

    const Value* find(const Key& key) const noexcept {
        const std::size_t k = static_cast<std::size_t>(key);
        if (k >= sparse_.size()) return nullptr;
        const std::size_t dense_idx = sparse_[k];
        if (dense_idx < dense_keys_.size() && dense_keys_[dense_idx] == key) {
            return &dense_values_[dense_idx];
        }
        return nullptr;
    }

    bool insert(const Key& key, const Value& val) {
        const std::size_t k = static_cast<std::size_t>(key);
        if (k >= sparse_.size()) {
            sparse_.resize(std::max(sparse_.size() * 2, k + 1), INVALID_INDEX);
        }

        std::size_t dense_idx = sparse_[k];
        if (dense_idx < dense_keys_.size() && dense_keys_[dense_idx] == key) {
            // Update existing entry
            dense_values_[dense_idx] = val;
            return true;
        }

        // New insertion: append to dense arrays
        dense_idx = dense_keys_.size();
        dense_keys_.push_back(key);
        dense_values_.push_back(val);
        sparse_[k] = dense_idx;
        return true;
    }

    bool erase(const Key& key) {
        const std::size_t k = static_cast<std::size_t>(key);
        if (k >= sparse_.size()) return false;

        const std::size_t dense_idx = sparse_[k];
        if (dense_idx >= dense_keys_.size() || dense_keys_[dense_idx] != key) {
            return false;
        }

        // Swap with last element to maintain contiguous density
        const std::size_t last_idx = dense_keys_.size() - 1;
        if (dense_idx != last_idx) {
            const Key last_key = dense_keys_[last_idx];
            dense_keys_[dense_idx] = last_key;
            dense_values_[dense_idx] = std::move(dense_values_[last_idx]);
            sparse_[static_cast<std::size_t>(last_key)] = dense_idx;
        }

        dense_keys_.pop_back();
        dense_values_.pop_back();
        sparse_[k] = INVALID_INDEX;
        return true;
    }

    void clear() {
        for (const Key key : dense_keys_) {
            const std::size_t k = static_cast<std::size_t>(key);
            if (k < sparse_.size()) {
                sparse_[k] = INVALID_INDEX;
            }
        }
        dense_keys_.clear();
        dense_values_.clear();
    }

    [[nodiscard]] std::size_t size() const noexcept { return dense_keys_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return sparse_.size(); }
    [[nodiscard]] std::size_t memory_footprint() const noexcept {
        return (sparse_.capacity() * sizeof(std::size_t)) +
               (dense_keys_.capacity() * sizeof(Key)) +
               (dense_values_.capacity() * sizeof(Value));
    }

    // Direct access to dense contiguous arrays for high-performance iteration
    [[nodiscard]] const std::vector<Key>& dense_keys() const noexcept { return dense_keys_; }
    [[nodiscard]] const std::vector<Value>& dense_values() const noexcept { return dense_values_; }
    [[nodiscard]] std::vector<Value>& dense_values() noexcept { return dense_values_; }

private:
    std::vector<std::size_t> sparse_;
    std::vector<Key> dense_keys_;
    std::vector<Value> dense_values_;
};

static_assert(concepts::StorageConcept<SlotArrayMap<uint32_t, int>, uint32_t, int>,
              "SlotArrayMap must satisfy StorageConcept");

} // namespace ginnunga::storage
