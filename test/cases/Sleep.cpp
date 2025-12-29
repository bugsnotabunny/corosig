#include "corosig/Sleep.hpp"

#include "corosig/Clock.hpp"
#include "corosig/Coro.hpp"
#include "corosig/Parallel.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

namespace {

using namespace corosig;
using namespace std::chrono_literals;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Sleep is ok") {
  auto start = Clock::now();
  auto foo = [](Reactor &) -> Fut<int> {
    co_await Sleep{10ms};
    co_return 20;
  };
  auto res = foo(reactor).block_on();
  COROSIG_REQUIRE(res);
  COROSIG_REQUIRE(res.value() == 20);
  COROSIG_REQUIRE(Clock::now() - start >= 10ms);
}

COROSIG_SIGHANDLER_TEST_CASE("Parallel sleep is ok") { // NOLINT
  constexpr static auto FOO = [](Reactor &) -> Fut<void> {
    co_await Sleep{10ms};
    co_return Ok{};
  };

  constexpr static auto FOO2 = [](Reactor &r) -> Fut<void> {
    COROSIG_CO_TRYV(co_await when_all_succeed(r, FOO(r), FOO(r), FOO(r), FOO(r)));
    co_return Ok{};
  };

  auto res = FOO2(reactor).block_on();
  COROSIG_REQUIRE(res);
}
