#pragma once

#include <concepts>
#include <cstddef>
#include <optional>

namespace ginnunga::concepts {

/**
 * @brief EvictionConcept defines how replacement policies track cache accesses
 * and select candidate keys for eviction when the cache reaches capacity.
 */
template <typename E, typename Key>
concept EvictionConcept = requires(E& policy, const E& const_policy, const Key& key) {
    // Notify policy that a key was successfully accessed
    { policy.on_hit(key) } -> std::same_as<void>;

    // Notify policy that a new key was inserted into storage
    { policy.on_insert(key) } -> std::same_as<void>;

    // Notify policy that a key was removed
    { policy.on_erase(key) } -> std::same_as<void>;

    // Select candidate key for eviction (std::nullopt if empty)
    { policy.evict() } -> std::same_as<std::optional<Key>>;

    // Clear eviction state
    { policy.reset() } -> std::same_as<void>;

    // Track policy tracking size
    { const_policy.size() } -> std::convertible_to<std::size_t>;
};

} // namespace ginnunga::concepts
