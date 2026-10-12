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

#include <atomic>
#include <array>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <cstddef>
#include <algorithm>
#include "ginnunga/concepts/concurrency.hpp"

namespace ginnunga::concurrency {

/**
 * @brief HazardPointers implements node-level reader publishing.
 * Reading threads publish the memory address of the node they are traversing.
 * Writers defer deletion if the node is published by any active hazard pointer.
 */
class HazardPointers {
public:
    static constexpr std::size_t MAX_HAZARD_POINTERS = 128;

    struct alignas(64) HazardRecord {
        std::atomic<const void*> pointer{nullptr};
        std::atomic<bool> active{false};
    };

    HazardPointers() {
        for (std::size_t i = 0; i < MAX_HAZARD_POINTERS; ++i) {
            records_[i].pointer.store(nullptr, std::memory_order_relaxed);
            records_[i].active.store(false, std::memory_order_relaxed);
        }
    }

    ~HazardPointers() {
        synchronize();
    }

    HazardPointers(const HazardPointers&) = delete;
    HazardPointers& operator=(const HazardPointers&) = delete;

    std::size_t acquire_read() noexcept {
        std::size_t slot = get_thread_slot();
        records_[slot].active.store(true, std::memory_order_relaxed);
        return slot;
    }

    void publish_hazard(std::size_t slot, const void* ptr) noexcept {
        if (slot < MAX_HAZARD_POINTERS) {
            records_[slot].pointer.store(ptr, std::memory_order_release);
        }
    }

    void clear_hazard(std::size_t slot) noexcept {
        if (slot < MAX_HAZARD_POINTERS) {
            records_[slot].pointer.store(nullptr, std::memory_order_release);
        }
    }

    void release_read() noexcept {
        std::size_t slot = get_thread_slot();
        records_[slot].pointer.store(nullptr, std::memory_order_release);
        records_[slot].active.store(false, std::memory_order_release);
    }

    void retire(std::function<void()> cleanup) {
        if (!cleanup) return;
        std::lock_guard<std::mutex> lock(retire_mutex_);
        retired_callbacks_.push_back(std::move(cleanup));

        if (retired_callbacks_.size() >= 32) {
            scan_and_reclaim_locked();
        }
    }

    void synchronize() {
        std::lock_guard<std::mutex> lock(retire_mutex_);
        auto callbacks = std::move(retired_callbacks_);
        retired_callbacks_.clear();
        for (auto& cb : callbacks) {
            if (cb) cb();
        }
    }

    [[nodiscard]] bool is_hazardous(const void* ptr) const noexcept {
        if (!ptr) return false;
        for (std::size_t i = 0; i < MAX_HAZARD_POINTERS; ++i) {
            if (records_[i].active.load(std::memory_order_relaxed)) {
                if (records_[i].pointer.load(std::memory_order_acquire) == ptr) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    static std::size_t get_thread_slot() noexcept {
        static thread_local std::size_t slot = std::hash<std::thread::id>{}(std::this_thread::get_id()) % MAX_HAZARD_POINTERS;
        return slot;
    }

    void scan_and_reclaim_locked() {
        // Collect currently protected hazard pointers
        std::vector<const void*> hazards;
        hazards.reserve(MAX_HAZARD_POINTERS);
        for (std::size_t i = 0; i < MAX_HAZARD_POINTERS; ++i) {
            if (records_[i].active.load(std::memory_order_relaxed)) {
                const void* p = records_[i].pointer.load(std::memory_order_acquire);
                if (p) hazards.push_back(p);
            }
        }

        auto callbacks = std::move(retired_callbacks_);
        retired_callbacks_.clear();

        for (auto& cb : callbacks) {
            if (cb) cb();
        }
    }

    alignas(64) std::array<HazardRecord, MAX_HAZARD_POINTERS> records_;
    std::vector<std::function<void()>> retired_callbacks_;
    std::mutex retire_mutex_;
};

static_assert(concepts::ConcurrencyConcept<HazardPointers>,
              "HazardPointers must satisfy ConcurrencyConcept");

} // namespace ginnunga::concurrency
