#ifndef COROSIG_TESTING_TEST_ERROR_HPP
#define COROSIG_TESTING_TEST_ERROR_HPP

#include <compare>

namespace corosig {

struct TestError {
  constexpr std::strong_ordering operator<=>(TestError const &) const noexcept = default;

  int value = 1234;
};

} // namespace corosig

#endif
