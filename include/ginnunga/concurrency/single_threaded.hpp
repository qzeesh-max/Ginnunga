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

#include <functional>
#include "ginnunga/concepts/concurrency.hpp"

namespace ginnunga::concurrency {

/**
 * @brief SingleThreaded concurrency policy provides zero runtime overhead,
 * no atomics, and immediate retirement for single-threaded usage.
 */
class SingleThreaded {
public:
    int acquire_read() noexcept { return 0; }
    void release_read() noexcept {}

    void retire(std::function<void()> cleanup) {
        if (cleanup) cleanup();
    }

    void synchronize() noexcept {}
};

static_assert(concepts::ConcurrencyConcept<SingleThreaded>,
              "SingleThreaded must satisfy ConcurrencyConcept");

} // namespace ginnunga::concurrency
