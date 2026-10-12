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

#include "test_framework.hpp"
#include <thread>
#include <chrono>
#include "ginnunga/cache.hpp"
#include "ginnunga/storage/simd_flat_map.hpp"
#include "ginnunga/storage/slot_array_map.hpp"
#include "ginnunga/storage/paged_sparse_array.hpp"
#include "ginnunga/storage/judy_tree_backend.hpp"
#include "ginnunga/eviction/lru_policy.hpp"
#include "ginnunga/eviction/lfu_policy.hpp"
#include "ginnunga/eviction/arc_policy.hpp"
#include "ginnunga/eviction/ttl_policy.hpp"
#include "ginnunga/concurrency/single_threaded.hpp"
#include "ginnunga/concurrency/epoch_reclamation.hpp"

TEST(Cache, FullLRUCacheLifecycle) {
    using Cache = ginnunga::SparseCache<
        uint32_t,
        std::string,
        ginnunga::storage::SlotArrayMap<uint32_t, std::string>,
        ginnunga::eviction::LRUPolicy<uint32_t>,
        ginnunga::concurrency::SingleThreaded
    >;

    Cache cache(3); // Small capacity of 3 to force eviction

    ASSERT_TRUE(cache.put(1, "one"));
    ASSERT_TRUE(cache.put(2, "two"));
    ASSERT_TRUE(cache.put(3, "three"));
    ASSERT_EQ(cache.size(), 3);

    // Hit key 1 to promote it
    ASSERT_EQ(cache.get(1).value(), "one");

    // Insert 4th element -> capacity 3 reached -> should evict key 2 (least recently used)
    ASSERT_TRUE(cache.put(4, "four"));
    ASSERT_EQ(cache.size(), 3);

    ASSERT_TRUE(cache.contains(1));
    ASSERT_FALSE(cache.contains(2)); // Evicted!
    ASSERT_TRUE(cache.contains(3));
    ASSERT_TRUE(cache.contains(4));

    ASSERT_EQ(cache.capacity(), 3);
    ASSERT_TRUE(cache.memory_footprint() > 0);

    auto stats = cache.stats();
    ASSERT_EQ(stats.hits, 1);
    ASSERT_EQ(stats.evictions, 1);
    ASSERT_EQ(stats.inserts, 4);

    // Erase key 3
    ASSERT_TRUE(cache.erase(3));
    ASSERT_EQ(cache.size(), 2);
    ASSERT_FALSE(cache.contains(3));

    // Erase non-existent key returns false
    ASSERT_FALSE(cache.erase(999));

    // Accessors
    ASSERT_EQ(cache.storage().size(), 2);
    ASSERT_EQ(cache.eviction().size(), 2);
    const auto& const_cache = cache;
    ASSERT_EQ(const_cache.storage().size(), 2);
    ASSERT_EQ(const_cache.eviction().size(), 2);
    (void)const_cache.concurrency();

    cache.clear();
    ASSERT_EQ(cache.size(), 0);
}

TEST(Cache, SimdFlatMapWithEBR) {
    using ConcurrentCache = ginnunga::SparseCache<
        uint64_t,
        int,
        ginnunga::storage::SimdFlatMap<uint64_t, int>,
        ginnunga::eviction::LFUPolicy<uint64_t>,
        ginnunga::concurrency::EpochReclamation
    >;

    ConcurrentCache cache(10); // Small capacity of 10 to force evictions with EBR

    for (uint64_t i = 1; i <= 20; ++i) {
        ASSERT_TRUE(cache.put(i * 10, static_cast<int>(i * 100)));
    }

    ASSERT_EQ(cache.size(), 10); // Bound by capacity

    // Non-existent key lookup
    ASSERT_FALSE(cache.get(99999).has_value());
    ASSERT_FALSE(cache.contains(99999));

    // Drain EBR retired callbacks from evictions
    cache.concurrency().synchronize();

    std::string json = cache.dump_telemetry_json("TestSimdCache");
    ASSERT_TRUE(!json.empty());
    ASSERT_TRUE(json.find("\"cache_name\": \"TestSimdCache\"") != std::string::npos);
}

TEST(Cache, PagedSparseArrayWithTTL) {
    using PagedCache = ginnunga::SparseCache<
        uint32_t,
        int,
        ginnunga::storage::PagedSparseArray<uint32_t, int>,
        ginnunga::eviction::TTLPolicy<uint32_t>,
        ginnunga::concurrency::SingleThreaded
    >;

    PagedCache cache(5);
    cache.eviction().set_ttl(std::chrono::milliseconds(50));

    cache.put(100, 1000);
    cache.put(200, 2000);
    ASSERT_EQ(cache.size(), 2);
    ASSERT_EQ(cache.get(100).value(), 1000);

    // Sleep past TTL
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    ASSERT_TRUE(cache.eviction().is_expired(100));
}

TEST(Cache, JudyTreeWithLFU) {
    using JudyCache = ginnunga::SparseCache<
        uint32_t,
        int,
        ginnunga::storage::JudyTreeBackend<uint32_t, int>,
        ginnunga::eviction::LFUPolicy<uint32_t>,
        ginnunga::concurrency::SingleThreaded
    >;

    JudyCache cache(3); // Capacity 3
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    // Hit key 1 twice, key 2 once -> key 3 has lowest freq (1)
    (void)cache.get(1);
    (void)cache.get(1);
    (void)cache.get(2);

    // Put 4th item -> must evict key 3
    cache.put(4, 40);
    ASSERT_EQ(cache.size(), 3);
    ASSERT_TRUE(cache.contains(1));
    ASSERT_TRUE(cache.contains(2));
    ASSERT_FALSE(cache.contains(3)); // Evicted by LFU
    ASSERT_TRUE(cache.contains(4));
}

TEST(Cache, MultiThreadedConcurrentAccess) {
    using ConcurrentCache = ginnunga::SparseCache<
        uint64_t,
        int,
        ginnunga::storage::SimdFlatMap<uint64_t, int>,
        ginnunga::eviction::ARCPolicy<uint64_t>,
        ginnunga::concurrency::EpochReclamation
    >;

    ConcurrentCache cache(500);

    // Seed cache
    for (uint64_t i = 1; i <= 50; ++i) {
        cache.put(i, static_cast<int>(i * 10));
    }

    constexpr int NUM_THREADS = 4;
    std::vector<std::thread> workers;

    for (int t = 0; t < NUM_THREADS; ++t) {
        workers.emplace_back([&cache, t]() {
            for (int op = 0; op < 50; ++op) {
                uint64_t key = (op + t) % 50 + 1;
                auto val = cache.get(key);
                if (val.has_value()) {
                    ASSERT_EQ(*val, static_cast<int>(key * 10));
                }
                // Also perform occasional concurrent put
                if (op % 5 == 0) {
                    cache.put(100 + t * 100 + op, op);
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    ASSERT_TRUE(cache.size() >= 50);
}
