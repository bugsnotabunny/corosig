#ifndef COROSIG_CONTAINER_STD_ALLOC_HPP
#define COROSIG_CONTAINER_STD_ALLOC_HPP

#include "corosig/meta/AnAllocator.hpp"

#include <cassert>
#include <cstddef>
#include <utility>

namespace corosig {

/// @brief STL-compatible allocator adapter to be used with typeless allocators. Supports both
///        stateless and statefull allocators. Statefull allocator may be owned by this adapter or
///        not, as demanded
/// @warning Use with caution. Since most STL classes throw exceptions on failure and this is not
///          only isn't signal safe, but will also most likely provoke a call to std::abort, since
///          Fut does not allow exceptions
/// @code
/// StdAllocator<int, GlobalAllocator> alloc;
/// StdAllocator<int, LocalAllocator> owning_alloc{std::move(local_alloc)};
/// StdAllocator<int, LocalAllocator&> non_owning_alloc{local_alloc}
/// @endcode
template <typename T, AnAllocator ALLOCATOR>
struct StdAllocator {
public:
  using value_type = T;
  using pointer = T *;
  using size_type = size_t;

  /// @brief Construct this adapter with default constructed ALLOCATOR under the hood
  StdAllocator() noexcept = default;

  /// @brief Construct this adapter with move-constructed ALLOCATOR
  StdAllocator(ALLOCATOR &&alloc) noexcept
      : m_alloc(std::forward<ALLOCATOR>(alloc)) {
  }

  /// @brief Allocate buffer for n objects via underlying allocator
  pointer allocate(size_type n) noexcept {
    return static_cast<pointer>(m_alloc.allocate(sizeof(value_type) * n, alignof(value_type)));
  }

  /// @brief Deallocate p via underlying allocator
  void deallocate(pointer p, size_type) noexcept {
    return m_alloc.deallocate(p);
  }

private:
  [[no_unique_address]] ALLOCATOR m_alloc;
};

} // namespace corosig

#endif
