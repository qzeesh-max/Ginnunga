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

#include <chrono>
#include <map>
#include <unordered_map>
#include <optional>
#include <cstddef>
#include "ginnunga/concepts/eviction.hpp"

namespace ginnunga::eviction {

/**
 * @brief TTLPolicy tracks key expiration using high-resolution monotonic clock
 * timestamps. Evicts keys that have exceeded their Time-To-Live, or the oldest
 * expiring key when cache capacity is reached.
 */
template <typename Key, typename Clock = std::chrono::steady_clock>
class TTLPolicy {
public:
    using TimePoint = typename Clock::time_point;
    using Duration = typename Clock::duration;

    explicit TTLPolicy(Duration default_ttl = std::chrono::seconds(60), bool sliding_ttl = true)
        : default_ttl_(default_ttl), sliding_ttl_(sliding_ttl) {}

    void on_hit(const Key& key) {
        if (!sliding_ttl_) return;
        auto it = key_to_time_.find(key);
        if (it != key_to_time_.end()) {
            TimePoint old_tp = it->second;
            TimePoint new_tp = Clock::now() + default_ttl_;

            // Remove from time index
            auto range = time_to_key_.equal_range(old_tp);
            for (auto tit = range.first; tit != range.second; ++tit) {
                if (tit->second == key) {
                    time_to_key_.erase(tit);
                    break;
                }
            }

            it->second = new_tp;
            time_to_key_.emplace(new_tp, key);
        }
    }

    void on_insert(const Key& key) {
        TimePoint expire_at = Clock::now() + default_ttl_;
        on_erase(key); // Remove any existing entry

        key_to_time_[key] = expire_at;
        time_to_key_.emplace(expire_at, key);
    }

    void on_erase(const Key& key) {
        auto it = key_to_time_.find(key);
        if (it != key_to_time_.end()) {
            TimePoint tp = it->second;
            auto range = time_to_key_.equal_range(tp);
            for (auto tit = range.first; tit != range.second; ++tit) {
                if (tit->second == key) {
                    time_to_key_.erase(tit);
                    break;
                }
            }
            key_to_time_.erase(it);
        }
    }

    std::optional<Key> evict() {
        if (time_to_key_.empty()) return std::nullopt;

        auto earliest_it = time_to_key_.begin();
        Key victim = earliest_it->second;
        time_to_key_.erase(earliest_it);
        key_to_time_.erase(victim);
        return victim;
    }

    [[nodiscard]] bool is_expired(const Key& key) const {
        auto it = key_to_time_.find(key);
        if (it == key_to_time_.end()) return true;
        return Clock::now() >= it->second;
    }

    void reset() {
        key_to_time_.clear();
        time_to_key_.clear();
    }

    [[nodiscard]] std::size_t size() const noexcept { return key_to_time_.size(); }
    [[nodiscard]] bool empty() const noexcept { return key_to_time_.empty(); }

    void set_ttl(Duration ttl) noexcept { default_ttl_ = ttl; }
    [[nodiscard]] Duration ttl() const noexcept { return default_ttl_; }

private:
    Duration default_ttl_;
    bool sliding_ttl_{true};
    std::unordered_map<Key, TimePoint> key_to_time_;
    std::multimap<TimePoint, Key> time_to_key_;
};

static_assert(concepts::EvictionConcept<TTLPolicy<int>, int>,
              "TTLPolicy must satisfy EvictionConcept");

} // namespace ginnunga::eviction
