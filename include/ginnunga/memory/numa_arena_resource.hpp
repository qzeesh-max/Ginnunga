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

#include <memory_resource>
#include <memory>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>
#include <new>
#include <cstdlib>

#if defined(__linux__) && defined(GINNUNGA_HAS_LIBNUMA)
#include <numa.h>
#include <numaif.h>
#endif

namespace ginnunga::memory {

/**
 * @brief NumaArenaResource provides a std::pmr::memory_resource implementation
 * that pins allocations to a specific NUMA domain (on Linux) or provides
 * cache-line aligned arena allocation (on macOS/UMA systems).
 */
class NumaArenaResource : public std::pmr::memory_resource {
public:
    explicit NumaArenaResource(int numa_node = 0, std::size_t initial_chunk_size = 1024 * 1024)
        : numa_node_(numa_node), default_chunk_size_(initial_chunk_size) {
        allocate_new_chunk(default_chunk_size_);
    }

    ~NumaArenaResource() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& chunk : chunks_) {
            deallocate_raw(chunk.ptr, chunk.size);
        }
        chunks_.clear();
    }

    NumaArenaResource(const NumaArenaResource&) = delete;
    NumaArenaResource& operator=(const NumaArenaResource&) = delete;

    [[nodiscard]] int numa_node() const noexcept { return numa_node_; }

    [[nodiscard]] std::size_t total_allocated() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_allocated_;
    }

    [[nodiscard]] std::size_t total_capacity() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_capacity_;
    }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        std::lock_guard<std::mutex> lock(mutex_);
        alignment = std::max(alignment, alignof(std::max_align_t));

        if (!chunks_.empty()) {
            auto& current = chunks_.back();
            std::size_t space = current.size - current.offset;
            void* current_ptr = static_cast<char*>(current.ptr) + current.offset;
            void* aligned_ptr = current_ptr;

            if (std::align(alignment, bytes, aligned_ptr, space)) {
                current.offset = (static_cast<char*>(aligned_ptr) - static_cast<char*>(current.ptr)) + bytes;
                total_allocated_ += bytes;
                return aligned_ptr;
            }
        }

        // Allocate a new chunk that is at least big enough for the requested bytes + alignment
        std::size_t new_chunk_size = std::max(default_chunk_size_, bytes + alignment * 2);
        allocate_new_chunk(new_chunk_size);

        auto& new_chunk = chunks_.back();
        std::size_t space = new_chunk.size;
        void* aligned_ptr = new_chunk.ptr;
        if (std::align(alignment, bytes, aligned_ptr, space)) {
            new_chunk.offset = (static_cast<char*>(aligned_ptr) - static_cast<char*>(new_chunk.ptr)) + bytes;
            total_allocated_ += bytes;
            return aligned_ptr;
        }

        throw std::bad_alloc();
    }

    void do_deallocate(void* /*p*/, std::size_t /*bytes*/, std::size_t /*alignment*/) override {
        // Arena allocator: individual deallocations are no-ops;
        // memory is reclaimed on arena destruction or explicit reset.
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    struct Chunk {
        void* ptr{nullptr};
        std::size_t size{0};
        std::size_t offset{0};
    };

    void* allocate_raw(std::size_t size) {
#if defined(__linux__) && defined(GINNUNGA_HAS_LIBNUMA)
        if (numa_available() >= 0) {
            void* p = numa_alloc_onnode(size, numa_node_);
            if (p) return p;
        }
#endif
        void* p = nullptr;
        // Align to 64-byte CPU cache line boundary
        if (posix_memalign(&p, 64, size) != 0) {
            throw std::bad_alloc();
        }
        return p;
    }

    void deallocate_raw(void* ptr, std::size_t size) {
        if (!ptr) return;
#if defined(__linux__) && defined(GINNUNGA_HAS_LIBNUMA)
        if (numa_available() >= 0) {
            numa_free(ptr, size);
            return;
        }
#endif
        (void)size;
        std::free(ptr);
    }

    void allocate_new_chunk(std::size_t size) {
        void* ptr = allocate_raw(size);
        chunks_.push_back(Chunk{.ptr = ptr, .size = size, .offset = 0});
        total_capacity_ += size;
    }

    int numa_node_{0};
    std::size_t default_chunk_size_{1024 * 1024};
    std::size_t total_allocated_{0};
    std::size_t total_capacity_{0};
    std::vector<Chunk> chunks_;
    mutable std::mutex mutex_;
};

} // namespace ginnunga::memory
