#include "corosig/Clock.hpp"

#include <ctime>

namespace corosig {

Clock::time_point Clock::now() noexcept {
  timespec ts;
  if (::clock_gettime(CLOCK_REALTIME, &ts) == -1) {
    return time_point{duration{-1}};
  }
  return time_point{duration{rep(ts.tv_sec) * period::den + rep(ts.tv_nsec)}};
}

} // namespace corosig
