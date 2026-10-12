#pragma once

#include <functional>
#include "ginnunga/concepts/concurrency.hpp"

namespace ginnunga::concurrency {

/**
 * @brief SingleThreaded concurrency policy provides zero runtime overhead,
 * no atomics, and immediate retirement for single-threaded usage.
 */
class SingleThreaded {
public:
    int acquire_read() noexcept { return 0; }
    void release_read() noexcept {}

    void retire(std::function<void()> cleanup) {
        if (cleanup) cleanup();
    }

    void synchronize() noexcept {}
};

static_assert(concepts::ConcurrencyConcept<SingleThreaded>,
              "SingleThreaded must satisfy ConcurrencyConcept");

} // namespace ginnunga::concurrency
