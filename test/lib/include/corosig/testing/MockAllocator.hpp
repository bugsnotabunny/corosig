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

  void *allocate(size_t n, size_t) noexcept;
  void *allocate(size_t n) noexcept;
  void deallocate(void *p) noexcept;
};

} // namespace corosig::testing

#endif
