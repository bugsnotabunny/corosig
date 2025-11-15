#include "corosig/Yield.hpp"

#include "corosig/Coro.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch_all.hpp>

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("Coroutine yielded") {
  auto foo = [](Reactor &) -> Fut<int> {
    co_await Yield{};
    co_return 20;
  };

  auto res = foo(reactor).block_on();
  COROSIG_REQUIRE(res);
  COROSIG_REQUIRE(res.value() == 20);
}

COROSIG_SIGHANDLER_TEST_CASE("Coroutine awaits another coroutine") {
  constexpr static auto FOO = [](Reactor &) -> Fut<int> {
    co_await Yield{};
    co_return 20;
  };

  constexpr static auto BAR = [](Reactor &r) -> Fut<int> {
    co_return 20 + (co_await FOO(r)).value();
  };

  auto res = BAR(reactor).block_on();
  COROSIG_REQUIRE(res);
  COROSIG_REQUIRE(res.value() == 40);
}
