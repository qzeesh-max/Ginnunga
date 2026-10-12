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
#include "ginnunga/eviction/lru_policy.hpp"
#include "ginnunga/eviction/lfu_policy.hpp"
#include "ginnunga/eviction/arc_policy.hpp"
#include "ginnunga/eviction/ttl_policy.hpp"

TEST(Eviction, LRUPolicy) {
    ginnunga::eviction::LRUPolicy<int> lru;

    ASSERT_FALSE(lru.evict().has_value()); // Empty eviction

    lru.on_insert(1);
    lru.on_insert(2);
    lru.on_insert(3);
    ASSERT_EQ(lru.size(), 3);

    // Re-inserting existing key moves it to front
    lru.on_insert(1);

    // Access 2 to make it most recently used
    lru.on_hit(2);

    // Erase non-existent
    lru.on_erase(999);

    // Evict should now evict 3 (least recently used)
    auto victim = lru.evict();
    ASSERT_TRUE(victim.has_value());
    ASSERT_EQ(*victim, 3);

    auto victim2 = lru.evict();
    ASSERT_TRUE(victim2.has_value());
    ASSERT_EQ(*victim2, 1);

    auto victim3 = lru.evict();
    ASSERT_TRUE(victim3.has_value());
    ASSERT_EQ(*victim3, 2);

    ASSERT_EQ(lru.size(), 0);
    ASSERT_FALSE(lru.evict().has_value());

    lru.on_insert(4);
    lru.reset();
    ASSERT_EQ(lru.size(), 0);
}

TEST(Eviction, LFUPolicy) {
    ginnunga::eviction::LFUPolicy<int> lfu;

    ASSERT_FALSE(lfu.evict().has_value()); // Empty eviction

    lfu.on_insert(1);
    lfu.on_insert(2);
    lfu.on_insert(3);

    // Re-inserting existing key 1 increments its frequency (on_hit)
    lfu.on_insert(1);

    // Access 1 again, and 2 once
    lfu.on_hit(1);
    lfu.on_hit(2);

    // Erase non-existent
    lfu.on_erase(999);

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

    ASSERT_EQ(lfu.size(), 0);
    ASSERT_FALSE(lfu.evict().has_value());

    lfu.on_insert(4);
    lfu.reset();
    ASSERT_EQ(lfu.size(), 0);
}

