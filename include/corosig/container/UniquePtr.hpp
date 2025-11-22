#ifndef COROSIG_CONTAINER_UNIQUE_PTR_HPP
#define COROSIG_CONTAINER_UNIQUE_PTR_HPP

#include "corosig/container/Allocator.hpp"
#include "corosig/meta/AnAllocator.hpp"

#include <cassert>
#include <memory>

namespace corosig {

template <typename T, AnAllocator ALLOCATOR = Allocator &>
struct AllocatorBoundDeleter {
  void operator()(T *p) noexcept {
    p->~T();
    return alloc.deallocate(p);
  }

  [[no_unique_address]] ALLOCATOR alloc;
};

/// Usage with pointers to arrays is not allowed
template <typename T, AnAllocator ALLOCATOR = Allocator &>
struct UniquePtr : std::unique_ptr<T, AllocatorBoundDeleter<T, ALLOCATOR>> {
  using std::unique_ptr<T, AllocatorBoundDeleter<T, ALLOCATOR>>::unique_ptr;
};

template <typename T, AnAllocator ALLOCATOR>
struct UniquePtr<T[], ALLOCATOR>; // NOLINT

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
