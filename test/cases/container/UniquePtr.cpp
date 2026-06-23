#include "corosig/container/UniquePtr.hpp"

#include "corosig/testing/LifetimeCounter.hpp"
#include "corosig/testing/MockAllocator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

namespace {

using namespace corosig;
using namespace corosig::testing;

} // namespace

TEST_CASE("make_unique constructs object and allocates memory") {
  MockAllocator alloc;
  LifetimeCounter::reset();

  {
    corosig::UniquePtr ptr = std::move(corosig::make_unique<LifetimeCounter>(alloc, 42).value());

    REQUIRE(ptr.get() != nullptr);
    REQUIRE(ptr->value == 42);

    REQUIRE(LifetimeCounter::constructed == 1);
    REQUIRE(alloc.allocate_calls == 1);
    REQUIRE(alloc.last_alloc_size == sizeof(LifetimeCounter));
  }

  REQUIRE(LifetimeCounter::destructed == 1);
  REQUIRE(alloc.deallocate_calls == 1);
  REQUIRE(alloc.last_dealloc_ptr == alloc.last_alloc_ptr);
}

TEST_CASE("UniquePtr move semantics work") {
  MockAllocator alloc;
  LifetimeCounter::reset();

  auto p1 = std::move(corosig::make_unique<LifetimeCounter>(alloc, 7).value());
  REQUIRE(p1.get() != nullptr);

  void *raw = p1.get();

  auto p2 = std::move(p1);

  REQUIRE(p1.get() == nullptr); // NOLINT
  REQUIRE(p2.get() == raw);
  REQUIRE(p2->value == 7);

  REQUIRE(LifetimeCounter::constructed == 1);
  REQUIRE(LifetimeCounter::destructed == 0);

} // destruction here

TEST_CASE("Deleteter uses correct allocator reference") {
  MockAllocator alloc1;
  MockAllocator alloc2;

  {
    auto ptr = std::move(corosig::make_unique<LifetimeCounter>(alloc1, 1).value());
    REQUIRE(&ptr.get_deleter().alloc == &alloc1);
    REQUIRE(&ptr.get_deleter().alloc != &alloc2);
  }

  REQUIRE(alloc1.deallocate_calls == 1);
  REQUIRE(alloc2.deallocate_calls == 0);
}

TEST_CASE("make_unique returns correct UniquePtr type") {
  MockAllocator alloc;
  auto ptr = corosig::make_unique<LifetimeCounter>(alloc, 123);
  static_assert(std::same_as<std::decay_t<decltype(ptr.value())>,
                             corosig::UniquePtr<LifetimeCounter, MockAllocator &>>);
  auto ptr2 = corosig::make_unique<LifetimeCounter>(MockAllocator{}, 123);
  static_assert(std::same_as<std::decay_t<decltype(ptr.value())>,
                             corosig::UniquePtr<LifetimeCounter, MockAllocator &>>);
}

TEST_CASE("reset() destroys object and deallocates") {
  MockAllocator alloc;
  LifetimeCounter::reset();

  auto ptr = std::move(corosig::make_unique<LifetimeCounter>(alloc, 99).value());

  REQUIRE(LifetimeCounter::constructed == 1);
  REQUIRE(LifetimeCounter::destructed == 0);

  ptr.reset(); // trigger destruction

  REQUIRE(ptr.get() == nullptr);
  REQUIRE(LifetimeCounter::destructed == 1);
  REQUIRE(alloc.deallocate_calls == 1);
}
