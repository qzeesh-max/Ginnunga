#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <mutex>

#include "ginnunga/concepts/storage.hpp"
#include "ginnunga/concepts/eviction.hpp"
#include "ginnunga/concepts/concurrency.hpp"
#include "ginnunga/storage/simd_flat_map.hpp"
#include "ginnunga/eviction/lru_policy.hpp"
#include "ginnunga/concurrency/single_threaded.hpp"
#include "ginnunga/telemetry/meta_dumper.hpp"

namespace ginnunga {

/**
 * @brief SparseCache is the primary zero-cost composable cache framework orchestrator.
 * 
 * Takes Key, Value, and pluggable policy concepts for Storage, Eviction, and Concurrency.
 * Provides deterministic low-latency access across sparse address spaces.
 */
template <
    typename Key,
    typename Value,
    typename Storage = storage::SimdFlatMap<Key, Value>,
    typename Eviction = eviction::LRUPolicy<Key>,
    typename Concurrency = concurrency::SingleThreaded
>
requires concepts::StorageConcept<Storage, Key, Value> &&
         concepts::EvictionConcept<Eviction, Key> &&
         concepts::ConcurrencyConcept<Concurrency>
class SparseCache {
public:
    using key_type = Key;
    using value_type = Value;
    using storage_type = Storage;
    using eviction_type = Eviction;
    using concurrency_type = Concurrency;

    explicit SparseCache(std::size_t max_capacity = 10000)
        : max_capacity_(max_capacity), storage_(), eviction_(), concurrency_() {}

    ~SparseCache() {
        clear();
    }

    SparseCache(const SparseCache&) = delete;
    SparseCache& operator=(const SparseCache&) = delete;
    SparseCache(SparseCache&&) noexcept = default;
    SparseCache& operator=(SparseCache&&) noexcept = default;

    /**
     * @brief Lookup an entry by Key. Updates eviction policy on hit.
     */
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        concurrency_.acquire_read();
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);

        Value* ptr = storage_.find(key);
        if (ptr) {
            eviction_.on_hit(key);
            ++stats_.hits;
            Value val = *ptr;
            concurrency_.release_read();
            return val;
        }

        ++stats_.misses;
        concurrency_.release_read();
        return std::nullopt;
    }

    /**
     * @brief Direct pointer lookup without value copying.
     */
    [[nodiscard]] Value* find(const Key& key) noexcept {
        concurrency_.acquire_read();
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);

        Value* ptr = storage_.find(key);
        if (ptr) {
            eviction_.on_hit(key);
            ++stats_.hits;
        } else {
            ++stats_.misses;
        }
        concurrency_.release_read();
        return ptr;
    }

    /**
     * @brief Check whether a key is present.
     */
    [[nodiscard]] bool contains(const Key& key) const noexcept {
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);
        return storage_.find(key) != nullptr;
    }

    /**
     * @brief Insert or update a key-value pair. Triggers eviction if capacity is reached.
     */
    bool put(const Key& key, const Value& val) {
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);

        // If key already exists, update and promote
        if (Value* existing = storage_.find(key); existing != nullptr) {
            *existing = val;
            eviction_.on_hit(key);
            return true;
        }

        // Cache capacity check: trigger eviction
        if (storage_.size() >= max_capacity_) {
            if (auto victim = eviction_.evict(); victim.has_value()) {
                Key v_key = *victim;
                storage_.erase(v_key);
                concurrency_.retire([v_key]() {
                    (void)v_key;
                });
                ++stats_.evictions;
            }
        }

        bool inserted = storage_.insert(key, val);
        if (inserted) {
            eviction_.on_insert(key);
            ++stats_.inserts;
        }
        return inserted;
    }

    /**
     * @brief Remove an entry by key.
     */
    bool erase(const Key& key) {
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);
        bool erased = storage_.erase(key);
        if (erased) {
            eviction_.on_erase(key);
            concurrency_.retire([key]() {
                (void)key;
            });
            ++stats_.erasures;
        }
        return erased;
    }

    /**
     * @brief Clear all cached entries.
     */
    void clear() {
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);
        storage_.clear();
        eviction_.reset();
        concurrency_.synchronize();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);
        return storage_.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return max_capacity_;
    }

    [[nodiscard]] std::size_t memory_footprint() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);
        return storage_.memory_footprint();
    }

    [[nodiscard]] telemetry::CacheStats stats() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);
        return stats_;
    }

    [[nodiscard]] std::string dump_telemetry_json(const std::string& name = "GinnungaCache") const {
        std::lock_guard<std::recursive_mutex> lock(write_mutex_);
        return telemetry::MetaDumper::dump_json(*this, stats_, name);
    }

    // Direct access to underlying policies
    [[nodiscard]] Storage& storage() noexcept { return storage_; }
    [[nodiscard]] const Storage& storage() const noexcept { return storage_; }
    [[nodiscard]] Eviction& eviction() noexcept { return eviction_; }
    [[nodiscard]] const Eviction& eviction() const noexcept { return eviction_; }
    [[nodiscard]] Concurrency& concurrency() noexcept { return concurrency_; }
    [[nodiscard]] const Concurrency& concurrency() const noexcept { return concurrency_; }

private:
    std::size_t max_capacity_{10000};
    Storage storage_;
    Eviction eviction_;
    Concurrency concurrency_;
    telemetry::CacheStats stats_{};
    mutable std::recursive_mutex write_mutex_;
};

} // namespace ginnunga
