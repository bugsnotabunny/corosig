#include "corosig/container/Vector.hpp"

#include "catch2/catch_test_macros.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <catch2/catch_all.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <initializer_list>
#include <list>

namespace {

using namespace corosig;

struct LifetimeCounter {
  static inline int constructed = 0;
  static inline int destructed = 0;
  int value;
  SetDefaultOnMove<bool, false> owned = true;

  LifetimeCounter(int v = 1234) noexcept
      : value(v) {
    constructed++;
  }

  LifetimeCounter(LifetimeCounter const &rhs) noexcept
      : value{rhs.value},
        owned{*rhs.owned} {
    if (*owned) {
      constructed++;
    }
  }

  LifetimeCounter(LifetimeCounter &&) noexcept = default;

  ~LifetimeCounter() {
    if (*owned) {
      destructed++;
    }
  }

  static void reset() {
    constructed = 0;
    destructed = 0;
  }

  bool operator==(int rhs) {
    return value == rhs;
  }
};

struct LifetimeCounterResetListener : Catch::EventListenerBase {
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const &) override {
    LifetimeCounter::reset();
  }
};

CATCH_REGISTER_LISTENER(LifetimeCounterResetListener);

struct MoveOnly {
  int value;
  MoveOnly(int x) noexcept
      : value(x) {
  }
  MoveOnly(const MoveOnly &) = delete;
  MoveOnly(MoveOnly &&) noexcept = default;
  MoveOnly &operator=(MoveOnly &&) noexcept = default;
};

struct CloneError {};

struct NonCopyableCloneable {
  int value = 67;
  bool fail_clone = false;

  NonCopyableCloneable() noexcept = default;

  NonCopyableCloneable(int v, bool fail_clone = false) noexcept
      : value{v},
        fail_clone{fail_clone} {
  }

  NonCopyableCloneable(const NonCopyableCloneable &) = delete;
  NonCopyableCloneable &operator=(const NonCopyableCloneable &) = delete;

  NonCopyableCloneable(NonCopyableCloneable &&other) noexcept = default;
  NonCopyableCloneable &operator=(NonCopyableCloneable &&) noexcept = default;

  Result<NonCopyableCloneable, CloneError> clone() const noexcept {
    if (fail_clone) {
      return Failure{CloneError{}};
    }
    return NonCopyableCloneable{value, false};
  }
};

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Vector default constructs empty", "[vector]") {
  {
    Vector<LifetimeCounter> v{reactor.allocator()};
    COROSIG_REQUIRE(v.empty());
    COROSIG_REQUIRE(v.capacity() == 0);
    COROSIG_REQUIRE(v.empty());
  }
  COROSIG_REQUIRE(LifetimeCounter::constructed == 0);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("push_back increases size", "[vector]") {
  {
    Vector<LifetimeCounter> v{reactor.allocator()};
    REQUIRE(v.empty());

    COROSIG_REQUIRE(v.push_back(1));
    COROSIG_REQUIRE(v.push_back(2));

    COROSIG_REQUIRE(v.size() == 2);
    COROSIG_REQUIRE(v[0] == 1);
    COROSIG_REQUIRE(v[1] == 2);
  }

  COROSIG_REQUIRE(LifetimeCounter::constructed == 2);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 2);
}

COROSIG_SIGHANDLER_TEST_CASE("push_back triggers growth", "[vector]") {
  {
    Vector<LifetimeCounter> v{reactor.allocator()};
    COROSIG_REQUIRE(v.push_back(10));
    auto old_cap = v.capacity();

    COROSIG_REQUIRE(v.push_back(20));

    COROSIG_REQUIRE(v.size() == 2);
    COROSIG_REQUIRE(v.capacity() >= old_cap);
  }

  COROSIG_REQUIRE(LifetimeCounter::constructed == 2);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 2);
}

COROSIG_SIGHANDLER_TEST_CASE("pop_back destroys last object", "[vector]") {

  {
    Vector<LifetimeCounter> v{reactor.allocator()};
    COROSIG_REQUIRE(v.push_back(LifetimeCounter(1)));
    COROSIG_REQUIRE(v.push_back(LifetimeCounter(2)));

    COROSIG_REQUIRE(LifetimeCounter::constructed == 2);

    v.pop_back();
    COROSIG_REQUIRE(LifetimeCounter::destructed == 1);
  }

  COROSIG_REQUIRE(LifetimeCounter::constructed == 2);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 2);
}

