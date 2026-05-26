#include "corosig/ErrorTypes.hpp"

#include "corosig/testing/Signals.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <unistd.h>

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("Extend error") {
  struct A1 {};
  struct A2 {};
  struct A3 {};
  struct A4 {};

  static_assert(std::same_as<extend_error<Error<A1, A2>, A3, Error<A4>>, Error<A1, A2, A3, A4>>);
  static_assert(std::same_as<extend_error<A1, A2, A3, A4>, Error<A1, A2, A3, A4>>);
  static_assert(std::same_as<extend_error<A1, A2, A3, A4, A4, A4>, Error<A1, A2, A3, A4>>);
  static_assert(
      std::same_as<extend_error<Error<A1, A2, A3>, A2, A3, A4, A4, A4>, Error<A1, A2, A3, A4>>);

  static_assert(std::same_as<extend_error<Error<A1, A2, A3>, A4, NoError>, Error<A1, A2, A3, A4>>);

  static_assert(std::same_as<extend_error<A1, A1>, A1>);
}

COROSIG_SIGHANDLER_TEST_CASE("SyscallError default construction") {
  SyscallError err;
  COROSIG_REQUIRE(err.value == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("SyscallError value construction") {
  SyscallError err1{42};
  COROSIG_REQUIRE(err1.value == 42);

  SyscallError err2{ENOENT};
  COROSIG_REQUIRE(err2.value == ENOENT);
}

COROSIG_SIGHANDLER_TEST_CASE("SyscallError captures EPERM errno") {
  errno = EPERM;
  SyscallError err = SyscallError::current();
  COROSIG_REQUIRE(err.value == EPERM);
}
