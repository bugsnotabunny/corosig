#include "corosig/Clock.hpp"

#include <chrono>
#include <ctime>

namespace {

using duration = std::chrono::nanoseconds;
using rep = duration::rep;
using period = duration::period;

duration clock_gettime_ns(int clock) {
  timespec ts;
  if (::clock_gettime(clock, &ts) == -1) {
    return duration{-1};
  }
  return duration{rep(ts.tv_sec) * period::den / period::num + rep(ts.tv_nsec)};
}

} // namespace

namespace corosig {

SteadyClock::time_point SteadyClock::now() noexcept {
  return time_point{clock_gettime_ns(CLOCK_MONOTONIC)};
}

SystemClock::time_point SystemClock::now() noexcept {
  return time_point{clock_gettime_ns(CLOCK_REALTIME)};
}

} // namespace corosig
