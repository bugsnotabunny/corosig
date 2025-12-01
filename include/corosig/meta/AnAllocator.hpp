#ifndef COROSIG_CONCEPT_AN_ALLOCATOR_HPP
#define COROSIG_CONCEPT_AN_ALLOCATOR_HPP

#include <concepts>
#include <cstddef>

namespace corosig {

/// @brief Tell if T has an interface of an allocator
template <typename ALLOCATOR>
concept AnAllocator = requires(ALLOCATOR alloc, size_t n, size_t align, void *p) {
  { alloc.allocate(n, align) } noexcept -> std::same_as<void *>;
  { alloc.allocate(n) } noexcept -> std::same_as<void *>;
  { alloc.deallocate(p) } noexcept -> std::same_as<void>;
};

} // namespace corosig

#endif
