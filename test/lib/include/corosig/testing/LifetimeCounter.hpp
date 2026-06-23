#ifndef COROSIG_TESTING_LIFETIME_COUNTER_HPP
#define COROSIG_TESTING_LIFETIME_COUNTER_HPP

#include "corosig/util/SetDefaultOnMove.hpp"

namespace corosig::testing {

// Type that tracks construction/destruction for testing
struct LifetimeCounter {
  static inline int constructed = 0;
  static inline int destructed = 0;
  int value;
  SetDefaultOnMove<bool, false> owned = true;

  LifetimeCounter(int v = 1234) noexcept
      : value(v) {
    constructed++;
  }

  LifetimeCounter(LifetimeCounter const &rhs) noexcept
      : value{rhs.value},
        owned{*rhs.owned} {
    if (*owned) {
      constructed++;
    }
  }

  LifetimeCounter(LifetimeCounter &&) noexcept = default;

  ~LifetimeCounter() {
    if (*owned) {
      destructed++;
    }
  }

  static void reset() {
    constructed = 0;
    destructed = 0;
  }

  bool operator==(int rhs) {
    return value == rhs;
  }
};

} // namespace corosig::testing

#endif
