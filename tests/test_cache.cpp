#include "test_framework.hpp"
#include "ginnunga/cache.hpp"
#include "ginnunga/storage/simd_flat_map.hpp"
#include "ginnunga/storage/slot_array_map.hpp"
#include "ginnunga/storage/paged_sparse_array.hpp"
#include "ginnunga/storage/judy_tree_backend.hpp"
#include "ginnunga/eviction/lru_policy.hpp"
#include "ginnunga/eviction/lfu_policy.hpp"
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

    auto stats = cache.stats();
    ASSERT_EQ(stats.hits, 1);
    ASSERT_EQ(stats.evictions, 1);
    ASSERT_EQ(stats.inserts, 4);

    // Erase key 3
    ASSERT_TRUE(cache.erase(3));
    ASSERT_EQ(cache.size(), 2);
    ASSERT_FALSE(cache.contains(3));

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

    ConcurrentCache cache(100);

    for (uint64_t i = 1; i <= 20; ++i) {
        ASSERT_TRUE(cache.put(i * 10, static_cast<int>(i * 100)));
    }

    ASSERT_EQ(cache.size(), 20);

    for (uint64_t i = 1; i <= 20; ++i) {
        auto val = cache.get(i * 10);
        ASSERT_TRUE(val.has_value());
        ASSERT_EQ(*val, static_cast<int>(i * 100));
    }

    std::string json = cache.dump_telemetry_json("TestSimdCache");
    ASSERT_TRUE(!json.empty());
    ASSERT_TRUE(json.find("\"cache_name\": \"TestSimdCache\"") != std::string::npos);
}
