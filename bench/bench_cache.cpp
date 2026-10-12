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

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <unordered_map>

#include "ginnunga/cache.hpp"
#include "ginnunga/storage/paged_sparse_array.hpp"
#include "ginnunga/storage/slot_array_map.hpp"
#include "ginnunga/storage/simd_flat_map.hpp"
#include "ginnunga/storage/judy_tree_backend.hpp"
#include "ginnunga/eviction/lru_policy.hpp"
#include "ginnunga/concurrency/single_threaded.hpp"

template <typename Func>
double measure_time_sec(Func&& f) {
    auto start = std::chrono::high_resolution_clock::now();
    f();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "  Project Ginnunga - Sparse Cache Microbenchmarks\n";
    std::cout << "========================================================================\n";

    constexpr std::size_t NUM_OPS = 500000;
    std::cout << "[*] Operations per benchmark run: " << NUM_OPS << "\n";

    // Generate non-sequential, pseudo-random sparse keys
    std::mt19937_64 rng(1337);
    std::uniform_int_distribution<uint64_t> dist(1, 10000000ULL);

    std::vector<uint64_t> keys;
    keys.reserve(NUM_OPS);
    for (std::size_t i = 0; i < NUM_OPS; ++i) {
        keys.push_back(dist(rng));
    }

    // 1. SimdFlatMap
    {
        ginnunga::storage::SimdFlatMap<uint64_t, uint64_t> simd_map(NUM_OPS);
        double insert_sec = measure_time_sec([&]() {
            for (std::size_t i = 0; i < NUM_OPS; ++i) {
                simd_map.insert(keys[i], keys[i] * 2);
            }
        });
        double lookup_sec = measure_time_sec([&]() {
            volatile uint64_t sink = 0;
            for (std::size_t i = 0; i < NUM_OPS; ++i) {
                if (auto* v = simd_map.find(keys[i])) sink += *v;
            }
            (void)sink;
        });

        std::cout << "  [SimdFlatMap (NEON/AVX2)]:\n";
        std::cout << "    Insert: " << std::fixed << std::setprecision(2)
                  << (NUM_OPS / insert_sec / 1e6) << " M ops/sec (" << (insert_sec * 1000.0) << " ms)\n";
        std::cout << "    Lookup: " << std::fixed << std::setprecision(2)
                  << (NUM_OPS / lookup_sec / 1e6) << " M ops/sec (" << (lookup_sec * 1000.0) << " ms)\n";
        std::cout << "    Memory: " << simd_map.memory_footprint() / 1024 << " KB\n\n";
    }

    // 2. SlotArrayMap (Sparse Set)
    {
        constexpr std::size_t DENSE_SUBSET = 100000;
        ginnunga::storage::SlotArrayMap<uint32_t, uint32_t> slot_map(500000);
        double insert_sec = measure_time_sec([&]() {
            for (std::size_t i = 0; i < DENSE_SUBSET; ++i) {
                slot_map.insert(static_cast<uint32_t>(keys[i] % 500000), static_cast<uint32_t>(i));
            }
        });
        double lookup_sec = measure_time_sec([&]() {
            volatile uint32_t sink = 0;
            for (std::size_t i = 0; i < DENSE_SUBSET; ++i) {
                if (auto* v = slot_map.find(static_cast<uint32_t>(keys[i] % 500000))) sink += *v;
            }
            (void)sink;
        });

        std::cout << "  [SlotArrayMap (Sparse Set)]:\n";
        std::cout << "    Insert: " << std::fixed << std::setprecision(2)
                  << (DENSE_SUBSET / insert_sec / 1e6) << " M ops/sec (" << (insert_sec * 1000.0) << " ms)\n";
        std::cout << "    Lookup: " << std::fixed << std::setprecision(2)
                  << (DENSE_SUBSET / lookup_sec / 1e6) << " M ops/sec (" << (lookup_sec * 1000.0) << " ms)\n";
        std::cout << "    Memory: " << slot_map.memory_footprint() / 1024 << " KB\n\n";
    }

    // 3. PagedSparseArray (Virtual Memory Paging)
    {
        constexpr std::size_t PAGED_OPS = 100000;
        ginnunga::storage::PagedSparseArray<uint32_t, uint32_t> paged_arr(1000000);
        double insert_sec = measure_time_sec([&]() {
            for (std::size_t i = 0; i < PAGED_OPS; ++i) {
                paged_arr.insert(static_cast<uint32_t>(keys[i] % 1000000), static_cast<uint32_t>(i));
            }
        });
        double lookup_sec = measure_time_sec([&]() {
            volatile uint32_t sink = 0;
            for (std::size_t i = 0; i < PAGED_OPS; ++i) {
                if (auto* v = paged_arr.find(static_cast<uint32_t>(keys[i] % 1000000))) sink += *v;
            }
            (void)sink;
        });

        std::cout << "  [PagedSparseArray (Virtual Memory mmap/madvise)]:\n";
        std::cout << "    Insert: " << std::fixed << std::setprecision(2)
                  << (PAGED_OPS / insert_sec / 1e6) << " M ops/sec (" << (insert_sec * 1000.0) << " ms)\n";
        std::cout << "    Lookup: " << std::fixed << std::setprecision(2)
                  << (PAGED_OPS / lookup_sec / 1e6) << " M ops/sec (" << (lookup_sec * 1000.0) << " ms)\n";
        std::cout << "    Memory: " << paged_arr.memory_footprint() / 1024 << " KB committed\n\n";
    }

    // 4. std::unordered_map Baseline
    {
        std::unordered_map<uint64_t, uint64_t> std_map;
        std_map.reserve(NUM_OPS);
        double insert_sec = measure_time_sec([&]() {
            for (std::size_t i = 0; i < NUM_OPS; ++i) {
                std_map[keys[i]] = keys[i] * 2;
            }
        });
        double lookup_sec = measure_time_sec([&]() {
            volatile uint64_t sink = 0;
            for (std::size_t i = 0; i < NUM_OPS; ++i) {
                auto it = std_map.find(keys[i]);
                if (it != std_map.end()) sink += it->second;
            }
            (void)sink;
        });

        std::cout << "  [std::unordered_map Baseline (Pointer-Chasing Buckets)]:\n";
        std::cout << "    Insert: " << std::fixed << std::setprecision(2)
                  << (NUM_OPS / insert_sec / 1e6) << " M ops/sec (" << (insert_sec * 1000.0) << " ms)\n";
        std::cout << "    Lookup: " << std::fixed << std::setprecision(2)
                  << (NUM_OPS / lookup_sec / 1e6) << " M ops/sec (" << (lookup_sec * 1000.0) << " ms)\n";
        std::cout << "    Memory: " << (std_map.size() * sizeof(std::pair<uint64_t, uint64_t>) * 3) / 1024 << " KB (approx node overhead)\n";
    }

    std::cout << "========================================================================\n";
    return 0;
}
