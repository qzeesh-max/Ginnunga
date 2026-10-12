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
#include <new>
#include <sys/mman.h>
#include <unistd.h>
#include <memory_resource>

#if defined(__APPLE__)
#include <mach/vm_statistics.h>
#endif

namespace ginnunga::memory {

/**
 * @brief HugePageAllocator manages 2MB (or platform huge page size) virtual memory allocations
 * using direct OS mmap to eliminate TLB misses when dealing with massive sparse address spaces.
 */
class HugePageResource : public std::pmr::memory_resource {
public:
    static constexpr std::size_t HUGE_PAGE_SIZE = 2 * 1024 * 1024; // 2 MB

    HugePageResource() = default;
    ~HugePageResource() override = default;

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        // Round up to nearest huge page size multiple
        std::size_t rounded_size = (bytes + HUGE_PAGE_SIZE - 1) & ~(HUGE_PAGE_SIZE - 1);
        if (rounded_size == 0) rounded_size = HUGE_PAGE_SIZE;

        void* ptr = nullptr;

#if defined(__linux__) && defined(MAP_HUGETLB)
        ptr = mmap(nullptr, rounded_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (ptr == MAP_FAILED) {
            // Fall back to standard anonymous mmap if huge pages are not reserved in kernel
            ptr = mmap(nullptr, rounded_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
#elif defined(__APPLE__) && defined(VM_FLAGS_SUPERPAGE_SIZE_2MB)
        // macOS superpages allocation attempt
        ptr = mmap(nullptr, rounded_size, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_PRIVATE, VM_FLAGS_SUPERPAGE_SIZE_2MB, 0);
        if (ptr == MAP_FAILED) {
            ptr = mmap(nullptr, rounded_size, PROT_READ | PROT_WRITE,
                       MAP_ANON | MAP_PRIVATE, -1, 0);
        }
#else
        ptr = mmap(nullptr, rounded_size, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_PRIVATE, -1, 0);
#endif

        if (ptr == MAP_FAILED || ptr == nullptr) {
            throw std::bad_alloc();
        }

#if defined(MADV_WILLNEED)
        // Advise kernel that these pages will be accessed
        madvise(ptr, rounded_size, MADV_WILLNEED);
#endif

        (void)alignment;
        return ptr;
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t /*alignment*/) override {
        if (!p) return;
        std::size_t rounded_size = (bytes + HUGE_PAGE_SIZE - 1) & ~(HUGE_PAGE_SIZE - 1);
        if (rounded_size == 0) rounded_size = HUGE_PAGE_SIZE;
        munmap(p, rounded_size);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

/**
 * @brief STL-compatible allocator wrapping HugePageResource
 */
template <typename T>
class HugePageAllocator {
public:
    using value_type = T;

    HugePageAllocator() noexcept = default;
    template <typename U>
    HugePageAllocator(const HugePageAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)) throw std::bad_alloc();
        return static_cast<T*>(resource_.allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept {
        resource_.deallocate(p, n * sizeof(T), alignof(T));
    }

    template <typename U>
    bool operator==(const HugePageAllocator<U>&) const noexcept { return true; }
    template <typename U>
    bool operator!=(const HugePageAllocator<U>&) const noexcept { return false; }

private:
    static inline HugePageResource resource_{};
};

} // namespace ginnunga::memory
