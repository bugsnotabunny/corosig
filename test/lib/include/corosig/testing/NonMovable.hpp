#ifndef COROSIG_TESTING_NON_MOVABLE_HPP
#define COROSIG_TESTING_NON_MOVABLE_HPP

#include <compare>

namespace corosig {

struct NonMovable {
  NonMovable(int v = 482) noexcept
      : value{v} {
  }

  NonMovable(NonMovable const &) = delete;
  NonMovable(NonMovable &&) = delete;
  NonMovable &operator=(NonMovable const &) = delete;
  NonMovable &operator=(NonMovable &&) = delete;

  constexpr std::strong_ordering operator<=>(NonMovable const &) const noexcept = default;

  int value;
};

} // namespace corosig

#endif
