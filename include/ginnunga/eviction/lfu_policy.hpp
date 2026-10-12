#pragma once

#include <unordered_map>
#include <list>
#include <optional>
#include <cstddef>
#include "ginnunga/concepts/eviction.hpp"

namespace ginnunga::eviction {

/**
 * @brief LFUPolicy tracks access frequency of keys in O(1) time
 * using frequency buckets and doubly linked lists.
 */
template <typename Key>
class LFUPolicy {
public:
    LFUPolicy() = default;
    ~LFUPolicy() = default;

    void on_hit(const Key& key) {
        auto it = key_node_map_.find(key);
        if (it == key_node_map_.end()) return;

        std::size_t old_freq = it->second.freq;
        auto list_it = it->second.list_iter;

        // Erase from old frequency bucket
        freq_buckets_[old_freq].erase(list_it);
        if (freq_buckets_[old_freq].empty()) {
            freq_buckets_.erase(old_freq);
            if (min_freq_ == old_freq) {
                ++min_freq_;
            }
        }

        // Increment frequency and add to new bucket
        std::size_t new_freq = old_freq + 1;
        freq_buckets_[new_freq].push_front(key);
        it->second.freq = new_freq;
        it->second.list_iter = freq_buckets_[new_freq].begin();
    }

    void on_insert(const Key& key) {
        auto it = key_node_map_.find(key);
        if (it != key_node_map_.end()) {
            on_hit(key);
            return;
        }

        min_freq_ = 1;
        freq_buckets_[1].push_front(key);
        key_node_map_[key] = Node{
            .freq = 1,
            .list_iter = freq_buckets_[1].begin()
        };
    }

    void on_erase(const Key& key) {
        auto it = key_node_map_.find(key);
        if (it == key_node_map_.end()) return;

        std::size_t freq = it->second.freq;
        auto list_it = it->second.list_iter;

        freq_buckets_[freq].erase(list_it);
        if (freq_buckets_[freq].empty()) {
            freq_buckets_.erase(freq);
            if (min_freq_ == freq) {
                // Find next smallest frequency
                min_freq_ = 0;
                for (const auto& [f, _] : freq_buckets_) {
                    if (min_freq_ == 0 || f < min_freq_) {
                        min_freq_ = f;
                    }
                }
            }
        }
        key_node_map_.erase(it);
    }

    std::optional<Key> evict() {
        if (key_node_map_.empty()) return std::nullopt;

        auto bucket_it = freq_buckets_.find(min_freq_);
        if (bucket_it == freq_buckets_.end() || bucket_it->second.empty()) {
            return std::nullopt;
        }

        Key lfu_key = bucket_it->second.back();
        bucket_it->second.pop_back();

        if (bucket_it->second.empty()) {
            freq_buckets_.erase(bucket_it);
            // Recompute min_freq_
            min_freq_ = 0;
            for (const auto& [f, _] : freq_buckets_) {
                if (min_freq_ == 0 || f < min_freq_) {
                    min_freq_ = f;
                }
            }
        }

        key_node_map_.erase(lfu_key);
        return lfu_key;
    }

    void reset() {
        key_node_map_.clear();
        freq_buckets_.clear();
        min_freq_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept { return key_node_map_.size(); }
    [[nodiscard]] bool empty() const noexcept { return key_node_map_.empty(); }

private:
    struct Node {
        std::size_t freq{1};
        typename std::list<Key>::iterator list_iter;
    };

    std::unordered_map<Key, Node> key_node_map_;
    std::unordered_map<std::size_t, std::list<Key>> freq_buckets_;
    std::size_t min_freq_{0};
};

static_assert(concepts::EvictionConcept<LFUPolicy<int>, int>,
              "LFUPolicy must satisfy EvictionConcept");

} // namespace ginnunga::eviction