COROSIG_SIGHANDLER_TEST_CASE("clear destroys all elements", "[vector]") {

  {
    Vector<LifetimeCounter> v{reactor.allocator()};
    for (int i = 0; i < 5; i++) {
      COROSIG_REQUIRE(v.push_back(LifetimeCounter(i)));
    }
    COROSIG_REQUIRE(LifetimeCounter::constructed == 5);
    v.clear();
    COROSIG_REQUIRE(LifetimeCounter::destructed == 5);
  }

  COROSIG_REQUIRE(LifetimeCounter::constructed == 5);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 5);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector supports move-only types", "[vector]") {
  Vector<MoveOnly> v{reactor.allocator()};

  COROSIG_REQUIRE(v.push_back(MoveOnly{1}));
  COROSIG_REQUIRE(v.push_back(MoveOnly{2}));

  COROSIG_REQUIRE(v.size() == 2);
  COROSIG_REQUIRE(v[0].value == 1);
  COROSIG_REQUIRE(v[1].value == 2);
}

COROSIG_SIGHANDLER_TEST_CASE("reserve increases capacity", "[vector]") {
  Vector<LifetimeCounter> v{reactor.allocator()};

  COROSIG_REQUIRE(v.reserve(50));
  COROSIG_REQUIRE(v.capacity() >= 50);
  COROSIG_REQUIRE(v.empty());

  COROSIG_REQUIRE(LifetimeCounter::constructed == 0);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("resize growing constructs new objects", "[vector]") {
  Vector<LifetimeCounter> v{reactor.allocator()};
  COROSIG_REQUIRE(v.resize(5));
  COROSIG_REQUIRE(LifetimeCounter::constructed == 6);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 1);

  COROSIG_REQUIRE(v.size() == 5);
  COROSIG_REQUIRE(v.capacity() == 5);
}

COROSIG_SIGHANDLER_TEST_CASE("resize shrinking destroys objects", "[vector]") {
  Vector<LifetimeCounter> v{reactor.allocator()};
  COROSIG_REQUIRE(v.resize(5));
  COROSIG_REQUIRE(LifetimeCounter::constructed == 6);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 1);

  COROSIG_REQUIRE(v.resize(2));
  COROSIG_REQUIRE(v.size() == 2);
  COROSIG_REQUIRE(LifetimeCounter::constructed == 7);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 5);
}

COROSIG_SIGHANDLER_TEST_CASE("shrink_to_fit reduces capacity", "[vector]") {
  Vector<int> v{reactor.allocator()};
  for (int i = 0; i < 10; i++) {
    COROSIG_REQUIRE(v.push_back(i));
  }

  auto old_cap = v.capacity();
  COROSIG_REQUIRE(v.shrink_to_fit());

  COROSIG_REQUIRE(v.capacity() <= old_cap);
  COROSIG_REQUIRE(v.size() == 10);
}

COROSIG_SIGHANDLER_TEST_CASE("clone produces a deep copy", "[vector]") {
  Vector<int> v{reactor.allocator()};
  COROSIG_REQUIRE(v.push_back(5));
  COROSIG_REQUIRE(v.push_back(7));

  auto cloned = v.clone();
  COROSIG_REQUIRE(cloned);
  Vector<int> v2{std::move(cloned.value())};

  COROSIG_REQUIRE(v2.size() == 2);
  COROSIG_REQUIRE(v2[0] == 5);
  COROSIG_REQUIRE(v2[1] == 7);
}

COROSIG_SIGHANDLER_TEST_CASE("clone() of vector with clone-only value_type succeeds",
                             "[vector][clone]") {
  Vector<NonCopyableCloneable> v{reactor.allocator()};

  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{1}));
  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{2}));

  auto r = v.clone();
  COROSIG_REQUIRE(r);
  Vector<NonCopyableCloneable> v2 = std::move(r.value());

  COROSIG_REQUIRE(v2.size() == 2);
  COROSIG_REQUIRE(v2[0].value == 1);
  COROSIG_REQUIRE(v2[1].value == 2);
}

