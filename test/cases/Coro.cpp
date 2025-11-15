#include "corosig/Coro.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <catch2/catch_all.hpp>
#include <cmath>
#include <csignal>

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("Coroutine executed synchronously") {
  auto foo = [](Reactor &) -> Fut<int> { //
    co_return 20;
  };
  auto res = foo(reactor).block_on();
  COROSIG_REQUIRE(res);
  COROSIG_REQUIRE(res.value() == 20);
}

COROSIG_SIGHANDLER_TEST_CASE("Fut move constructor transfers ownership") {
  auto foo = [](Reactor &) -> Fut<> { co_return success(); };

  Fut<> fut1 = foo(reactor);
  COROSIG_REQUIRE(fut1.has_value());
  Fut<> fut2{std::move(fut1)};
  COROSIG_REQUIRE(fut2.has_value());
}

COROSIG_SIGHANDLER_TEST_CASE("Fut move constructor with nontrivial move") {
  using MoveType = SetDefaultOnMove<double, double(2 << 10)>;

  auto foo = [](Reactor &) -> Fut<MoveType> { co_return success(MoveType{123.0}); };

  Fut<MoveType> fut1 = foo(reactor);
  COROSIG_REQUIRE(fut1.has_value());

  Fut<MoveType> fut2{std::move(fut1)};
  COROSIG_REQUIRE(fut2.has_value());
}

COROSIG_SIGHANDLER_TEST_CASE("Fut::block_on returns error when reactor fails") {
  Result res = Fut<>::promise_type::get_return_object_on_allocation_failure().block_on();
  COROSIG_REQUIRE(!res.has_value());
  COROSIG_REQUIRE(res.has_error());
  COROSIG_REQUIRE(res.assume_error().holds<AllocationError>());
}

COROSIG_SIGHANDLER_TEST_CASE("Fut can be co_awaited inside a coroutine") {
  constexpr static auto FOO = [](Reactor &) -> Fut<int> { co_return 99; };
  constexpr static auto BAR = [](Reactor &r) -> Fut<int> {
    co_return 123 + (co_await FOO(r)).value();
  };

  auto result = BAR(reactor).block_on();
  REQUIRE(result.has_value());
  REQUIRE(result.assume_value() == 123 + 99);
}
