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
#include <thread>
#include <vector>
#include <atomic>
#include "ginnunga/concurrency/single_threaded.hpp"
#include "ginnunga/concurrency/epoch_reclamation.hpp"
#include "ginnunga/concurrency/hazard_pointers.hpp"
#include "ginnunga/concurrency/thread_local_cache_front.hpp"

TEST(Concurrency, SingleThreaded) {
    ginnunga::concurrency::SingleThreaded st;
    st.acquire_read();
    st.release_read();

    bool cleaned = false;
    st.retire([&cleaned]() { cleaned = true; });
    ASSERT_TRUE(cleaned);
    st.synchronize();
}

TEST(Concurrency, EpochReclamation) {
    ginnunga::concurrency::EpochReclamation ebr;

    ASSERT_EQ(ebr.current_epoch(), 0);
    std::atomic<bool> retired_called{false};

    // Thread 1 acquires read
    auto epoch1 = ebr.acquire_read();
    (void)epoch1;

    // Retire an action
    ebr.retire([&retired_called]() {
        retired_called.store(true);
    });

    // Retire empty lambda (no-op guard)
    ebr.retire({});

    // Retire >= 65 callbacks to test try_advance_epoch_locked threshold
    std::atomic<int> batch_reclaimed{0};
    for (int i = 0; i < 70; ++i) {
        ebr.retire([&batch_reclaimed]() {
            batch_reclaimed.fetch_add(1);
        });
    }

    ebr.release_read();
    ebr.synchronize();

    ASSERT_TRUE(retired_called.load());
    ASSERT_EQ(batch_reclaimed.load(), 70);
    ASSERT_TRUE(ebr.current_epoch() > 0);
}

TEST(Concurrency, HazardPointers) {
    ginnunga::concurrency::HazardPointers hp;

    int sample_node = 42;
    std::size_t slot = hp.acquire_read();
    hp.publish_hazard(slot, &sample_node);

    ASSERT_TRUE(hp.is_hazardous(&sample_node));
    ASSERT_FALSE(hp.is_hazardous(nullptr));

    // Test retiring callbacks and batch reclamation (triggering >= 32 callback threshold)
    std::atomic<int> retired_count{0};
    for (int i = 0; i < 35; ++i) {
        hp.retire([&retired_count]() {
            retired_count.fetch_add(1);
        });
    }

    hp.clear_hazard(slot);
    ASSERT_FALSE(hp.is_hazardous(&sample_node));

    hp.release_read();
    hp.synchronize();

    ASSERT_EQ(retired_count.load(), 35);
}

TEST(Concurrency, ThreadLocalCacheFront) {
    ginnunga::concurrency::ThreadLocalCacheFront<int, std::string> front;

    ASSERT_TRUE(front.local_lookup(10) == nullptr);

    // ConcurrencyConcept compliance tests
    ASSERT_EQ(front.acquire_read(), 0);
    front.release_read();
    std::atomic<bool> cb_run{false};
    front.retire([&cb_run]() { cb_run = true; });
    ASSERT_TRUE(cb_run.load());
    front.synchronize();

    // Insert and lookup hit
    front.local_insert(10, "Val10");
    std::string* pval = front.local_lookup(10);
    ASSERT_TRUE(pval != nullptr);
    ASSERT_EQ(*pval, "Val10");

    // Invalidate existing key
    front.local_invalidate(10);
    ASSERT_TRUE(front.local_lookup(10) == nullptr);

    // Invalidate entire local buffer
    front.local_insert(20, "Val20");
    front.local_insert(30, "Val30");
    ASSERT_TRUE(front.local_lookup(20) != nullptr);
    ASSERT_TRUE(front.local_lookup(30) != nullptr);
    front.invalidate_local();
    ASSERT_TRUE(front.local_lookup(20) == nullptr);
    ASSERT_TRUE(front.local_lookup(30) == nullptr);
}

TEST(Concurrency, MultiThreadedEpochReclamation) {
    ginnunga::concurrency::EpochReclamation ebr;
    constexpr int NUM_READERS = 4;
    constexpr int NUM_OPS = 200;
    std::atomic<bool> running{true};
    std::atomic<int> retired_count{0};

    std::vector<std::thread> readers;
    for (int t = 0; t < NUM_READERS; ++t) {
        readers.emplace_back([&ebr, &running]() {
            while (running.load()) {
                ebr.acquire_read();
                std::this_thread::yield();
                ebr.release_read();
            }
        });
    }

    // Writer thread retiring actions
    for (int i = 0; i < NUM_OPS; ++i) {
        ebr.retire([&retired_count]() {
            retired_count.fetch_add(1);
        });
    }

    // Stop readers
    running.store(false);
    for (auto& r : readers) {
        r.join();
    }

    // Drain all retired queues
    ebr.synchronize();
    ASSERT_EQ(retired_count.load(), NUM_OPS);
}

TEST(Concurrency, MultiThreadedHazardPointers) {
    ginnunga::concurrency::HazardPointers hp;
    constexpr int NUM_THREADS = 4;
    int protected_values[NUM_THREADS] = {10, 20, 30, 40};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&hp, &protected_values, t]() {
            std::size_t slot = hp.acquire_read();
            hp.publish_hazard(slot, &protected_values[t]);
            ASSERT_TRUE(hp.is_hazardous(&protected_values[t]));
            hp.clear_hazard(slot);
            hp.release_read();
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}