COROSIG_SIGHANDLER_TEST_CASE("clone() propagates clone failure from value_type",
                             "[vector][clone]") {
  Vector<NonCopyableCloneable> v{reactor.allocator()};

  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{1}));
  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{99, true}));

  auto r = v.clone();
  COROSIG_REQUIRE(!r);
  COROSIG_REQUIRE(r.error().holds<CloneError>());
}

COROSIG_SIGHANDLER_TEST_CASE("resize() growing branch uses clone() and propagates failure",
                             "[vector][resize][error]") {
  Vector<NonCopyableCloneable> v{reactor.allocator()};
  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{10}));

  auto result = v.resize(3, NonCopyableCloneable{777, true});

  COROSIG_REQUIRE(!result);
  COROSIG_REQUIRE(result.error().holds<CloneError>());
  COROSIG_REQUIRE(v.size() == 1);
}

COROSIG_SIGHANDLER_TEST_CASE("resize() growing branch clones multiple times and fails mid-way",
                             "[vector][resize][error]") {
  Vector<NonCopyableCloneable> v{reactor.allocator()};
  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{10}));

  auto result = v.resize(5, NonCopyableCloneable{888, true});

  COROSIG_REQUIRE(!result);
  COROSIG_REQUIRE(result.error().holds<CloneError>());
  COROSIG_REQUIRE(v.size() == 1);
}

COROSIG_SIGHANDLER_TEST_CASE("shrink_to_fit with clone-only value_type (no errors)",
                             "[vector][shrink]") {
  Vector<NonCopyableCloneable> v{reactor.allocator()};
  for (int i = 0; i < 5; i++) {
    COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{i}));
  }

  auto old_cap = v.capacity();

  auto result = v.shrink_to_fit();
  COROSIG_REQUIRE(result);
  COROSIG_REQUIRE(v.size() == 5);
  COROSIG_REQUIRE(v.capacity() <= old_cap);
}

COROSIG_SIGHANDLER_TEST_CASE(
    "shrink_to_fit does NOT call clone() and therefore never propagates clone errors",
    "[vector][shrink][noerror]") {
  Vector<NonCopyableCloneable> v{reactor.allocator()};
  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{1}));
  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{2, true}));

  auto result = v.shrink_to_fit();

  COROSIG_REQUIRE(result);
  COROSIG_REQUIRE(v.size() == 2);
}

COROSIG_SIGHANDLER_TEST_CASE("assign from sized range", "[vector][assign]") {
  Vector<int> v{reactor.allocator()};
  std::array src{1, 2, 3};

  auto result = v.assign(src);
  COROSIG_REQUIRE(result);

  COROSIG_REQUIRE(v.size() == 3);
  COROSIG_REQUIRE(v[0] == 1);
  COROSIG_REQUIRE(v[1] == 2);
  COROSIG_REQUIRE(v[2] == 3);
}

TEST_CASE("assign from unsized range", "[vector][assign]") {

  Allocator::Memory<1234> mem;
  Allocator alloc{mem};
  Vector<int> v{alloc};

  std::list<int> src{10, 20, 30, 40};

  auto result = v.assign(src);
  REQUIRE(result);

  REQUIRE(v.size() == 4);
  REQUIRE(v[0] == 10);
  REQUIRE(v[1] == 20);
  REQUIRE(v[2] == 30);
  REQUIRE(v[3] == 40);
}

