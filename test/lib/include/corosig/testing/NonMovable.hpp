#ifndef COROSIG_TESTING_NON_MOVABLE_HPP
#define COROSIG_TESTING_NON_MOVABLE_HPP

#include <catch2/catch_all.hpp>
#include <unistd.h>

namespace corosig {

struct NonMovable {
  NonMovable(int v = 482) noexcept
      : value{v} {
  }

  NonMovable(const NonMovable &) = delete;
  NonMovable(NonMovable &&) = delete;
  NonMovable &operator=(const NonMovable &) = delete;
  NonMovable &operator=(NonMovable &&) = delete;

  constexpr auto operator<=>(NonMovable const &) const noexcept = default;

  int value;
};

} // namespace corosig

#endif
