#include "corosig/container/Allocator.hpp"

#include "corosig/testing/Signals.hpp"

#include <catch2/catch_all.hpp>
#include <cstddef>
#include <cstdint>

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("Allocation and freeing within capacity") {
  Allocator::Memory<1024> mem;
  Allocator alloc{mem};

  void *p1 = alloc.allocate(128);
  COROSIG_REQUIRE(p1 != nullptr);

  void *p2 = alloc.allocate(256);
  COROSIG_REQUIRE(p2 != nullptr);

  alloc.deallocate(p1);
  alloc.deallocate(p2);
}

COROSIG_SIGHANDLER_TEST_CASE("Allocation exceeds capacity") {
  Allocator::Memory<256> mem;
  Allocator alloc{mem};

  void *p = alloc.allocate(512);
  COROSIG_REQUIRE(p == nullptr);
}

COROSIG_SIGHANDLER_TEST_CASE("Alignment handling") {
  Allocator::Memory<1024> mem;
  Allocator alloc{mem};

  constexpr size_t ALIGN = alignof(std::max_align_t);
  void *p = alloc.allocate(64, ALIGN);

  COROSIG_REQUIRE(p != nullptr);
  COROSIG_REQUIRE(reinterpret_cast<std::uintptr_t>(p) % ALIGN == 0);
  alloc.deallocate(p);
}

COROSIG_SIGHANDLER_TEST_CASE("Multiple allocations until exhaustion") {
  Allocator::Memory<128> mem;
  Allocator alloc{mem};

  std::array<void *, 10> blocks = {};
  size_t count = 0;
  while (void *p = alloc.allocate(16)) {
    blocks[count++] = p;
  }
  COROSIG_REQUIRE(count > 0);
  COROSIG_REQUIRE(count * 16 <= 128);

  for (size_t i = 0; i < count; ++i) {
    alloc.deallocate(blocks[i]);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Freeing nullptr should be safe") {
  Allocator::Memory<128> mem;
  Allocator alloc{mem};

  alloc.deallocate(nullptr);
}

COROSIG_SIGHANDLER_TEST_CASE("Zero-size allocation should return non-null or null consistently") {
  Allocator::Memory<128> mem;
  Allocator alloc{mem};

  void *p = alloc.allocate(0);
  // Depending on implementation: could return nullptr or a valid pointer.
  // Just ensure it doesn't crash.
  alloc.deallocate(p);
}

COROSIG_SIGHANDLER_TEST_CASE("Reallocation after freeing") {
  Allocator::Memory<128> mem;
  Allocator alloc{mem};

  void *p1 = alloc.allocate(64);
  COROSIG_REQUIRE(p1 != nullptr);
  alloc.deallocate(p1);

  void *p2 = alloc.allocate(64);
  COROSIG_REQUIRE(p2 != nullptr);
  alloc.deallocate(p2);
}

COROSIG_SIGHANDLER_TEST_CASE("Stress test with varied sizes and alignments") {
  Allocator::Memory<512> mem;
  Allocator alloc{mem};

  void *a = alloc.allocate(32, alignof(int));
  COROSIG_REQUIRE(a != nullptr);

  void *b = alloc.allocate(64, alignof(double));
  COROSIG_REQUIRE(b != nullptr);

  void *c = alloc.allocate(128, alignof(std::max_align_t));
  COROSIG_REQUIRE(c != nullptr);

  alloc.deallocate(a);
  alloc.deallocate(b);
  alloc.deallocate(c);
}
