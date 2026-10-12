#include "test_framework.hpp"
#include <thread>
#include <chrono>
#include "ginnunga/eviction/lru_policy.hpp"
#include "ginnunga/eviction/lfu_policy.hpp"
#include "ginnunga/eviction/arc_policy.hpp"
#include "ginnunga/eviction/ttl_policy.hpp"

TEST(Eviction, LRUPolicy) {
    ginnunga::eviction::LRUPolicy<int> lru;

    lru.on_insert(1);
    lru.on_insert(2);
    lru.on_insert(3);
    ASSERT_EQ(lru.size(), 3);

    // Access 1 to make it most recently used
    lru.on_hit(1);

    // Evict should now evict 2 (least recently used)
    auto victim = lru.evict();
    ASSERT_TRUE(victim.has_value());
    ASSERT_EQ(*victim, 2);

    auto victim2 = lru.evict();
    ASSERT_TRUE(victim2.has_value());
    ASSERT_EQ(*victim2, 3);

    auto victim3 = lru.evict();
    ASSERT_TRUE(victim3.has_value());
    ASSERT_EQ(*victim3, 1);

    ASSERT_EQ(lru.size(), 0);
    ASSERT_FALSE(lru.evict().has_value());
}

TEST(Eviction, LFUPolicy) {
    ginnunga::eviction::LFUPolicy<int> lfu;

    lfu.on_insert(1);
    lfu.on_insert(2);
    lfu.on_insert(3);

    // Access 1 multiple times, 2 once
    lfu.on_hit(1);
    lfu.on_hit(1);
    lfu.on_hit(2);

    // 3 has frequency 1 (lowest), so 3 must be evicted first
    auto victim1 = lfu.evict();
    ASSERT_TRUE(victim1.has_value());
    ASSERT_EQ(*victim1, 3);

    // 2 has frequency 2, 1 has frequency 3. Next victim must be 2
    auto victim2 = lfu.evict();
    ASSERT_TRUE(victim2.has_value());
    ASSERT_EQ(*victim2, 2);

    auto victim3 = lfu.evict();
    ASSERT_TRUE(victim3.has_value());
    ASSERT_EQ(*victim3, 1);
}

TEST(Eviction, ARCPolicy) {
    ginnunga::eviction::ARCPolicy<int> arc(4);

    arc.on_insert(10);
    arc.on_insert(20);
    arc.on_insert(30);
    arc.on_insert(40);
    ASSERT_EQ(arc.size(), 4);

    // Access 10 and 20 to make them frequent (T2)
    arc.on_hit(10);
    arc.on_hit(20);

    // Evict: victim should come from T1 (recency list: 30 or 40)
    auto victim = arc.evict();
    ASSERT_TRUE(victim.has_value());
    ASSERT_TRUE(*victim == 30 || *victim == 40);

    arc.reset();
    ASSERT_EQ(arc.size(), 0);
}

TEST(Eviction, TTLPolicy) {
    ginnunga::eviction::TTLPolicy<int> ttl(std::chrono::milliseconds(50));

    ttl.on_insert(100);
    ASSERT_FALSE(ttl.is_expired(100));

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    ASSERT_TRUE(ttl.is_expired(100));

    auto victim = ttl.evict();
    ASSERT_TRUE(victim.has_value());
    ASSERT_EQ(*victim, 100);
}
