#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace ginnunga::concepts {

/**
 * @brief StorageConcept defines the contract for pluggable sparse storage backends.
 * 
 * Backends manage sparse association of Key to Value, emphasizing minimal
 * overhead, cache-locality, and predictable access latency.
 */
template <typename S, typename Key, typename Value>
concept StorageConcept = requires(S& storage, const S& const_storage, const Key& key, const Value& val) {
    // Lookup returns a pointer to Value (nullptr if absent)
    { storage.find(key) } -> std::same_as<Value*>;
    { const_storage.find(key) } -> std::same_as<const Value*>;

    // Insertion of key-value pair
    { storage.insert(key, val) } -> std::same_as<bool>;

    // Erase by key
    { storage.erase(key) } -> std::same_as<bool>;

    // Metrics & introspection
    { const_storage.size() } -> std::convertible_to<std::size_t>;
    { const_storage.capacity() } -> std::convertible_to<std::size_t>;
    { const_storage.memory_footprint() } -> std::convertible_to<std::size_t>;

    // Resetting
    { storage.clear() } -> std::same_as<void>;
};

} // namespace ginnunga::concepts