TEST(Eviction, ARCPolicy) {
    ginnunga::eviction::ARCPolicy<int> arc(4);

    ASSERT_FALSE(arc.evict().has_value()); // Empty eviction

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
    ASSERT_EQ(ttl.ttl(), std::chrono::milliseconds(50));
    ASSERT_TRUE(ttl.empty());
    ASSERT_FALSE(ttl.evict().has_value()); // Empty eviction
    ASSERT_TRUE(ttl.is_expired(999)); // Non-existent key is treated as expired

    ttl.on_insert(100);
    ASSERT_FALSE(ttl.empty());
    ASSERT_EQ(ttl.size(), 1);
    ASSERT_FALSE(ttl.is_expired(100));

    // Re-insert existing key (updates timestamp)
    ttl.on_insert(100);

    // Explicit erase of existing key
    ttl.on_insert(200);
    ttl.on_erase(200);
    ASSERT_EQ(ttl.size(), 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    ASSERT_TRUE(ttl.is_expired(100));

    auto victim = ttl.evict();
    ASSERT_TRUE(victim.has_value());
    ASSERT_EQ(*victim, 100);
    ASSERT_TRUE(ttl.empty());

    ttl.on_insert(300);
    ttl.reset();
    ASSERT_TRUE(ttl.empty());
}

TEST(Eviction, ARCGhostCacheAdaptation) {
    ginnunga::eviction::ARCPolicy<int> arc(2); // capacity 2
    ASSERT_EQ(arc.target_p(), 0);

    arc.on_insert(1);
    arc.on_insert(2);
    ASSERT_EQ(arc.size(), 2);

    // Evict 1 -> moves 1 to ghost cache B1
    auto v1 = arc.evict();
    ASSERT_TRUE(v1.has_value());
    ASSERT_EQ(*v1, 1);
    ASSERT_EQ(arc.size(), 1);

    // Re-inserting 1 should register as a ghost hit in B1 and increase target_p
    arc.on_insert(1);
    ASSERT_TRUE(arc.target_p() > 0);

    // Move elements to T2 by hitting them
    arc.on_hit(1);
    arc.on_hit(2);

    // Now T1 is empty, all elements are in T2. Eviction must now evict from T2 and populate B2!
    auto v_t2 = arc.evict();
    ASSERT_TRUE(v_t2.has_value());

    // Re-inserting that victim registers a ghost hit in B2 and decreases target_p
    arc.on_insert(*v_t2);

    // Test explicit erasure across T1, T2, B1, and B2
    ginnunga::eviction::ARCPolicy<int> arc_erase(2);
    // 1. Erase from T1
    arc_erase.on_insert(10);
    arc_erase.on_erase(10);
    ASSERT_EQ(arc_erase.size(), 0);

    // 2. Erase from B1
    arc_erase.on_insert(20);
    arc_erase.evict(); // moves 20 to B1
    arc_erase.on_erase(20);

    // 3. Erase from B2
    arc_erase.on_insert(30);
    arc_erase.on_hit(30); // moves to T2
    arc_erase.evict(); // moves 30 to B2
    arc_erase.on_erase(30);

    arc_erase.on_erase(999); // non-existent

    // Test B1 ghost trimming by inserting and evicting multiple keys past capacity c=2
    ginnunga::eviction::ARCPolicy<int> arc_trim_b1(2);
    for (int k = 1; k <= 5; ++k) {
        arc_trim_b1.on_insert(k);
        arc_trim_b1.evict();
    }

    // Test B2 ghost trimming
    ginnunga::eviction::ARCPolicy<int> arc_trim_b2(2);
    for (int k = 1; k <= 5; ++k) {
        arc_trim_b2.on_insert(k);
        arc_trim_b2.on_hit(k); // move to T2
        arc_trim_b2.evict();   // move to B2
    }
}

TEST(Eviction, TTLNonSlidingAndMultipleKeys) {
    // Non-sliding TTL: hits do not reset the expiration timestamp
    ginnunga::eviction::TTLPolicy<int> ttl(std::chrono::milliseconds(60), /*sliding_ttl=*/false);

    ttl.on_insert(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ttl.on_insert(2);

    // Hit key 1 - should NOT slide
    ttl.on_hit(1);

    std::this_thread::sleep_for(std::chrono::milliseconds(45));
    // 1 was inserted 65ms ago -> expired
    ASSERT_TRUE(ttl.is_expired(1));
    // 2 was inserted 45ms ago -> still fresh
    ASSERT_FALSE(ttl.is_expired(2));

    // Earliest expiring must be evicted first
    auto victim = ttl.evict();
    ASSERT_TRUE(victim.has_value());
    ASSERT_EQ(*victim, 1);
}

TEST(Eviction, LFUTieBreakingAndMinFreqRecalc) {
    ginnunga::eviction::LFUPolicy<int> lfu;

    lfu.on_insert(10);
    lfu.on_insert(20);

    // Hit both to frequency 2
    lfu.on_hit(10);
    lfu.on_hit(20);

    // Erase 10 and 20 to force min_freq recalculation from higher bucket
    lfu.on_insert(30);
    lfu.on_hit(30);
    lfu.on_hit(30); // freq 3

    lfu.on_erase(10);
    lfu.on_erase(20);

    // Remaining element 30 should be evictable
    auto v = lfu.evict();
    ASSERT_TRUE(v.has_value());
    ASSERT_EQ(*v, 30);
    ASSERT_EQ(lfu.size(), 0);
}
