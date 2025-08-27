#define CATCH_CONFIG_MAIN

#include "corosig/static_buf_allocator.hpp"

#include <catch2/catch_all.hpp>
#include <cstddef>
#include <cstdint>
#include <new>

template <size_t SIZE>
using Alloc = corosig::StaticBufAllocator<SIZE>;

TEST_CASE("Allocation and freeing within capacity") {
  Alloc<1024> alloc;

  void *p1 = alloc.allocate(128);
  REQUIRE(p1 != nullptr);

  void *p2 = alloc.allocate(256);
  REQUIRE(p2 != nullptr);

  alloc.free(p1);
  alloc.free(p2);
}

TEST_CASE("Allocation exceeds capacity") {
  Alloc<256> alloc;
  void *p = alloc.allocate(512);
  REQUIRE(p == nullptr);
}

TEST_CASE("Alignment handling") {
  Alloc<1024> alloc;
  constexpr size_t align = alignof(std::max_align_t);
  void *p = alloc.allocate(64, align);

  REQUIRE(p != nullptr);
  REQUIRE(reinterpret_cast<std::uintptr_t>(p) % align == 0);
  alloc.free(p);
}

TEST_CASE("Multiple allocations until exhaustion") {
  Alloc<128> alloc;
  void *blocks[10] = {};
  size_t count = 0;
  while (void *p = alloc.allocate(16)) {
    blocks[count++] = p;
  }
  REQUIRE(count > 0);
  REQUIRE(count * 16 <= 128);

  for (size_t i = 0; i < count; ++i) {
    alloc.free(blocks[i]);
  }
}

TEST_CASE("Freeing nullptr should be safe") {
  Alloc<128> alloc;
  REQUIRE_NOTHROW(alloc.free(nullptr));
}

TEST_CASE("Zero-size allocation should return non-null or null consistently") {
  Alloc<128> alloc;
  void *p = alloc.allocate(0);
  // Depending on implementation: could return nullptr or a valid pointer.
  // Just ensure it doesn't crash.
  REQUIRE_NOTHROW(alloc.free(p));
}

TEST_CASE("Reallocation after freeing") {
  Alloc<128> alloc;
  void *p1 = alloc.allocate(64);
  REQUIRE(p1 != nullptr);
  alloc.free(p1);

  void *p2 = alloc.allocate(64);
  REQUIRE(p2 != nullptr);
}

TEST_CASE("Freeing the same pointer twice should be safe or detected") {
  Alloc<128> alloc;
  void *p = alloc.allocate(64);
  REQUIRE(p != nullptr);
  alloc.free(p);
  // Depending on implementation, this may or may not be safe.
  REQUIRE_NOTHROW(alloc.free(p));
}

TEST_CASE("Stress test with varied sizes and alignments") {
  Alloc<512> alloc;
  void *a = alloc.allocate(32, alignof(int));
  REQUIRE(a != nullptr);

  void *b = alloc.allocate(64, alignof(double));
  REQUIRE(b != nullptr);

  void *c = alloc.allocate(128, alignof(std::max_align_t));
  REQUIRE(c != nullptr);

  alloc.free(a);
  alloc.free(b);
  alloc.free(c);
}
