#ifndef COROSIG_CLOCK_HPP
#define COROSIG_CLOCK_HPP

#include <chrono>

namespace corosig {

/// @brief std::chrono clock type which is guaranteed to be signal safe
struct Clock {
  using duration = std::chrono::nanoseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<Clock>;

  static constexpr bool is_steady = true;

  static time_point now() noexcept;
};

} // namespace corosig

#endif
