#include "corosig/Clock.hpp"

#include "corosig/testing/Signals.hpp"

#include <chrono>
#include <ranges>

namespace {

using namespace corosig;
using namespace std::chrono_literals;

static_assert(SteadyClock::is_steady);
static_assert(!SystemClock::is_steady);

static_assert(noexcept(SteadyClock::now()));
static_assert(std::is_same_v<decltype(SteadyClock::now()), SteadyClock::time_point>);
static_assert(std::is_same_v<decltype(SteadyClock::now())::clock, SteadyClock>);

static_assert(noexcept(SystemClock::now()));
static_assert(std::is_same_v<decltype(SystemClock::now()), SystemClock::time_point>);
static_assert(std::is_same_v<decltype(SystemClock::now())::clock, SystemClock>);

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("now() returns valid time points") {
  auto t1 = SteadyClock::now();
  COROSIG_REQUIRE(t1.time_since_epoch().count() >= 0);

  auto t2 = SteadyClock::now();
  COROSIG_REQUIRE(t2.time_since_epoch().count() >= 0);

  auto t3 = SystemClock::now();
  COROSIG_REQUIRE(t3.time_since_epoch().count() >= 0);

  auto t4 = SystemClock::now();
  COROSIG_REQUIRE(t4.time_since_epoch().count() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("SteadyClock is monotonic") {
  constexpr auto ITERATIONS = 100;
  std::array<SteadyClock::time_point, ITERATIONS> time_points;

  for (auto i : std::views::iota(0, ITERATIONS)) {
    time_points[i] = SteadyClock::now();
  }

  for (size_t i = 1; i < time_points.size(); ++i) {
    COROSIG_REQUIRE(time_points[i] >= time_points[i - 1]);
  }
}
