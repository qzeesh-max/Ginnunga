#pragma once

#include <concepts>
#include <functional>

namespace ginnunga::concepts {

/**
 * @brief ConcurrencyConcept defines the thread synchronization and reclamation contracts.
 * 
 * Supports both single-threaded zero-overhead models and lock-free/wait-free
 * multi-threaded models (e.g., Epoch-Based Reclamation, Hazard Pointers).
 */
template <typename C>
concept ConcurrencyConcept = requires(C& concurrency) {
    // Acquire read registration (e.g., entering epoch or pin hazard pointer)
    { concurrency.acquire_read() };

    // Release read registration
    { concurrency.release_read() } -> std::same_as<void>;

    // Retire an object for deferred destruction once no active readers can observe it
    { concurrency.retire(std::declval<std::function<void()>>()) } -> std::same_as<void>;

    // Ensure all prior retired actions have been completed
    { concurrency.synchronize() } -> std::same_as<void>;
};

} // namespace ginnunga::concepts
