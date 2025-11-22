#include "corosig/container/Vector.hpp"

#include "catch2/reporters/catch_reporter_registrars.hpp"
#include "corosig/testing/Signals.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <catch2/catch_all.hpp>

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
  int v;
  MoveOnly(int x) noexcept
      : v(x) {
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
  COROSIG_REQUIRE(v[0].v == 1);
  COROSIG_REQUIRE(v[1].v == 2);
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
  COROSIG_REQUIRE(!r.is_ok());
  COROSIG_REQUIRE(r.error().holds<CloneError>());
}

COROSIG_SIGHANDLER_TEST_CASE("resize() growing branch uses clone() and propagates failure",
                             "[vector][resize][error]") {
  Vector<NonCopyableCloneable> v{reactor.allocator()};
  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{10}));

  auto result = v.resize(3, NonCopyableCloneable{777, true});

  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(result.error().holds<CloneError>());
  COROSIG_REQUIRE(v.size() == 1);
}

COROSIG_SIGHANDLER_TEST_CASE("resize() growing branch clones multiple times and fails mid-way",
                             "[vector][resize][error]") {
  Vector<NonCopyableCloneable> v{reactor.allocator()};
  COROSIG_REQUIRE(v.push_back(NonCopyableCloneable{10}));

  auto result = v.resize(5, NonCopyableCloneable{888, true});

  COROSIG_REQUIRE(!result.is_ok());
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
  COROSIG_REQUIRE(result.is_ok());
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

  REQUIRE(result.is_ok());
  REQUIRE(v.size() == 2);
}
