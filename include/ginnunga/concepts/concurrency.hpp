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
#include <functional>

namespace ginnunga::concepts {

/**
 * @brief ConcurrencyConcept defines the thread synchronization and reclamation contracts.
 * 
 * Supports both single-threaded zero-overhead models and lock-free/wait-free
 * multi-threaded models (e.g., Epoch-Based Reclamation, Hazard Pointers).
 */
template <typename C>
concept ConcurrencyConcept = requires(C& concurrency) {
    // Acquire read registration (e.g., entering epoch or pin hazard pointer)
    { concurrency.acquire_read() };

    // Release read registration
    { concurrency.release_read() } -> std::same_as<void>;

    // Retire an object for deferred destruction once no active readers can observe it
    { concurrency.retire(std::declval<std::function<void()>>()) } -> std::same_as<void>;

    // Ensure all prior retired actions have been completed
    { concurrency.synchronize() } -> std::same_as<void>;
};

} // namespace ginnunga::concepts
