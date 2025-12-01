#ifndef COROSIG_CONTAINER_UNIQUE_PTR_HPP
#define COROSIG_CONTAINER_UNIQUE_PTR_HPP

#include "corosig/container/Allocator.hpp"
#include "corosig/meta/AnAllocator.hpp"

#include <cassert>
#include <memory>

namespace corosig {

/// @brief An allocator-aware deleter for unique pointers
template <typename T, AnAllocator ALLOCATOR = Allocator &>
struct AllocatorBoundDeleter {
  void operator()(T *p) noexcept {
    p->~T();
    return alloc.deallocate(p);
  }

  [[no_unique_address]] ALLOCATOR alloc;
};

/// @brief Unique pointer wrapper to make it easier to use with AllocatorBoundDeleter
template <typename T, AnAllocator ALLOCATOR = Allocator &>
struct UniquePtr : std::unique_ptr<T, AllocatorBoundDeleter<T, ALLOCATOR>> {
  using std::unique_ptr<T, AllocatorBoundDeleter<T, ALLOCATOR>>::unique_ptr;
};

/// @brief Make a unique pointer via given allocator
template <typename T, AnAllocator ALLOCATOR = Allocator &, typename... ARGS>
UniquePtr<T, ALLOCATOR> make_unique(ALLOCATOR &&alloc, ARGS &&...args) noexcept {
  void *p = alloc.allocate(sizeof(T));
  new (p) T{std::forward<ARGS>(args)...};
  return UniquePtr<T, ALLOCATOR>{
      static_cast<T *>(p),
      AllocatorBoundDeleter<T, ALLOCATOR>{std::forward<ALLOCATOR>(alloc)},
  };
}

} // namespace corosig

#endif
