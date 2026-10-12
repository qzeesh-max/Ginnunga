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

#include <cstddef>
#include <string>
#include <sstream>
#include <typeinfo>

namespace ginnunga::telemetry {

/**
 * @brief CacheLineVisualizer provides compile-time inspection and runtime ASCII
 * visualization of data structure cache-line alignment (64 bytes).
 * It verifies whether hot structures prevent false sharing and fit inside single or
 * exact multiples of CPU L1 cache lines.
 */
template <typename T>
class CacheLineVisualizer {
public:
    static constexpr std::size_t CACHE_LINE_SIZE = 64;
    static constexpr std::size_t TYPE_SIZE = sizeof(T);
    static constexpr std::size_t TYPE_ALIGN = alignof(T);

    static constexpr bool fits_in_single_line = (TYPE_SIZE <= CACHE_LINE_SIZE);
    static constexpr bool is_line_aligned = (TYPE_ALIGN >= CACHE_LINE_SIZE);
    static constexpr std::size_t lines_spanned = (TYPE_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
    static constexpr std::size_t internal_padding = (lines_spanned * CACHE_LINE_SIZE) - TYPE_SIZE;

    static std::string describe_layout(const std::string& type_name = "Structure") {
        std::ostringstream oss;
        oss << "========================================================\n";
        oss << "  CACHE LINE ALIGNMENT INSPECTOR: " << type_name << "\n";
        oss << "========================================================\n";
        oss << "  Size:            " << TYPE_SIZE << " bytes\n";
        oss << "  Alignment:       " << TYPE_ALIGN << " bytes\n";
        oss << "  Lines Spanned:   " << lines_spanned << " line(s) (64B each)\n";
        oss << "  Fits in 64B L1:  " << (fits_in_single_line ? "YES [HOT]" : "NO (Spans multiple)") << "\n";
        oss << "  Line-Aligned:    " << (is_line_aligned ? "YES [NO FALSE SHARING]" : "NO (Standard align)") << "\n";
        oss << "  Tail Padding:    " << internal_padding << " bytes to next cache line boundary\n";
        oss << "--------------------------------------------------------\n";
        oss << "  Memory Block Layout (64-byte blocks):\n";

        for (std::size_t i = 0; i < lines_spanned; ++i) {
            std::size_t start_byte = i * CACHE_LINE_SIZE;
            std::size_t end_byte = std::min(TYPE_SIZE, (i + 1) * CACHE_LINE_SIZE);
            std::size_t bytes_in_line = end_byte - start_byte;

            oss << "  [Line " << i << " : bytes " << start_byte << ".." << (end_byte - 1) << "] ";
            oss << "[";
            for (std::size_t b = 0; b < bytes_in_line / 4; ++b) oss << "#";
            for (std::size_t p = 0; p < (CACHE_LINE_SIZE - bytes_in_line) / 4; ++p) oss << ".";
            oss << "] (" << bytes_in_line << "B used, " << (CACHE_LINE_SIZE - bytes_in_line) << "B pad)\n";
        }
        oss << "========================================================\n";
        return oss.str();
    }
};

} // namespace ginnunga::telemetry
