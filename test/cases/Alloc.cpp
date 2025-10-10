#include "corosig/Alloc.hpp"

#include "corosig/testing/Signals.hpp"

#include <catch2/catch.hpp>
#include <cstddef>
#include <cstdint>

using namespace corosig;

TEST_CASE("Allocation and freeing within capacity") {
  test_in_sighandler([] {
    Alloc::Memory<1024> mem;
    Alloc alloc{mem};

    void *p1 = alloc.allocate(128);
    COROSIG_REQUIRE(p1 != nullptr);

    void *p2 = alloc.allocate(256);
    COROSIG_REQUIRE(p2 != nullptr);

    alloc.free(p1);
    alloc.free(p2);
  });
}

TEST_CASE("Allocation exceeds capacity") {
  test_in_sighandler([] {
    Alloc::Memory<256> mem;
    Alloc alloc{mem};

    void *p = alloc.allocate(512);
    COROSIG_REQUIRE(p == nullptr);
  });
}

TEST_CASE("Alignment handling") {
  test_in_sighandler([] {
    Alloc::Memory<1024> mem;
    Alloc alloc{mem};

    constexpr size_t align = alignof(std::max_align_t);
    void *p = alloc.allocate(64, align);

    COROSIG_REQUIRE(p != nullptr);
    COROSIG_REQUIRE(reinterpret_cast<std::uintptr_t>(p) % align == 0);
    alloc.free(p);
  });
}

TEST_CASE("Multiple allocations until exhaustion") {
  test_in_sighandler([] {
    Alloc::Memory<128> mem;
    Alloc alloc{mem};

    void *blocks[10] = {};
    size_t count = 0;
    while (void *p = alloc.allocate(16)) {
      blocks[count++] = p;
    }
    COROSIG_REQUIRE(count > 0);
    COROSIG_REQUIRE(count * 16 <= 128);

    for (size_t i = 0; i < count; ++i) {
      alloc.free(blocks[i]);
    }
  });
}

TEST_CASE("Freeing nullptr should be safe") {
  test_in_sighandler([] {
    Alloc::Memory<128> mem;
    Alloc alloc{mem};

    alloc.free(nullptr);
  });
}

TEST_CASE("Zero-size allocation should return non-null or null consistently") {
  test_in_sighandler([] {
    Alloc::Memory<128> mem;
    Alloc alloc{mem};

    void *p = alloc.allocate(0);
    // Depending on implementation: could return nullptr or a valid pointer.
    // Just ensure it doesn't crash.
    alloc.free(p);
  });
}

TEST_CASE("Reallocation after freeing") {
  test_in_sighandler([] {
    Alloc::Memory<128> mem;
    Alloc alloc{mem};

    void *p1 = alloc.allocate(64);
    COROSIG_REQUIRE(p1 != nullptr);
    alloc.free(p1);

    void *p2 = alloc.allocate(64);
    COROSIG_REQUIRE(p2 != nullptr);
    alloc.free(p2);
  });
}

TEST_CASE("Stress test with varied sizes and alignments") {
  test_in_sighandler([] {
    Alloc::Memory<512> mem;
    Alloc alloc{mem};

    void *a = alloc.allocate(32, alignof(int));
    COROSIG_REQUIRE(a != nullptr);

    void *b = alloc.allocate(64, alignof(double));
    COROSIG_REQUIRE(b != nullptr);

    void *c = alloc.allocate(128, alignof(std::max_align_t));
    COROSIG_REQUIRE(c != nullptr);

    alloc.free(a);
    alloc.free(b);
    alloc.free(c);
  });
}