COROSIG_SIGHANDLER_TEST_CASE("assign replaces old contents", "[vector][assign]") {
  Vector<int> v{reactor.allocator()};

  COROSIG_REQUIRE(v.push_back(99));

  std::array src{5, 6};

  COROSIG_REQUIRE(v.assign(src));
  COROSIG_REQUIRE(v.size() == 2);
  COROSIG_REQUIRE(v[0] == 5);
  COROSIG_REQUIRE(v[1] == 6);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::erase(pos) removes a single element and shifts others") {
  Vector<LifetimeCounter> v{reactor.allocator()};
  COROSIG_REQUIRE(v.push_back(LifetimeCounter{1}));
  COROSIG_REQUIRE(v.push_back(LifetimeCounter{2}));
  COROSIG_REQUIRE(v.push_back(LifetimeCounter{3}));

  auto *it = v.erase(v.begin() + 1);
  COROSIG_REQUIRE(it == v.begin() + 1);
  COROSIG_REQUIRE(v.size() == 2);

  COROSIG_REQUIRE(v[0].value == 1);
  COROSIG_REQUIRE(v[1].value == 3);

  COROSIG_REQUIRE(LifetimeCounter::constructed == 3);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 1);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert single element at beginning") {
  Vector<int> vec{reactor.allocator()};

  auto *it = vec.insert(vec.begin(), 42).value();

  COROSIG_REQUIRE(vec.size() == 1);
  COROSIG_REQUIRE(vec[0] == 42);
  COROSIG_REQUIRE(it == vec.begin());
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert single element in middle") {
  Vector<int> vec{reactor.allocator()};

  COROSIG_REQUIRE(vec.push_back(1));
  COROSIG_REQUIRE(vec.push_back(3));

  auto *it = vec.insert(vec.begin() + 1, 2).value();

  COROSIG_REQUIRE(vec.size() == 3);
  COROSIG_REQUIRE(vec[0] == 1);
  COROSIG_REQUIRE(vec[1] == 2);
  COROSIG_REQUIRE(vec[2] == 3);
  COROSIG_REQUIRE(it == vec.begin() + 1);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert single element at end") {
  Vector<int> vec{reactor.allocator()};

  COROSIG_REQUIRE(vec.push_back(1));
  COROSIG_REQUIRE(vec.push_back(2));

  auto *it = vec.insert(vec.end(), 3).value();

  COROSIG_REQUIRE(vec.size() == 3);
  COROSIG_REQUIRE(vec[0] == 1);
  COROSIG_REQUIRE(vec[1] == 2);
  COROSIG_REQUIRE(vec[2] == 3);
  COROSIG_REQUIRE(it == vec.begin() + 2);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert single element into empty vector") {
  Vector<int> vec{reactor.allocator()};

  auto *it = vec.insert(vec.begin(), 99).value();

  COROSIG_REQUIRE(vec.size() == 1);
  COROSIG_REQUIRE(vec[0] == 99);
  COROSIG_REQUIRE(it == vec.begin());
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert single element triggers reallocation") {
  Vector<int> vec{reactor.allocator()};

  // Fill to force reallocation (assuming small initial capacity)
  for (int i = 0; i < 16; ++i) {
    COROSIG_REQUIRE(vec.push_back(i));
  }

  size_t old_capacity = vec.capacity();
  auto *old_data = vec.data();

  COROSIG_REQUIRE(vec.insert(vec.begin() + 5, 99));

  COROSIG_REQUIRE(vec.capacity() > old_capacity);
  COROSIG_REQUIRE(vec.data() != old_data);
  COROSIG_REQUIRE(vec.size() == 17);
  COROSIG_REQUIRE(vec[5] == 99);

  // Verify all elements
  for (int i = 0; i < 5; ++i) {
    COROSIG_REQUIRE(vec[i] == i);
  }
  for (int i = 6; i < 17; ++i) {
    COROSIG_REQUIRE(vec[i] == i - 1);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert single element with move semantics") {
  Vector<MoveOnly, Allocator &> vec{reactor.allocator()};

  COROSIG_REQUIRE(vec.push_back(1));
  COROSIG_REQUIRE(vec.push_back(2));

  MoveOnly value{3};
  COROSIG_REQUIRE(vec.insert(vec.begin() + 1, std::move(value)));

  COROSIG_REQUIRE(vec.size() == 3);
  COROSIG_REQUIRE(vec[0].value == 1);
  COROSIG_REQUIRE(vec[1].value == 3);
  COROSIG_REQUIRE(vec[2].value == 2);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert range at beginning") {
  Vector<int> vec{reactor.allocator()};

  COROSIG_REQUIRE(vec.push_back(4));
  COROSIG_REQUIRE(vec.push_back(5));

  std::initializer_list<int> to_insert{1, 2, 3};
  auto *it = vec.insert(vec.begin(), to_insert).value();

  COROSIG_REQUIRE(vec.size() == 5);
  COROSIG_REQUIRE(vec[0] == 1);
  COROSIG_REQUIRE(vec[1] == 2);
  COROSIG_REQUIRE(vec[2] == 3);
  COROSIG_REQUIRE(vec[3] == 4);
  COROSIG_REQUIRE(vec[4] == 5);
  COROSIG_REQUIRE(it == vec.begin());
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert range in middle") {

  Vector<int, Allocator &> vec{reactor.allocator()};

  for (int i = 0; i < 5; ++i) {
    COROSIG_REQUIRE(vec.push_back(i * 10));
  }

  std::array<int, 3> to_insert{15, 16, 17};
  auto *it = vec.insert(vec.begin() + 2, to_insert).value();

  COROSIG_REQUIRE(vec.size() == 8);
  COROSIG_REQUIRE(vec[0] == 0);
  COROSIG_REQUIRE(vec[1] == 10);
  COROSIG_REQUIRE(vec[2] == 15);
  COROSIG_REQUIRE(vec[3] == 16);
  COROSIG_REQUIRE(vec[4] == 17);
  COROSIG_REQUIRE(vec[5] == 20);
  COROSIG_REQUIRE(vec[6] == 30);
  COROSIG_REQUIRE(vec[7] == 40);
  COROSIG_REQUIRE(it == vec.begin() + 2);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert range at end") {
  Vector<int> vec{reactor.allocator()};

  COROSIG_REQUIRE(vec.push_back(1));
  COROSIG_REQUIRE(vec.push_back(2));

  std::initializer_list<int> to_insert{3, 4, 5};
  auto *it = vec.insert(vec.end(), to_insert).value();

  COROSIG_REQUIRE(vec.size() == 5);
  COROSIG_REQUIRE(vec[0] == 1);
  COROSIG_REQUIRE(vec[1] == 2);
  COROSIG_REQUIRE(vec[2] == 3);
  COROSIG_REQUIRE(vec[3] == 4);
  COROSIG_REQUIRE(vec[4] == 5);
  COROSIG_REQUIRE(it == vec.begin() + 2);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert empty range") {
  Vector<int> vec{reactor.allocator()};

  COROSIG_REQUIRE(vec.push_back(1));
  COROSIG_REQUIRE(vec.push_back(2));

  auto *it = vec.insert(vec.begin() + 1, std::initializer_list<int>{}).value();

  COROSIG_REQUIRE(vec.size() == 2);
  COROSIG_REQUIRE(vec[0] == 1);
  COROSIG_REQUIRE(vec[1] == 2);
  COROSIG_REQUIRE(it == vec.begin() + 1);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert object lifetime") {
  Vector<LifetimeCounter> vec{reactor.allocator()};

  COROSIG_REQUIRE(vec.insert(vec.begin(), LifetimeCounter{123}));

  COROSIG_REQUIRE(LifetimeCounter::constructed == 1);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 0);

  COROSIG_REQUIRE(vec[0].value == 123);

  vec.clear();
  COROSIG_REQUIRE(LifetimeCounter::constructed == 1);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 1);
}

COROSIG_SIGHANDLER_TEST_CASE("Vector::insert returns iterator to first inserted element") {
  Vector<int, Allocator &> vec{reactor.allocator()};

  for (int i = 0; i < 5; ++i) {
    COROSIG_REQUIRE(vec.push_back(i));
  }

  auto *it1 = vec.insert(vec.begin() + 2, 99).value();
  COROSIG_REQUIRE(it1 == vec.begin() + 2);
  COROSIG_REQUIRE(*it1 == 99);

  auto *it2 = vec.insert(vec.begin() + 4, std::array{10, 11, 12}).value();
  COROSIG_REQUIRE(it2 == vec.begin() + 4);
  COROSIG_REQUIRE(*it2 == 10);
  COROSIG_REQUIRE(*(it2 + 1) == 11);
  COROSIG_REQUIRE(*(it2 + 2) == 12);
}
