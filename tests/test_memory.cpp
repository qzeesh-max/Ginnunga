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
#include <vector>
#include <string>
#include "ginnunga/memory/numa_arena_resource.hpp"
#include "ginnunga/memory/huge_page_allocator.hpp"

TEST(Memory, NumaArenaResourceSmallAndLargeAllocations) {
    ginnunga::memory::NumaArenaResource arena(0, 1024 * 64); // 64KB initial chunk
    ASSERT_EQ(arena.numa_node(), 0);
    ASSERT_TRUE(arena.total_capacity() >= 64 * 1024);

    // 1. Small allocation
    void* p1 = arena.allocate(128, 64);
    ASSERT_TRUE(p1 != nullptr);
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(p1) % 64, 0); // 64-byte aligned
    ASSERT_TRUE(arena.total_allocated() >= 128);

    // 2. Large allocation that exceeds initial chunk
    std::size_t large_size = 1024 * 128; // 128KB > 64KB chunk
    void* p2 = arena.allocate(large_size, 64);
    ASSERT_TRUE(p2 != nullptr);
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(p2) % 64, 0);
    ASSERT_TRUE(arena.total_capacity() > 64 * 1024);

    // 3. Compatibility with std::pmr containers
    ginnunga::memory::NumaArenaResource vector_arena(0, 1024 * 32);
    std::pmr::vector<int> pmr_vec(&vector_arena);
    for (int i = 0; i < 1000; ++i) {
        pmr_vec.push_back(i * 10);
    }
    ASSERT_EQ(pmr_vec.size(), 1000);
    ASSERT_EQ(pmr_vec[999], 9990);

    // 4. Equality checks
    ASSERT_TRUE(arena.is_equal(arena));
    ASSERT_FALSE(arena.is_equal(vector_arena));
}

TEST(Memory, HugePageResourceAndAllocator) {
    ginnunga::memory::HugePageResource hp_res;

    // Allocate 4096 bytes (should round up to huge page size)
    void* ptr = hp_res.allocate(4096, 64);
    ASSERT_TRUE(ptr != nullptr);

    // Verify write and read
    int* int_ptr = static_cast<int*>(ptr);
    int_ptr[0] = 12345;
    int_ptr[100] = 67890;
    ASSERT_EQ(int_ptr[0], 12345);
    ASSERT_EQ(int_ptr[100], 67890);

    hp_res.deallocate(ptr, 4096, 64);

    // Equality checks on HugePageResource
    ginnunga::memory::HugePageResource hp_res2;
    ASSERT_TRUE(hp_res.is_equal(hp_res));
    ASSERT_FALSE(hp_res.is_equal(hp_res2));

    // Test HugePageAllocator with STL vector
    ginnunga::memory::HugePageAllocator<double> alloc;
    double* dbl_arr = alloc.allocate(100);
    ASSERT_TRUE(dbl_arr != nullptr);
    dbl_arr[0] = 3.14159;
    dbl_arr[99] = 2.71828;
    ASSERT_EQ(dbl_arr[0], 3.14159);
    ASSERT_EQ(dbl_arr[99], 2.71828);
    alloc.deallocate(dbl_arr, 100);

    // Copy and comparison operators
    ginnunga::memory::HugePageAllocator<int> alloc_copy;
    ASSERT_TRUE(alloc == alloc);
    ASSERT_FALSE(alloc != alloc);
    (void)alloc_copy;
}
