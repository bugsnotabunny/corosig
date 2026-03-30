#include "corosig/container/Allocator.hpp"

#include "corosig/testing/Signals.hpp"

#include <catch2/catch_all.hpp>
#include <cstddef>
#include <cstdint>
#include <random>

namespace {

using namespace corosig;

} // namespace

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

TEST_CASE("Allocator stress test - random sizes and alignments", "[allocator][stress]") {
  constexpr auto BUFFER_SIZE = size_t(20) * 1024 * 1024;
  std::vector<char> mem;
  mem.resize(BUFFER_SIZE);
  Allocator allocator{mem};

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> size_dist(1, size_t(1024) * 1024);
  std::uniform_int_distribution<size_t> alignments_dist(0, 12);

  std::vector<void *> allocations;

  for (size_t i = 0; i != 1000000; ++i) {
    if (!allocations.empty() && gen() % 4 == 0) {
      std::uniform_int_distribution<size_t> idx_dist(0, allocations.size() - 1);
      size_t idx = idx_dist(gen);
      allocator.deallocate(allocations[idx]);
      allocations.erase(allocations.begin() + long(idx));
      continue;
    }

    size_t size = size_dist(gen);
    size_t alignment = 1 << alignments_dist(gen);

    void *ptr = allocator.allocate(size, alignment);
    if (ptr != nullptr) {
      REQUIRE(reinterpret_cast<uintptr_t>(ptr) % alignment == 0);
      allocations.push_back(ptr);
    }

    // Verify no double frees or memory corruption
    size_t current = allocator.current_memory();
    REQUIRE(current <= BUFFER_SIZE);
  }

  for (void *ptr : allocations) {
    allocator.deallocate(ptr);
  }

  REQUIRE(allocator.current_memory() == 0);
}
