#pragma once

#include <concepts>
#include <cstddef>
#include <memory_resource>

namespace ginnunga::concepts {

/**
 * @brief MemoryResourceConcept requires compatibility with std::pmr::memory_resource
 * or an interface providing allocate, deallocate, and is_equal.
 */
template <typename M>
concept MemoryResourceConcept = requires(M& res, void* ptr, std::size_t bytes, std::size_t alignment) {
    { res.allocate(bytes, alignment) } -> std::same_as<void*>;
    { res.deallocate(ptr, bytes, alignment) } -> std::same_as<void>;
    { res.is_equal(std::declval<const M&>()) } -> std::convertible_to<bool>;
} || std::derived_from<M, std::pmr::memory_resource>;

} // namespace ginnunga::concepts
