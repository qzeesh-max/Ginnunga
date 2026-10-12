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

#include "test_framework.hpp"
#include <string>
#include "ginnunga/telemetry/cache_line_visualizer.hpp"
#include "ginnunga/telemetry/meta_dumper.hpp"
#include "ginnunga/cache.hpp"
#include "ginnunga/storage/simd_flat_map.hpp"

struct alignas(64) HotStruct {
    char data[64];
};

struct alignas(8) SpanStruct {
    char data[120];
};

TEST(Telemetry, CacheLineVisualizerCalculations) {
    // 64-byte aligned struct
    using HotVis = ginnunga::telemetry::CacheLineVisualizer<HotStruct>;
    ASSERT_TRUE(HotVis::fits_in_single_line);
    ASSERT_TRUE(HotVis::is_line_aligned);
    ASSERT_EQ(HotVis::lines_spanned, 1);
    ASSERT_EQ(HotVis::internal_padding, 0);

    std::string hot_diagram = HotVis::describe_layout("HotStruct");
    ASSERT_TRUE(!hot_diagram.empty());
    ASSERT_TRUE(hot_diagram.find("CACHE LINE ALIGNMENT INSPECTOR") != std::string::npos);
    ASSERT_TRUE(hot_diagram.find("Fits in 64B L1:  YES") != std::string::npos);

    // Multi-line spanning struct
    using SpanVis = ginnunga::telemetry::CacheLineVisualizer<SpanStruct>;
    ASSERT_FALSE(SpanVis::fits_in_single_line);
    ASSERT_FALSE(SpanVis::is_line_aligned);
    ASSERT_EQ(SpanVis::lines_spanned, 2);
    ASSERT_EQ(SpanVis::internal_padding, 8); // 128 - 120 = 8

    std::string span_diagram = SpanVis::describe_layout("SpanStruct");
    ASSERT_TRUE(span_diagram.find("Spans multiple") != std::string::npos);
}

TEST(Telemetry, MetaDumperMetricsAndJson) {
    ginnunga::telemetry::CacheStats stats;
    ASSERT_EQ(stats.hit_ratio(), 0.0);

    stats.hits = 75;
    stats.misses = 25;
    stats.inserts = 100;
    stats.evictions = 10;
    stats.erasures = 5;
    ASSERT_EQ(stats.hit_ratio(), 0.75);

    using MockCache = ginnunga::SparseCache<uint32_t, int>;
    MockCache cache(50);
    cache.put(1, 10);
    cache.put(2, 20);

    std::string json = ginnunga::telemetry::MetaDumper::dump_json(cache, stats, "TelemetryUnitTestCache");
    ASSERT_TRUE(json.find("\"cache_name\": \"TelemetryUnitTestCache\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"hits\": 75") != std::string::npos);
    ASSERT_TRUE(json.find("\"misses\": 25") != std::string::npos);
    ASSERT_TRUE(json.find("\"hit_ratio\": 0.7500") != std::string::npos);
}
