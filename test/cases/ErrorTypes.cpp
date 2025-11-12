#include "corosig/ErrorTypes.hpp"

#include <catch2/catch_all.hpp>

using namespace corosig;

struct a1 {};
struct a2 {};
struct a3 {};
struct a4 {};

TEST_CASE("Extend error") {
  static_assert(std::same_as<extend_error<Error<a1, a2>, a3, Error<a4>>, Error<a1, a2, a3, a4>>);
  static_assert(std::same_as<extend_error<a1, a2, a3, a4>, Error<a1, a2, a3, a4>>);
  static_assert(std::same_as<extend_error<a1, a2, a3, a4, a4, a4>, Error<a1, a2, a3, a4>>);
  static_assert(
      std::same_as<extend_error<Error<a1, a2, a3>, a2, a3, a4, a4, a4>, Error<a1, a2, a3, a4>>);
}
