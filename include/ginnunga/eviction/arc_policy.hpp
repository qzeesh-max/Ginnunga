#pragma once

#include <list>
#include <unordered_map>
#include <optional>
#include <cstddef>
#include <algorithm>
#include "ginnunga/concepts/eviction.hpp"

namespace ginnunga::eviction {

/**
 * @brief ARCPolicy (Adaptive Replacement Cache) dynamically balances between
 * recency (T1) and frequency (T2) using ghost caches (B1, B2) to tune
 * adaptation parameter p without manual heuristics.
 */
template <typename Key>
class ARCPolicy {
public:
    explicit ARCPolicy(std::size_t capacity = 1024)
        : c_(std::max(std::size_t(2), capacity)), p_(0) {}

    void on_hit(const Key& key) {
        // Case 1: Hit in T1 -> move to T2 MRU
        if (auto it = t1_map_.find(key); it != t1_map_.end()) {
            t1_.erase(it->second);
            t1_map_.erase(it);

            t2_.push_front(key);
            t2_map_[key] = t2_.begin();
            return;
        }

        // Case 2: Hit in T2 -> move to T2 MRU
        if (auto it = t2_map_.find(key); it != t2_map_.end()) {
            t2_.splice(t2_.begin(), t2_, it->second);
            return;
        }
    }

    void on_insert(const Key& key) {
        // Case A: Key was in ghost cache B1 (recency hit in ghost)
        if (auto it = b1_map_.find(key); it != b1_map_.end()) {
            std::size_t delta = b1_.size() >= b2_.size() ? 1 : std::max(std::size_t(1), b2_.size() / b1_.size());
            p_ = std::min(c_, p_ + delta);

            b1_.erase(it->second);
            b1_map_.erase(it);

            t2_.push_front(key);
            t2_map_[key] = t2_.begin();
            return;
        }

        // Case B: Key was in ghost cache B2 (frequency hit in ghost)
        if (auto it = b2_map_.find(key); it != b2_map_.end()) {
            std::size_t delta = b2_.size() >= b1_.size() ? 1 : std::max(std::size_t(1), b1_.size() / b2_.size());
            p_ = p_ > delta ? p_ - delta : 0;

            b2_.erase(it->second);
            b2_map_.erase(it);

            t2_.push_front(key);
            t2_map_[key] = t2_.begin();
            return;
        }

        // Case C: Brand new key -> insert into T1 MRU
        t1_.push_front(key);
        t1_map_[key] = t1_.begin();
    }

    void on_erase(const Key& key) {
        if (auto it = t1_map_.find(key); it != t1_map_.end()) {
            t1_.erase(it->second);
            t1_map_.erase(it);
        } else if (auto it2 = t2_map_.find(key); it2 != t2_map_.end()) {
            t2_.erase(it2->second);
            t2_map_.erase(it2);
        } else if (auto it3 = b1_map_.find(key); it3 != b1_map_.end()) {
            b1_.erase(it3->second);
            b1_map_.erase(it3);
        } else if (auto it4 = b2_map_.find(key); it4 != b2_map_.end()) {
            b2_.erase(it4->second);
            b2_map_.erase(it4);
        }
    }

    std::optional<Key> evict() {
        if (size() == 0) return std::nullopt;

        // Eviction algorithm: replace from T1 if |T1| > p, else from T2
        if (!t1_.empty() && (t1_.size() > p_ || (t2_.empty()))) {
            Key victim = t1_.back();
            t1_.pop_back();
            t1_map_.erase(victim);

            // Move victim into ghost cache B1
            b1_.push_front(victim);
            b1_map_[victim] = b1_.begin();
            trim_ghosts();
            return victim;
        } else if (!t2_.empty()) {
            Key victim = t2_.back();
            t2_.pop_back();
            t2_map_.erase(victim);

            // Move victim into ghost cache B2
            b2_.push_front(victim);
            b2_map_[victim] = b2_.begin();
            trim_ghosts();
            return victim;
        }

        return std::nullopt;
    }

    void reset() {
        t1_.clear();
        t1_map_.clear();
        t2_.clear();
        t2_map_.clear();
        b1_.clear();
        b1_map_.clear();
        b2_.clear();
        b2_map_.clear();
        p_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return t1_.size() + t2_.size();
    }

    [[nodiscard]] std::size_t target_p() const noexcept { return p_; }

private:
    void trim_ghosts() {
        while (b1_.size() > c_) {
            Key old_key = b1_.back();
            b1_.pop_back();
            b1_map_.erase(old_key);
        }
        while (b2_.size() > c_) {
            Key old_key = b2_.back();
            b2_.pop_back();
            b2_map_.erase(old_key);
        }
    }

    std::size_t c_{1024};
    std::size_t p_{0};

    std::list<Key> t1_;
    std::unordered_map<Key, typename std::list<Key>::iterator> t1_map_;

    std::list<Key> t2_;
    std::unordered_map<Key, typename std::list<Key>::iterator> t2_map_;

    std::list<Key> b1_;
    std::unordered_map<Key, typename std::list<Key>::iterator> b1_map_;

    std::list<Key> b2_;
    std::unordered_map<Key, typename std::list<Key>::iterator> b2_map_;
};

static_assert(concepts::EvictionConcept<ARCPolicy<int>, int>,
              "ARCPolicy must satisfy EvictionConcept");

} // namespace ginnunga::eviction
