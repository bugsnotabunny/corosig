#ifndef COROSIG_TESTING_NON_COPYABLE_HPP
#define COROSIG_TESTING_NON_COPYABLE_HPP

#include <catch2/catch_all.hpp>
#include <unistd.h>

namespace corosig {

struct NonCopyable {
  NonCopyable(int v = -1234444)
      : value{v} {
  }

  NonCopyable(NonCopyable const &) = delete;
  NonCopyable(NonCopyable &&) noexcept = default;
  NonCopyable &operator=(NonCopyable const &) = delete;
  NonCopyable &operator=(NonCopyable &&) noexcept = default;

  constexpr auto operator<=>(NonCopyable const &) const noexcept = default;

  int value;
};

} // namespace corosig

#endif
