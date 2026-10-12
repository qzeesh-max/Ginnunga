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
#include <array>
#include <optional>
#include <functional>
#include <concepts>
#include "ginnunga/concepts/concurrency.hpp"

namespace ginnunga::concurrency {

/**
 * @brief ThreadLocalCacheFront acts as an ultra-fast L1 front-cache per thread.
 * Reads hit the thread-local ring buffer without acquiring locks or touching
 * shared atomic structures, falling back to the global sparse cache only on an L1 miss.
 */
template <typename Key, typename Value, std::size_t FRONT_CAPACITY = 64>
class ThreadLocalCacheFront {
public:
    struct Entry {
        Key key{};
        Value value{};
        bool valid{false};
    };

    ThreadLocalCacheFront() = default;
    ~ThreadLocalCacheFront() = default;

    // ConcurrencyConcept compliance
    int acquire_read() noexcept { return 0; }
    void release_read() noexcept {}
    void retire(std::function<void()> cleanup) {
        if (cleanup) cleanup();
    }
    void synchronize() noexcept {
        invalidate_local();
    }

    [[nodiscard]] Value* local_lookup(const Key& key) noexcept {
        auto& buffer = get_local_buffer();
        std::size_t slot = hash_key(key) % FRONT_CAPACITY;
        if (buffer[slot].valid && buffer[slot].key == key) {
            return &buffer[slot].value;
        }
        return nullptr;
    }

    void local_insert(const Key& key, const Value& val) noexcept {
        auto& buffer = get_local_buffer();
        std::size_t slot = hash_key(key) % FRONT_CAPACITY;
        buffer[slot].key = key;
        buffer[slot].value = val;
        buffer[slot].valid = true;
    }

    void local_invalidate(const Key& key) noexcept {
        auto& buffer = get_local_buffer();
        std::size_t slot = hash_key(key) % FRONT_CAPACITY;
        if (buffer[slot].valid && buffer[slot].key == key) {
            buffer[slot].valid = false;
        }
    }

    void invalidate_local() noexcept {
        auto& buffer = get_local_buffer();
        for (auto& entry : buffer) {
            entry.valid = false;
        }
    }

private:
    static std::size_t hash_key(const Key& key) noexcept {
        return std::hash<Key>{}(key);
    }

    static std::array<Entry, FRONT_CAPACITY>& get_local_buffer() noexcept {
        static thread_local std::array<Entry, FRONT_CAPACITY> buffer{};
        return buffer;
    }
};

static_assert(concepts::ConcurrencyConcept<ThreadLocalCacheFront<int, int>>,
              "ThreadLocalCacheFront must satisfy ConcurrencyConcept");

} // namespace ginnunga::concurrency
