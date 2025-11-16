#include "corosig/ErrorTypes.hpp"

#include <catch2/catch_all.hpp>

using namespace corosig;

struct A1 {};
struct A2 {};
struct A3 {};
struct A4 {};

TEST_CASE("Extend error") {
  static_assert(std::same_as<extend_error<Error<A1, A2>, A3, Error<A4>>, Error<A1, A2, A3, A4>>);
  static_assert(std::same_as<extend_error<A1, A2, A3, A4>, Error<A1, A2, A3, A4>>);
  static_assert(std::same_as<extend_error<A1, A2, A3, A4, A4, A4>, Error<A1, A2, A3, A4>>);
  static_assert(
      std::same_as<extend_error<Error<A1, A2, A3>, A2, A3, A4, A4, A4>, Error<A1, A2, A3, A4>>);
}
