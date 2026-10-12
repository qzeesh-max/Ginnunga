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

#include <atomic>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <array>
#include <cstddef>
#include <cstdint>
#include "ginnunga/concepts/concurrency.hpp"

namespace ginnunga::concurrency {

/**
 * @brief EpochReclamation implements Epoch-Based Reclamation (EBR)
 * providing wait-free reads on cache hits and safe deferred memory reclamation.
 * Read threads register with local epochs, ensuring writers only reclaim memory
 * after all concurrent readers have completed their critical read sections.
 */
class EpochReclamation {
public:
    static constexpr std::size_t MAX_THREADS = 64;
    static constexpr std::size_t EPOCH_COUNT = 3;

    struct alignas(64) ThreadState {
        std::atomic<bool> active{false};
        std::atomic<std::uint64_t> epoch{0};
    };

    EpochReclamation() : global_epoch_(0) {
        for (std::size_t i = 0; i < MAX_THREADS; ++i) {
            thread_states_[i].active.store(false, std::memory_order_relaxed);
            thread_states_[i].epoch.store(0, std::memory_order_relaxed);
        }
    }

    ~EpochReclamation() {
        synchronize();
        for (auto& queue : retired_queues_) {
            for (auto& func : queue) {
                if (func) func();
            }
            queue.clear();
        }
    }

    EpochReclamation(const EpochReclamation&) = delete;
    EpochReclamation& operator=(const EpochReclamation&) = delete;

    std::uint64_t acquire_read() noexcept {
        std::size_t tid = get_thread_slot();
        std::uint64_t curr_epoch = global_epoch_.load(std::memory_order_acquire);
        thread_states_[tid].epoch.store(curr_epoch, std::memory_order_relaxed);
        thread_states_[tid].active.store(true, std::memory_order_seq_cst);
        return curr_epoch;
    }

    void release_read() noexcept {
        std::size_t tid = get_thread_slot();
        thread_states_[tid].active.store(false, std::memory_order_release);
    }

    void retire(std::function<void()> cleanup) {
        if (!cleanup) return;
        std::lock_guard<std::mutex> lock(retire_mutex_);
        std::uint64_t curr_epoch = global_epoch_.load(std::memory_order_relaxed);
        std::size_t bucket = curr_epoch % EPOCH_COUNT;
        retired_queues_[bucket].push_back(std::move(cleanup));

        if (retired_queues_[bucket].size() > 64) {
            try_advance_epoch_locked();
        }
    }

    void synchronize() {
        // Advance epoch across full rotation to drain retired callbacks
        for (std::size_t i = 0; i < EPOCH_COUNT * 2; ++i) {
            {
                std::lock_guard<std::mutex> lock(retire_mutex_);
                try_advance_epoch_locked();
            }
            std::this_thread::yield();
        }
    }

    [[nodiscard]] std::uint64_t current_epoch() const noexcept {
        return global_epoch_.load(std::memory_order_relaxed);
    }

private:
    static std::size_t get_thread_slot() noexcept {
        static thread_local std::size_t slot = std::hash<std::thread::id>{}(std::this_thread::get_id()) % MAX_THREADS;
        return slot;
    }

    void try_advance_epoch_locked() {
        std::uint64_t curr_epoch = global_epoch_.load(std::memory_order_relaxed);

        // Check if any active reader is stuck in an older epoch
        for (std::size_t i = 0; i < MAX_THREADS; ++i) {
            if (thread_states_[i].active.load(std::memory_order_acquire)) {
                if (thread_states_[i].epoch.load(std::memory_order_relaxed) < curr_epoch) {
                    return; // Reader is still active in prior epoch
                }
            }
        }

        // Advance global epoch
        std::uint64_t next_epoch = curr_epoch + 1;
        global_epoch_.store(next_epoch, std::memory_order_release);

        // Safely reclaim retired items from two epochs ago
        std::size_t reclaim_bucket = (next_epoch + 1) % EPOCH_COUNT;
        auto to_reclaim = std::move(retired_queues_[reclaim_bucket]);
        retired_queues_[reclaim_bucket].clear();

        for (auto& func : to_reclaim) {
            if (func) func();
        }
    }

    alignas(64) std::atomic<std::uint64_t> global_epoch_{0};
    std::array<ThreadState, MAX_THREADS> thread_states_;
    std::array<std::vector<std::function<void()>>, EPOCH_COUNT> retired_queues_;
    std::mutex retire_mutex_;
};

static_assert(concepts::ConcurrencyConcept<EpochReclamation>,
              "EpochReclamation must satisfy ConcurrencyConcept");

} // namespace ginnunga::concurrency
