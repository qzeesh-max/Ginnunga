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
#include <memory_resource>

namespace ginnunga::concepts {

/**
 * @brief MemoryResourceConcept requires compatibility with std::pmr::memory_resource
 * or an interface providing allocate, deallocate, and is_equal.
 */
template <typename M>
concept MemoryResourceConcept = requires(M& res, void* ptr, std::size_t bytes, std::size_t alignment) {
    { res.allocate(bytes, alignment) } -> std::same_as<void*>;
    { res.deallocate(ptr, bytes, alignment) } -> std::same_as<void>;
    { res.is_equal(std::declval<const M&>()) } -> std::convertible_to<bool>;
} || std::derived_from<M, std::pmr::memory_resource>;

} // namespace ginnunga::concepts
