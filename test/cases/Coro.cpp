#include "corosig/Coro.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/Sleep.hpp"
#include "corosig/Yield.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/LifetimeCounter.hpp"
#include "corosig/testing/NonCopyable.hpp"
#include "corosig/testing/NonMovable.hpp"
#include "corosig/testing/Signals.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

namespace {

using namespace corosig;
using namespace corosig::testing;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Coroutine executed synchronously") {
  auto foo = [](Reactor &) -> Fut<int> { //
    co_return 20;
  };
  auto res = foo(reactor).block_on();
  COROSIG_REQUIRE(res);
  COROSIG_REQUIRE(res.value() == 20);
}

COROSIG_SIGHANDLER_TEST_CASE("Fut move constructor transfers ownership") {
  auto foo = [](Reactor &) -> Fut<> { co_return Ok{}; };

  Fut<> fut1 = foo(reactor);
  COROSIG_REQUIRE(fut1.completed());
  Fut<> fut2{std::move(fut1)};
  COROSIG_REQUIRE(fut2.completed());
}

COROSIG_SIGHANDLER_TEST_CASE("Fut move constructor with nontrivial move") {
  using MoveType = SetDefaultOnMove<double, double(2 << 10)>;

  auto foo = [](Reactor &) -> Fut<MoveType> { co_return Ok{MoveType{123.0}}; };

  Fut<MoveType> fut1 = foo(reactor);
  COROSIG_REQUIRE(fut1.completed());

  Fut<MoveType> fut2{std::move(fut1)};
  COROSIG_REQUIRE(fut2.completed());
}

COROSIG_SIGHANDLER_TEST_CASE("Fut::block_on returns error when reactor fails") {
  Result res = Fut<>::promise_type::get_return_object_on_allocation_failure().block_on();
  COROSIG_REQUIRE(!res.is_ok());
  COROSIG_REQUIRE(res.error().holds<AllocationError>());
}

COROSIG_SIGHANDLER_TEST_CASE("Fut can be co_awaited inside a coroutine") {
  constexpr static auto FOO = [](Reactor &) -> Fut<int> { co_return 99; };
  constexpr static auto BAR = [](Reactor &r) -> Fut<int> {
    co_return 123 + (co_await FOO(r)).value();
  };

  auto result = BAR(reactor).block_on();
  REQUIRE(result.is_ok());
  REQUIRE(result.value() == 123 + 99);
}

COROSIG_SIGHANDLER_TEST_CASE("Fut destruction calls dtor for object on frame") {
  LifetimeCounter::reset();

  {
    auto coro = [](Reactor &) -> Fut<LifetimeCounter> {
      LifetimeCounter obj{42};
      co_return obj;
    };
    Fut<LifetimeCounter> fut = coro(reactor);
    COROSIG_REQUIRE(fut.completed());
    COROSIG_REQUIRE(LifetimeCounter::constructed == 1);
    COROSIG_REQUIRE(LifetimeCounter::destructed == 0);
  }

  COROSIG_REQUIRE(LifetimeCounter::constructed == 1);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 1);

  COROSIG_REQUIRE(!reactor.has_active_tasks());
}

COROSIG_SIGHANDLER_TEST_CASE("Fut discarding before coroutine finishes") {
  LifetimeCounter::reset();

  {
    auto coro = [](Reactor &) -> Fut<int> {
      LifetimeCounter obj1{100};
      LifetimeCounter obj2{200};
      co_await Yield{};
      LifetimeCounter obj3{300};
      co_await Yield{};
      co_return 999;
    };
    Fut<int> fut = coro(reactor);
    COROSIG_REQUIRE(!fut.completed());
    COROSIG_REQUIRE(LifetimeCounter::constructed == 2);
    COROSIG_REQUIRE(LifetimeCounter::destructed == 0);
  }

  COROSIG_REQUIRE(LifetimeCounter::constructed == 2);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 2);
  COROSIG_REQUIRE(!reactor.has_active_tasks());
}

COROSIG_SIGHANDLER_TEST_CASE("Fut discarding at sleep suspension point") {
  using namespace std::chrono_literals;

  LifetimeCounter::reset();

  {
    auto coro = [](Reactor &) -> Fut<int> {
      LifetimeCounter obj{111};
      co_await Sleep{5ms};
      LifetimeCounter obj2{222};
      co_return 0;
    };
    Fut<int> fut = coro(reactor);
    COROSIG_REQUIRE(LifetimeCounter::constructed == 1);
    COROSIG_REQUIRE(LifetimeCounter::destructed == 0);
  }

  COROSIG_REQUIRE(LifetimeCounter::constructed == 1);
  COROSIG_REQUIRE(LifetimeCounter::destructed == 1);

  COROSIG_REQUIRE(!reactor.has_active_tasks());
}
