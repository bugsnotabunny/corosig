#include "corosig/Clock.hpp"

#include "corosig/testing/Signals.hpp"

#include <chrono>
#include <concepts>
#include <ranges>

namespace {

using namespace corosig;
using namespace std::chrono_literals;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Type aliases are correctly defined") {
  static_assert(std::same_as<Clock::duration, std::chrono::nanoseconds>);
  static_assert(std::same_as<Clock::rep, std::chrono::nanoseconds::rep>);
  static_assert(std::same_as<Clock::period, std::chrono::nanoseconds::period>);
  static_assert(std::same_as<Clock::time_point, std::chrono::time_point<Clock>>);
}

COROSIG_SIGHANDLER_TEST_CASE("Static properties") {
  static_assert(Clock::is_steady);
}

COROSIG_SIGHANDLER_TEST_CASE("Clock meets chrono clock requirements") {
  static_assert(noexcept(Clock::now()));
  static_assert(std::is_same_v<decltype(Clock::now()), Clock::time_point>);
  static_assert(std::is_same_v<decltype(Clock::now())::clock, Clock>);
}

COROSIG_SIGHANDLER_TEST_CASE("now() returns valid time points") {
  auto t1 = Clock::now();
  COROSIG_REQUIRE(t1.time_since_epoch().count() >= 0);

  auto t2 = Clock::now();
  COROSIG_REQUIRE(t2.time_since_epoch().count() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("Clock is monotonic") {
  constexpr auto ITERATIONS = 100;
  std::array<Clock::time_point, ITERATIONS> time_points;

  for (auto i : std::views::iota(0, ITERATIONS)) {
    time_points[i] = Clock::now();
  }

  for (size_t i = 1; i < time_points.size(); ++i) {
    COROSIG_REQUIRE(time_points[i] >= time_points[i - 1]);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Time point arithmetic") {
  auto now = Clock::now();
  auto one_sec = std::chrono::seconds(1);

  auto future = now + one_sec;
  auto past = now - one_sec;

  COROSIG_REQUIRE(future > now);
  COROSIG_REQUIRE(past < now);
  COROSIG_REQUIRE(future - now == std::chrono::seconds(1));
  COROSIG_REQUIRE(now - past == std::chrono::seconds(1));
}
