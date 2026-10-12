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

#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace ginnunga::concepts {

/**
 * @brief StorageConcept defines the contract for pluggable sparse storage backends.
 * 
 * Backends manage sparse association of Key to Value, emphasizing minimal
 * overhead, cache-locality, and predictable access latency.
 */
template <typename S, typename Key, typename Value>
concept StorageConcept = requires(S& storage, const S& const_storage, const Key& key, const Value& val) {
    // Lookup returns a pointer to Value (nullptr if absent)
    { storage.find(key) } -> std::same_as<Value*>;
    { const_storage.find(key) } -> std::same_as<const Value*>;

    // Insertion of key-value pair
    { storage.insert(key, val) } -> std::same_as<bool>;

    // Erase by key
    { storage.erase(key) } -> std::same_as<bool>;

    // Metrics & introspection
    { const_storage.size() } -> std::convertible_to<std::size_t>;
    { const_storage.capacity() } -> std::convertible_to<std::size_t>;
    { const_storage.memory_footprint() } -> std::convertible_to<std::size_t>;

    // Resetting
    { storage.clear() } -> std::same_as<void>;
};

} // namespace ginnunga::concepts
