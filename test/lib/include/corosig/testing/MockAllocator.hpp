#ifndef COROSIG_TESTING_MOCK_ALLOCATOR_HPP
#define COROSIG_TESTING_MOCK_ALLOCATOR_HPP

#include <cstddef>

namespace corosig::testing {

struct MockAllocator {
  size_t allocate_calls = 0;
  size_t deallocate_calls = 0;
  size_t allocated_bytes = 0;
  void *last_alloc_ptr = nullptr;
  size_t last_alloc_size = 0;
  void *last_dealloc_ptr = nullptr;

  void *allocate(size_t n, size_t) noexcept {
    return allocate(n);
  }

  void *allocate(size_t n) noexcept {
    ++allocate_calls;
    allocated_bytes += n;
    last_alloc_size = n;
    last_alloc_ptr = ::operator new(n * sizeof(std::max_align_t));
    return last_alloc_ptr;
  }

  void deallocate(void *p) noexcept {
    ++deallocate_calls;
    last_dealloc_ptr = p;
    ::operator delete(p);
  }
};

} // namespace corosig::testing

#endif
