#include "corosig/testing/MockAllocator.hpp"

namespace corosig::testing {

void *MockAllocator::allocate(size_t n, size_t) noexcept {
  return allocate(n);
}

void *MockAllocator::allocate(size_t n) noexcept {
  ++allocate_calls;
  allocated_bytes += n;
  last_alloc_size = n;
  last_alloc_ptr = ::operator new(n * sizeof(std::max_align_t));
  return last_alloc_ptr;
}

void MockAllocator::deallocate(void *p) noexcept {
  ++deallocate_calls;
  last_dealloc_ptr = p;
  ::operator delete(p);
}

} // namespace corosig::testing
