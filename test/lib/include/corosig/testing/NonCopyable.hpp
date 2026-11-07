#ifndef COROSIG_TESTING_NON_COPYABLE_HPP
#define COROSIG_TESTING_NON_COPYABLE_HPP

#include <compare>

namespace corosig {

struct NonCopyable {
  NonCopyable(int v = -1234444)
      : value{v} {
  }

  NonCopyable(NonCopyable const &) = delete;
  NonCopyable(NonCopyable &&) noexcept = default;
  NonCopyable &operator=(NonCopyable const &) = delete;
  NonCopyable &operator=(NonCopyable &&) noexcept = default;

  constexpr std::strong_ordering operator<=>(NonCopyable const &) const noexcept = default;

  int value;
};

} // namespace corosig

#endif
