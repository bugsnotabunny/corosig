#include "corosig/container/UniquePtr.hpp"

#include <catch2/catch_all.hpp>
#include <cstddef>

namespace {

using namespace corosig;

struct MockAllocator {
  int allocations = 0;
  int deallocations = 0;
  void *last_alloc_ptr = nullptr;
  size_t last_alloc_size = 0;
  void *last_dealloc_ptr = nullptr;

  void *allocate(size_t size, size_t align) noexcept;

  void *allocate(size_t size) noexcept {
    ++allocations;
    last_alloc_size = size;
    last_alloc_ptr = ::operator new(size);
    return last_alloc_ptr;
  }

  void deallocate(void *p) noexcept {
    ++deallocations;
    last_dealloc_ptr = p;
    ::operator delete(p);
  }
};

struct Tracked {
  static int constructions;
  static int destructions;

  int x;

  Tracked(int v)
      : x(v) {
    constructions++;
  }
  ~Tracked() {
    destructions++;
  }

  static void reset() {
    constructions = 0;
    destructions = 0;
  }
};

int Tracked::constructions = 0;
int Tracked::destructions = 0;
} // namespace

TEST_CASE("make_unique constructs object and allocates memory") {
  MockAllocator alloc;
  Tracked::reset();

  {
    UniquePtr ptr = make_unique<Tracked>(alloc, 42);

    REQUIRE(ptr.get() != nullptr);
    REQUIRE(ptr->x == 42);

    REQUIRE(Tracked::constructions == 1);
    REQUIRE(alloc.allocations == 1);
    REQUIRE(alloc.last_alloc_size == sizeof(Tracked));
  }

  REQUIRE(Tracked::destructions == 1);
  REQUIRE(alloc.deallocations == 1);
  REQUIRE(alloc.last_dealloc_ptr == alloc.last_alloc_ptr);
}

TEST_CASE("UniquePtr move semantics work") {
  MockAllocator alloc;
  Tracked::reset();

  auto p1 = make_unique<Tracked>(alloc, 7);
  REQUIRE(p1.get() != nullptr);

  void *raw = p1.get();

  auto p2 = std::move(p1);

  REQUIRE(p1.get() == nullptr); // NOLINT
  REQUIRE(p2.get() == raw);
  REQUIRE(p2->x == 7);

  REQUIRE(Tracked::constructions == 1);
  REQUIRE(Tracked::destructions == 0);

} // destruction here

TEST_CASE("Deleter uses correct allocator reference") {
  MockAllocator alloc1;
  MockAllocator alloc2;

  {
    auto ptr = make_unique<Tracked>(alloc1, 1);
    REQUIRE(&ptr.get_deleter().alloc == &alloc1);
    REQUIRE(&ptr.get_deleter().alloc != &alloc2);
  }

  REQUIRE(alloc1.deallocations == 1);
  REQUIRE(alloc2.deallocations == 0);
}

TEST_CASE("make_unique returns correct UniquePtr type") {
  MockAllocator alloc;
  auto ptr = make_unique<Tracked>(alloc, 123);
  static_assert(std::same_as<decltype(ptr), UniquePtr<Tracked, MockAllocator &>>);
  static_assert(std::same_as<decltype(make_unique<Tracked>(MockAllocator{}, 123)),
                             UniquePtr<Tracked, MockAllocator>>);
}

TEST_CASE("reset() destroys object and deallocates") {
  MockAllocator alloc;
  Tracked::reset();

  auto ptr = make_unique<Tracked>(alloc, 99);

  REQUIRE(Tracked::constructions == 1);
  REQUIRE(Tracked::destructions == 0);

  ptr.reset(); // trigger destruction

  REQUIRE(ptr.get() == nullptr);
  REQUIRE(Tracked::destructions == 1);
  REQUIRE(alloc.deallocations == 1);
}
