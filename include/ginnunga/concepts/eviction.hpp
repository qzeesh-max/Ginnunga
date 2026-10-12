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

#include <concepts>
#include <cstddef>
#include <optional>

namespace ginnunga::concepts {

/**
 * @brief EvictionConcept defines how replacement policies track cache accesses
 * and select candidate keys for eviction when the cache reaches capacity.
 */
template <typename E, typename Key>
concept EvictionConcept = requires(E& policy, const E& const_policy, const Key& key) {
    // Notify policy that a key was successfully accessed
    { policy.on_hit(key) } -> std::same_as<void>;

    // Notify policy that a new key was inserted into storage
    { policy.on_insert(key) } -> std::same_as<void>;

    // Notify policy that a key was removed
    { policy.on_erase(key) } -> std::same_as<void>;

    // Select candidate key for eviction (std::nullopt if empty)
    { policy.evict() } -> std::same_as<std::optional<Key>>;

    // Clear eviction state
    { policy.reset() } -> std::same_as<void>;

    // Track policy tracking size
    { const_policy.size() } -> std::convertible_to<std::size_t>;
};

} // namespace ginnunga::concepts
