#ifndef COROSIG_CONTAINER_UNIQUE_PTR_HPP
#define COROSIG_CONTAINER_UNIQUE_PTR_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/meta/AnAllocator.hpp"

#include <cassert>
#include <cstddef>
#include <memory>

namespace corosig {

/// @brief An allocator-aware deleter for unique pointers
template <AnAllocator ALLOCATOR = AllocatorRef<Allocator>>
struct AllocatorBoundDeleter {
  template <typename T>
  void operator()(T *p) noexcept {
    if (p) {
      p->~T();
      return alloc.deallocate(p);
    }
  }

  [[no_unique_address]] ALLOCATOR alloc;
};

/// @brief Unique pointer wrapper to make it easier to use with AllocatorBoundDeleter
template <typename T, AnAllocator ALLOCATOR = AllocatorRef<Allocator>>
struct UniquePtr : std::unique_ptr<T, AllocatorBoundDeleter<ALLOCATOR>> {
  using std::unique_ptr<T, AllocatorBoundDeleter<ALLOCATOR>>::unique_ptr;

  UniquePtr(T *p, ALLOCATOR alloc) noexcept
      : std::unique_ptr<T, AllocatorBoundDeleter<ALLOCATOR>>{
            p, AllocatorBoundDeleter<ALLOCATOR>{std::forward<ALLOCATOR>(alloc)}} {
  }

  UniquePtr(std::nullptr_t, ALLOCATOR alloc) noexcept
      : std::unique_ptr<T, AllocatorBoundDeleter<ALLOCATOR>>{
            static_cast<T *>(nullptr),
            AllocatorBoundDeleter<ALLOCATOR>{std::forward<ALLOCATOR>(alloc)}} {
  }
};

/// @brief Make a unique pointer via given allocator
template <typename T, AnAllocator ALLOCATOR = AllocatorRef<Allocator>, typename... ARGS>
Result<UniquePtr<T, ALLOCATOR>, AllocationError> make_unique(ALLOCATOR &&alloc,
                                                             ARGS &&...args) noexcept {
  void *p = alloc.allocate(sizeof(T), alignof(T));
  if (p == nullptr) {
    return Failure{AllocationError{}};
  }
  new (p) T{std::forward<ARGS>(args)...};
  return UniquePtr<T, ALLOCATOR>{
      static_cast<T *>(p),
      AllocatorBoundDeleter<ALLOCATOR>{std::forward<ALLOCATOR>(alloc)},
  };
}

} // namespace corosig

#endif
