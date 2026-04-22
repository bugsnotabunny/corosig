#ifndef COROSIG_TESTING_TEST_ERROR_HPP
#define COROSIG_TESTING_TEST_ERROR_HPP

#include <catch2/catch_all.hpp>
#include <unistd.h>

namespace corosig {

struct TestError {
  constexpr auto operator<=>(TestError const &) const noexcept = default;

  int value = 1234;
};

} // namespace corosig

#endif
