#include "corosig/util/SetDefaultOnMove.hpp"

#include "corosig/testing/Signals.hpp"

#include <catch2/catch.hpp>
#include <utility>

using namespace corosig;

TEST_CASE("Default constructs to DEFAULT value") {
  test_in_sighandler([] {
    SetDefaultOnMove<int, -1> s;
    COROSIG_REQUIRE(s.value == -1);
  });
}

TEST_CASE("Constructs with explicit value") {
  test_in_sighandler([] {
    SetDefaultOnMove<int, -1> s{42};
    COROSIG_REQUIRE(s.value == 42);
  });
}

TEST_CASE("Move constructor resets source") {
  test_in_sighandler([] {
    SetDefaultOnMove<int, -1> s(42);
    SetDefaultOnMove<int, -1> t(std::move(s));

    COROSIG_REQUIRE(t.value == 42);
    COROSIG_REQUIRE(s.value == -1);
  });
}

TEST_CASE("Move assignment resets source") {
  test_in_sighandler([] {
    SetDefaultOnMove<int, -1> s(100);
    SetDefaultOnMove<int, -1> t(200);

    t = std::move(s);

    COROSIG_REQUIRE(t.value == 100);
    COROSIG_REQUIRE(s.value == -1);
  });
}

TEST_CASE("Multiple chained moves", "[SetDefaultOnMove]") {
  test_in_sighandler([] {
    SetDefaultOnMove<int, -1> a(10);
    SetDefaultOnMove<int, -1> b(std::move(a));
    SetDefaultOnMove<int, -1> c(std::move(b));

    COROSIG_REQUIRE(a.value == -1);
    COROSIG_REQUIRE(b.value == -1);
    COROSIG_REQUIRE(c.value == 10);
  });
}
