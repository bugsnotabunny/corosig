#include "corosig/Result.hpp"

#include "corosig/testing/NonCopyable.hpp"
#include "corosig/testing/NonMovable.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <type_traits>

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Construct with value") {
  Result<int, std::string_view> r{42};
  COROSIG_REQUIRE(r.is_ok());
  COROSIG_REQUIRE(!r.is_nothing());
  COROSIG_REQUIRE(r.value() == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Construct with value using direct initialization") {
  Result<int, std::string_view> r{42};
  COROSIG_REQUIRE(r.is_ok());
  COROSIG_REQUIRE(r.value() == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Construct with void value") {
  Result<void, std::string_view> r = Ok();
  COROSIG_REQUIRE(r.is_ok());
  COROSIG_REQUIRE(!r.is_nothing());
}

COROSIG_SIGHANDLER_TEST_CASE("Construct with error") {
  Result<int, std::string_view> r = Failure(std::string_view("error message"));
  COROSIG_REQUIRE(!r.is_ok());
  COROSIG_REQUIRE(!r.is_nothing());
  COROSIG_REQUIRE(r.error() == "error message");
}

COROSIG_SIGHANDLER_TEST_CASE("Construct with different error type") {
  Result<int, std::string_view> r = Failure("C-string error");
  COROSIG_REQUIRE(!r.is_ok());
  COROSIG_REQUIRE(r.error() == "C-string error");
}

COROSIG_SIGHANDLER_TEST_CASE("Default constructed result is nothing") {
  Result<int, std::string_view> r;
  COROSIG_REQUIRE(r.is_nothing());
  COROSIG_REQUIRE(!r.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Result holding reference to value") {
  int value = 42;
  Result<int &, std::string_view> r{std::ref(value)};
  COROSIG_REQUIRE(r.is_ok());
  COROSIG_REQUIRE(r.value() == 42);

  value = 100;
  COROSIG_REQUIRE(r.value() == 100);
}

COROSIG_SIGHANDLER_TEST_CASE("Result holding const reference") {
  const int value = 42;
  Result<const int &, std::string_view> r{std::cref(value)};
  COROSIG_REQUIRE(r.is_ok());
  COROSIG_REQUIRE(r.value() == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Result with error type same as value is operable") {
  Result<int, int> r1{Failure{1234}};
  COROSIG_REQUIRE(!r1.is_ok());
  COROSIG_REQUIRE(r1.error() == 1234);
}

COROSIG_SIGHANDLER_TEST_CASE("Convert between compatible result types") {
  Result<int, std::string_view> r1{42};
  Result<unsigned, std::string_view> r2{std::move(r1)}; // NOLINT
  COROSIG_REQUIRE(r2.is_ok());
  COROSIG_REQUIRE(r2.value() == 42.0);
}

COROSIG_SIGHANDLER_TEST_CASE("Convert error between compatible types") {
  Result<int, std::string_view> r1{Failure{std::string_view{"error"}}};
  Result<double, std::string_view> r2{std::move(r1)}; // NOLINT
  COROSIG_REQUIRE(!r2.is_ok());
  COROSIG_REQUIRE(std::string_view(r2.error()) == "error");
}

COROSIG_SIGHANDLER_TEST_CASE("Access value from non-const lvalue") {
  Result<int, std::string_view> r = Ok(42);
  COROSIG_REQUIRE(r.value() == 42);
  r.value() = 100;
  COROSIG_REQUIRE(r.value() == 100);
}

COROSIG_SIGHANDLER_TEST_CASE("Access value from const lvalue") {
  const Result<int, std::string_view> r = Ok(42);
  COROSIG_REQUIRE(r.value() == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Access value from rvalue") {
  Result<int, std::string_view> r = Ok(42);
  int value = std::move(r).value();
  COROSIG_REQUIRE(value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Access void value does nothing") {
  Result<void, std::string_view> r = Ok();
  r.value(); // Should compile and do nothing
}

COROSIG_SIGHANDLER_TEST_CASE("Access error from non-const lvalue") {
  Result<int, std::string_view> r = Failure(std::string_view("error"));
  COROSIG_REQUIRE(r.error() == "error");
  r.error() = "new error";
  COROSIG_REQUIRE(r.error() == "new error");
}

COROSIG_SIGHANDLER_TEST_CASE("Access error from const lvalue") {
  const Result<int, std::string_view> r = Failure(std::string_view("error"));
  COROSIG_REQUIRE(r.error() == "error");
}

COROSIG_SIGHANDLER_TEST_CASE("Access error from rvalue") {
  Result<int, std::string_view> r = Failure(std::string_view("error"));
  std::string_view error = std::move(r).error();
  COROSIG_REQUIRE(error == "error");
}

COROSIG_SIGHANDLER_TEST_CASE("Ok result converts to true") {
  Result<int, std::string_view> r = Ok(42);
  COROSIG_REQUIRE(static_cast<bool>(r) == true);
}

COROSIG_SIGHANDLER_TEST_CASE("Error result converts to false") {
  Result<int, std::string_view> r = Failure("error");
  COROSIG_REQUIRE(static_cast<bool>(r) == false);
}

COROSIG_SIGHANDLER_TEST_CASE("Nothing result converts to false") {
  Result<int, std::string_view> r;
  COROSIG_REQUIRE(static_cast<bool>(r) == false);
}

COROSIG_SIGHANDLER_TEST_CASE("Ok with non-copyable") {
  Result<NonCopyable, std::string_view> r = Ok(NonCopyable(42));
  COROSIG_REQUIRE(r.is_ok());
  COROSIG_REQUIRE(r.value().value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Error with non-copyable") {
  Result<int, NonCopyable> r = Failure(NonCopyable(42));
  COROSIG_REQUIRE(!r.is_ok());
  COROSIG_REQUIRE(r.error().value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Move non-copyable from result") {
  Result<NonCopyable, std::string_view> r = Ok(NonCopyable(42));
  NonCopyable val = std::move(r).value();
  COROSIG_REQUIRE(val.value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Ok with non-movable") {
  Result<NonMovable, std::string_view> r{Ok(42)};
  COROSIG_REQUIRE(r.is_ok());
  COROSIG_REQUIRE(r.value().value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Error with non-movable") {
  Result<int, NonMovable> r = Failure(42);
  COROSIG_REQUIRE(!r.is_ok());
  COROSIG_REQUIRE(r.error().value == 42);
}
