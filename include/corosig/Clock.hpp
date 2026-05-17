#ifndef COROSIG_CLOCK_HPP
#define COROSIG_CLOCK_HPP

#include <chrono>

namespace corosig {

/// @brief std::chrono::steady_clock type which is guaranteed to be signal safe
struct SteadyClock {
  using duration = std::chrono::nanoseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<SteadyClock>;

  static constexpr bool is_steady = true; // NOLINT (readability-identifier-naming)

  static time_point now() noexcept;
};

/// @brief std::chrono::system_clock type which is guaranteed to be signal safe
struct SystemClock {
  using duration = std::chrono::nanoseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<SystemClock>;

  static constexpr bool is_steady = false; // NOLINT (readability-identifier-naming)

  static time_point now() noexcept;
};

} // namespace corosig

#endif
