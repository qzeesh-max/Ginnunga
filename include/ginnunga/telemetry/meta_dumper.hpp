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

#include <string>
#include <sstream>
#include <cstddef>
#include <iomanip>
#include <type_traits>

#if __has_include(<experimental/reflect>) || (defined(__cpp_reflection) && __cpp_reflection >= 202400L)
#include <experimental/reflect>
#define GINNUNGA_HAS_STD_META 1
#endif

namespace ginnunga::telemetry {

/**
 * @brief CacheStats holds runtime metrics for telemetry introspection.
 */
struct CacheStats {
    std::size_t hits{0};
    std::size_t misses{0};
    std::size_t inserts{0};
    std::size_t evictions{0};
    std::size_t erasures{0};

    [[nodiscard]] double hit_ratio() const noexcept {
        std::size_t total = hits + misses;
        return total == 0 ? 0.0 : static_cast<double>(hits) / static_cast<double>(total);
    }
};

/**
 * @brief MetaDumper generates zero-overhead JSON telemetry dumps
 * introspecting template parameters, memory footprint, cache line characteristics,
 * and operational throughput.
 */
class MetaDumper {
public:
    template <typename CacheType>
    static std::string dump_json(const CacheType& cache, const CacheStats& stats, const std::string& name = "GinnungaSparseCache") {
        std::ostringstream json;
        json << "{\n";
        json << "  \"cache_name\": \"" << name << "\",\n";
        json << "  \"metrics\": {\n";
        json << "    \"size\": " << cache.size() << ",\n";
        json << "    \"capacity\": " << cache.capacity() << ",\n";
        json << "    \"memory_footprint_bytes\": " << cache.memory_footprint() << ",\n";
        json << "    \"memory_footprint_kb\": " << std::fixed << std::setprecision(2)
             << (static_cast<double>(cache.memory_footprint()) / 1024.0) << ",\n";
        json << "    \"hits\": " << stats.hits << ",\n";
        json << "    \"misses\": " << stats.misses << ",\n";
        json << "    \"inserts\": " << stats.inserts << ",\n";
        json << "    \"evictions\": " << stats.evictions << ",\n";
        json << "    \"erasures\": " << stats.erasures << ",\n";
        json << "    \"hit_ratio\": " << std::fixed << std::setprecision(4) << stats.hit_ratio() << "\n";
        json << "  },\n";
        json << "  \"reflection\": {\n";
        json << "    \"key_size_bytes\": " << sizeof(typename CacheType::key_type) << ",\n";
        json << "    \"value_size_bytes\": " << sizeof(typename CacheType::value_type) << ",\n";
        json << "    \"key_align_bytes\": " << alignof(typename CacheType::key_type) << ",\n";
        json << "    \"value_align_bytes\": " << alignof(typename CacheType::value_type) << ",\n";
        json << "    \"cache_align_bytes\": " << alignof(CacheType) << ",\n";
        json << "    \"is_cache_line_aligned\": " << (alignof(CacheType) >= 64 ? "true" : "false") << "\n";
        json << "  }\n";
        json << "}";
        return json.str();
    }
};

} // namespace ginnunga::telemetry
