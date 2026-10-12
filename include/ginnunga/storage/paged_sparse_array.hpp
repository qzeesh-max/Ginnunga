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
#include <cstdint>
#include <concepts>
#include <sys/mman.h>
#include <unistd.h>
#include <new>
#include <utility>
#include <algorithm>
#include <vector>
#include "ginnunga/concepts/storage.hpp"

namespace ginnunga::storage {

/**
 * @brief PagedSparseArray reserves a large virtual address space via mmap()
 * without committing physical memory upfront. Physical RAM is allocated lazily
 * by the kernel upon writing to a page. When cleared, madvise(MADV_DONTNEED)
 * releases physical pages back to the OS while preserving virtual mappings.
 *
 * Ideal for sparse 32-bit or 64-bit integer identifiers (e.g. FIX Order IDs).
 */
template <std::integral Key, typename Value>
class PagedSparseArray {
public:
    struct Slot {
        Value value{};
        bool occupied{false};
    };

    explicit PagedSparseArray(std::size_t virtual_capacity = 1024 * 1024)
        : capacity_(virtual_capacity), page_size_(static_cast<std::size_t>(sysconf(_SC_PAGESIZE))) {
        
        std::size_t total_bytes = capacity_ * sizeof(Slot);
        // Align up to page boundary
        total_bytes = (total_bytes + page_size_ - 1) & ~(page_size_ - 1);
        total_mapped_bytes_ = total_bytes;

        void* ptr = mmap(nullptr, total_mapped_bytes_, PROT_READ | PROT_WRITE,
                         MAP_ANON | MAP_PRIVATE, -1, 0);
        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }

        slots_ = static_cast<Slot*>(ptr);
    }

    ~PagedSparseArray() {
        if (slots_) {
            // Destruct occupied values
            for (const auto& key : occupied_keys_) {
                if (key < capacity_ && slots_[key].occupied) {
                    slots_[key].value.~Value();
                }
            }
            munmap(slots_, total_mapped_bytes_);
            slots_ = nullptr;
        }
    }

    PagedSparseArray(const PagedSparseArray&) = delete;
    PagedSparseArray& operator=(const PagedSparseArray&) = delete;

    PagedSparseArray(PagedSparseArray&& other) noexcept
        : slots_(std::exchange(other.slots_, nullptr)),
          capacity_(other.capacity_),
          total_mapped_bytes_(other.total_mapped_bytes_),
          page_size_(other.page_size_),
          size_(other.size_),
          occupied_keys_(std::move(other.occupied_keys_)) {}

    PagedSparseArray& operator=(PagedSparseArray&& other) noexcept {
        if (this != &other) {
            this->~PagedSparseArray();
            new (this) PagedSparseArray(std::move(other));
        }
        return *this;
    }

    Value* find(const Key& key) noexcept {
        if (static_cast<std::size_t>(key) >= capacity_) return nullptr;
        Slot& slot = slots_[key];
        return slot.occupied ? &slot.value : nullptr;
    }

    const Value* find(const Key& key) const noexcept {
        if (static_cast<std::size_t>(key) >= capacity_) return nullptr;
        const Slot& slot = slots_[key];
        return slot.occupied ? &slot.value : nullptr;
    }

    bool insert(const Key& key, const Value& val) {
        if (static_cast<std::size_t>(key) >= capacity_) {
            expand_virtual_space(static_cast<std::size_t>(key) + 1);
        }
        Slot& slot = slots_[key];
        if (!slot.occupied) {
            slot.value = val;
            slot.occupied = true;
            occupied_keys_.push_back(key);
            ++size_;
        } else {
            slot.value = val;
        }
        return true;
    }

private:
    void expand_virtual_space(std::size_t required_capacity) {
        std::size_t new_cap = std::max(capacity_ * 2, required_capacity);
        std::size_t new_bytes = new_cap * sizeof(Slot);
        new_bytes = (new_bytes + page_size_ - 1) & ~(page_size_ - 1);

        void* new_ptr = mmap(nullptr, new_bytes, PROT_READ | PROT_WRITE,
                             MAP_ANON | MAP_PRIVATE, -1, 0);
        if (new_ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }

        Slot* new_slots = static_cast<Slot*>(new_ptr);
        for (const auto& k : occupied_keys_) {
            if (k < capacity_ && slots_[k].occupied) {
                new_slots[k] = std::move(slots_[k]);
            }
        }

        munmap(slots_, total_mapped_bytes_);
        slots_ = new_slots;
        capacity_ = new_cap;
        total_mapped_bytes_ = new_bytes;
    }

public:

    bool erase(const Key& key) {
        if (static_cast<std::size_t>(key) >= capacity_) return false;
        Slot& slot = slots_[key];
        if (!slot.occupied) return false;

        slot.occupied = false;
        slot.value = Value{};
        --size_;

        auto it = std::find(occupied_keys_.begin(), occupied_keys_.end(), key);
        if (it != occupied_keys_.end()) {
            occupied_keys_.erase(it);
        }
        return true;
    }

    void clear() {
        for (const auto& key : occupied_keys_) {
            if (key < capacity_) {
                slots_[key].occupied = false;
                slots_[key].value = Value{};
            }
        }
        occupied_keys_.clear();
        size_ = 0;

#if defined(MADV_DONTNEED)
        // Discard physical pages immediately while keeping virtual mappings
        madvise(slots_, total_mapped_bytes_, MADV_DONTNEED);
#endif
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t memory_footprint() const noexcept {
        // Approximate committed physical memory: pages touched + vector overhead
        std::size_t touched_pages = (size_ * sizeof(Slot) + page_size_ - 1) / page_size_;
        return touched_pages * page_size_ + (occupied_keys_.capacity() * sizeof(Key));
    }

    [[nodiscard]] const std::vector<Key>& occupied_keys() const noexcept {
        return occupied_keys_;
    }

private:
    Slot* slots_{nullptr};
    std::size_t capacity_{0};
    std::size_t total_mapped_bytes_{0};
    std::size_t page_size_{4096};
    std::size_t size_{0};
    std::vector<Key> occupied_keys_{};
};

static_assert(concepts::StorageConcept<PagedSparseArray<uint32_t, int>, uint32_t, int>,
              "PagedSparseArray must satisfy StorageConcept");

} // namespace ginnunga::storage
