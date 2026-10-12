// SPDX-License-Identifier: AGPL-3.0-or-later
// Project Ginnunga: High-Performance C++ Sparse Cache Algorithm Framework
// Copyright (C) 2026 Zeeshan Qazi <zeeshan@zeeshan.im>
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

#include <list>
#include <unordered_map>
#include <optional>
#include <cstddef>
#include "ginnunga/concepts/eviction.hpp"

namespace ginnunga::eviction {

/**
 * @brief LRUPolicy tracks recency of key accesses using a doubly-linked list
 * and hash map for O(1) hit promotion, insert, and eviction of the least recently used key.
 */
template <typename Key>
class LRUPolicy {
public:
    LRUPolicy() = default;
    ~LRUPolicy() = default;

    void on_hit(const Key& key) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.splice(list_.begin(), list_, it->second);
        }
    }

    void on_insert(const Key& key) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.splice(list_.begin(), list_, it->second);
        } else {
            list_.push_front(key);
            map_[key] = list_.begin();
        }
    }

    void on_erase(const Key& key) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.erase(it->second);
            map_.erase(it);
        }
    }

    std::optional<Key> evict() {
        if (list_.empty()) return std::nullopt;
        Key lru_key = list_.back();
        list_.pop_back();
        map_.erase(lru_key);
        return lru_key;
    }

    void reset() {
        list_.clear();
        map_.clear();
    }

    [[nodiscard]] std::size_t size() const noexcept { return list_.size(); }
    [[nodiscard]] bool empty() const noexcept { return list_.empty(); }

private:
    std::list<Key> list_;
    std::unordered_map<Key, typename std::list<Key>::iterator> map_;
};

static_assert(concepts::EvictionConcept<LRUPolicy<int>, int>,
              "LRUPolicy must satisfy EvictionConcept");

} // namespace ginnunga::eviction
