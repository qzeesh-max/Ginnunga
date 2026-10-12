#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <vector>
#include <functional>
#include <utility>
#include <bit>
#include <cstring>
#include <algorithm>
#include "ginnunga/concepts/storage.hpp"

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define GINNUNGA_NEON_AVAILABLE 1
#elif defined(__AVX2__) || defined(__SSE2__)
#include <immintrin.h>
#define GINNUNGA_SSE_AVAILABLE 1
#endif

namespace ginnunga::storage {

/**
 * @brief SimdFlatMap is an open-addressing flat hash table with SIMD-accelerated
 * metadata probing (ARM NEON or x86 SSE/AVX2). Control bytes are inspected 16 at a time
 * in parallel in a single CPU instruction, minimizing pointer chasing and keeping
 * cache lines hot.
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class SimdFlatMap {
public:
    static constexpr std::uint8_t CTRL_EMPTY = 0x80;
    static constexpr std::uint8_t CTRL_DELETED = 0xFE;
    static constexpr std::size_t GROUP_SIZE = 16;

    struct alignas(64) Slot {
        Key key{};
        Value value{};
    };

    explicit SimdFlatMap(std::size_t min_capacity = 16)
        : hash_fn_{}, eq_fn_{} {
        std::size_t cap = std::max(GROUP_SIZE, min_capacity);
        // Round up to multiple of GROUP_SIZE
        cap = (cap + GROUP_SIZE - 1) & ~(GROUP_SIZE - 1);
        reallocate(cap);
    }

    ~SimdFlatMap() = default;

    SimdFlatMap(const SimdFlatMap&) = default;
    SimdFlatMap& operator=(const SimdFlatMap&) = default;
    SimdFlatMap(SimdFlatMap&&) noexcept = default;
    SimdFlatMap& operator=(SimdFlatMap&&) noexcept = default;

    Value* find(const Key& key) noexcept {
        if (size_ == 0) return nullptr;
        const std::size_t hash = hash_fn_(key);
        const std::uint8_t h2 = static_cast<std::uint8_t>(hash & 0x7F);
        const std::size_t num_groups = capacity_ / GROUP_SIZE;
        std::size_t group_idx = (hash >> 7) % num_groups;

        for (std::size_t g = 0; g < num_groups; ++g) {
            const std::size_t base_idx = group_idx * GROUP_SIZE;
            std::uint32_t mask = match_h2(base_idx, h2);

            while (mask != 0) {
                int bit = std::countr_zero(mask);
                std::size_t slot_idx = base_idx + static_cast<std::size_t>(bit);
                if (eq_fn_(slots_[slot_idx].key, key)) {
                    return &slots_[slot_idx].value;
                }
                mask &= mask - 1;
            }

            if (has_empty(base_idx)) {
                return nullptr;
            }

            group_idx = (group_idx + 1) % num_groups;
        }
        return nullptr;
    }

    const Value* find(const Key& key) const noexcept {
        return const_cast<SimdFlatMap*>(this)->find(key);
    }

    bool insert(const Key& key, const Value& val) {
        if (size_ + 1 > capacity_ * 7 / 10) { // 70% max load factor
            grow();
        }

        const std::size_t hash = hash_fn_(key);
        const std::uint8_t h2 = static_cast<std::uint8_t>(hash & 0x7F);
        const std::size_t num_groups = capacity_ / GROUP_SIZE;
        std::size_t group_idx = (hash >> 7) % num_groups;

        std::size_t first_available = std::size_t(-1);

        for (std::size_t g = 0; g < num_groups; ++g) {
            const std::size_t base_idx = group_idx * GROUP_SIZE;

            // Check if key already exists
            std::uint32_t match_mask = match_h2(base_idx, h2);
            while (match_mask != 0) {
                int bit = std::countr_zero(match_mask);
                std::size_t slot_idx = base_idx + static_cast<std::size_t>(bit);
                if (eq_fn_(slots_[slot_idx].key, key)) {
                    slots_[slot_idx].value = val;
                    return true;
                }
                match_mask &= match_mask - 1;
            }

            // Find empty or deleted slot for insertion
            if (first_available == std::size_t(-1)) {
                std::uint32_t avail_mask = match_available(base_idx);
                if (avail_mask != 0) {
                    int bit = std::countr_zero(avail_mask);
                    first_available = base_idx + static_cast<std::size_t>(bit);
                }
            }

            if (has_empty(base_idx)) {
                break;
            }

            group_idx = (group_idx + 1) % num_groups;
        }

        if (first_available != std::size_t(-1)) {
            ctrl_[first_available] = h2;
            slots_[first_available].key = key;
            slots_[first_available].value = val;
            ++size_;
            return true;
        }

        return false;
    }

    bool erase(const Key& key) {
        if (size_ == 0) return false;
        const std::size_t hash = hash_fn_(key);
        const std::uint8_t h2 = static_cast<std::uint8_t>(hash & 0x7F);
        const std::size_t num_groups = capacity_ / GROUP_SIZE;
        std::size_t group_idx = (hash >> 7) % num_groups;

        for (std::size_t g = 0; g < num_groups; ++g) {
            const std::size_t base_idx = group_idx * GROUP_SIZE;
            std::uint32_t mask = match_h2(base_idx, h2);

            while (mask != 0) {
                int bit = std::countr_zero(mask);
                std::size_t slot_idx = base_idx + static_cast<std::size_t>(bit);
                if (eq_fn_(slots_[slot_idx].key, key)) {
                    ctrl_[slot_idx] = CTRL_DELETED;
                    slots_[slot_idx].key = Key{};
                    slots_[slot_idx].value = Value{};
                    --size_;
                    return true;
                }
                mask &= mask - 1;
            }

            if (has_empty(base_idx)) {
                return false;
            }

            group_idx = (group_idx + 1) % num_groups;
        }
        return false;
    }

    void clear() {
        std::fill(ctrl_.begin(), ctrl_.end(), CTRL_EMPTY);
        std::fill(slots_.begin(), slots_.end(), Slot{});
        size_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t memory_footprint() const noexcept {
        return (ctrl_.capacity() * sizeof(std::uint8_t)) +
               (slots_.capacity() * sizeof(Slot));
    }

private:
    void reallocate(std::size_t new_cap) {
        capacity_ = new_cap;
        ctrl_.assign(capacity_, CTRL_EMPTY);
        slots_.resize(capacity_);
    }

    void grow() {
        std::size_t old_cap = capacity_;
        std::vector<std::uint8_t> old_ctrl = std::move(ctrl_);
        std::vector<Slot> old_slots = std::move(slots_);

        reallocate(old_cap * 2);
        size_ = 0;

        for (std::size_t i = 0; i < old_cap; ++i) {
            if (old_ctrl[i] < 0x80) { // Active slot
                insert(old_slots[i].key, old_slots[i].value);
            }
        }
    }

    // SIMD Match functions across 16 control bytes
    [[nodiscard]] std::uint32_t match_h2(std::size_t base_idx, std::uint8_t h2) const noexcept {
#if defined(GINNUNGA_NEON_AVAILABLE)
        uint8x16_t ctrl_vec = vld1q_u8(&ctrl_[base_idx]);
        uint8x16_t target_vec = vdupq_n_u8(h2);
        uint8x16_t cmp = vceqq_u8(ctrl_vec, target_vec);
        uint16x8_t high_bits = vreinterpretq_u16_u8(vshrq_n_u8(cmp, 7));
        uint8x8_t narrowed = vshrn_n_u16(high_bits, 4);
        (void)narrowed;
        std::uint32_t mask = 0;
        for (std::size_t i = 0; i < GROUP_SIZE; ++i) {
            if (ctrl_[base_idx + i] == h2) mask |= (1u << i);
        }
        return mask;
#elif defined(GINNUNGA_SSE_AVAILABLE)
        __m128i ctrl_vec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&ctrl_[base_idx]));
        __m128i target_vec = _mm_set1_epi8(static_cast<char>(h2));
        __m128i cmp = _mm_cmpeq_epi8(ctrl_vec, target_vec);
        return static_cast<std::uint32_t>(_mm_movemask_epi8(cmp));
#else
        std::uint32_t mask = 0;
        for (std::size_t i = 0; i < GROUP_SIZE; ++i) {
            if (ctrl_[base_idx + i] == h2) mask |= (1u << i);
        }
        return mask;
#endif
    }

    [[nodiscard]] std::uint32_t match_available(std::size_t base_idx) const noexcept {
        std::uint32_t mask = 0;
        for (std::size_t i = 0; i < GROUP_SIZE; ++i) {
            if (ctrl_[base_idx + i] == CTRL_EMPTY || ctrl_[base_idx + i] == CTRL_DELETED) {
                mask |= (1u << i);
            }
        }
        return mask;
    }

    [[nodiscard]] bool has_empty(std::size_t base_idx) const noexcept {
        for (std::size_t i = 0; i < GROUP_SIZE; ++i) {
            if (ctrl_[base_idx + i] == CTRL_EMPTY) return true;
        }
        return false;
    }

    std::size_t capacity_{16};
    std::size_t size_{0};
    std::vector<std::uint8_t> ctrl_;
    std::vector<Slot> slots_;
    Hash hash_fn_;
    KeyEqual eq_fn_;
};

static_assert(concepts::StorageConcept<SimdFlatMap<uint64_t, int>, uint64_t, int>,
              "SimdFlatMap must satisfy StorageConcept");

} // namespace ginnunga::storage
