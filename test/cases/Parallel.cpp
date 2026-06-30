#include "corosig/Parallel.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch_all.hpp>
#include <string>
#include <type_traits>
#include <variant>

namespace {

using namespace corosig;

struct IntAwaiter {
  int value;
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  int await_resume() const noexcept {
    return value;
  }
};

struct VoidAwaiter {
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  void await_resume() const noexcept {
  }
};

struct AsyncIntAwaiter {
  int value;
  int ready_after = 0;
  int counter = 0;

  bool await_ready() const noexcept {
    return counter >= ready_after;
  }
  void await_suspend(std::coroutine_handle<> h) noexcept {
    counter++;
    h.resume();
  }
  int await_resume() const noexcept {
    return value;
  }
};

struct OkIntResultAwaiter {
  int value;
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  Result<int, AllocationError> await_resume() const noexcept {
    return Ok{value};
  }
};

struct ErrorIntResultAwaiter {
  AllocationError error;
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  Result<int, AllocationError> await_resume() const noexcept {
    return Failure{error};
  }
};

struct OkStringResultAwaiter {
  std::string_view value;
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  Result<std::string_view, AllocationError> await_resume() const noexcept {
    return Ok{value};
  }
};

struct ErrorStringResultAwaiter {
  AllocationError error;
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  Result<std::string_view, AllocationError> await_resume() const noexcept {
    return Failure{error};
  }
};

struct OkVoidResultAwaiter {
  bool await_ready() const noexcept {
    return true;
  }

  void await_suspend(std::coroutine_handle<>) const noexcept {
  }

  Result<void, AllocationError> await_resume() const noexcept {
    return Ok{};
  }
};

struct ErrorVoidResultAwaiter {
  AllocationError error;
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  Result<void, AllocationError> await_resume() const noexcept {
    return Failure{error};
  }
};

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("when_all: single awaiter returns correct result") {
  IntAwaiter awaiter{42};
  auto result = when_all(reactor, std::move(awaiter)).block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [value] = result.value();
  COROSIG_REQUIRE(value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all: multiple awaiters return correct results") {
  IntAwaiter awaiter1{1};
  IntAwaiter awaiter2{2};
  IntAwaiter awaiter3{3};

  auto result = when_all(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3))
                    .block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [a, b, c] = result.value();
  COROSIG_REQUIRE(a == 1);
  COROSIG_REQUIRE(b == 2);
  COROSIG_REQUIRE(c == 3);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all: mixed types return correct results") {
  IntAwaiter int_awaiter{42};
  VoidAwaiter void_awaiter;

  auto result = when_all(reactor, std::move(int_awaiter), std::move(void_awaiter))
                    .block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [int_val, void_val] = result.value();
  COROSIG_REQUIRE(int_val == 42);
  static_assert(std::same_as<decltype(int_val), int>);
  static_assert(std::same_as<decltype(void_val), std::monostate>);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all: void awaiter returns monostate") {
  VoidAwaiter awaiter;
  auto result = when_all(reactor, std::move(awaiter)).block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [value] = result.value();
  COROSIG_REQUIRE(value == std::monostate{});
}

COROSIG_SIGHANDLER_TEST_CASE("when_all: handles async awaiters") {
  AsyncIntAwaiter awaiter1{10, 1};
  AsyncIntAwaiter awaiter2{20, 1};

  auto result =
      when_all(reactor, std::move(awaiter1), std::move(awaiter2)).block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [a, b] = result.value();
  COROSIG_REQUIRE(a == 10);
  COROSIG_REQUIRE(b == 20);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: single successful result") {
  OkIntResultAwaiter awaiter{42};
  auto result = when_all_succeed(reactor, std::move(awaiter)).block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [value] = result.value();
  COROSIG_REQUIRE(value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: single error result returns error") {
  ErrorIntResultAwaiter awaiter{AllocationError{}};
  auto result = when_all_succeed(reactor, std::move(awaiter)).block_on_with_reactor_drain();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: multiple successful results return all values") {
  OkIntResultAwaiter awaiter1{1};
  OkIntResultAwaiter awaiter2{2};
  OkStringResultAwaiter awaiter3{"test"};

  auto result =
      when_all_succeed(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3))
          .block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [v1, v2, v3] = result.value();
  COROSIG_REQUIRE(v1 == 1);
  COROSIG_REQUIRE(v2 == 2);
  COROSIG_REQUIRE(v3 == "test");
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: first error in sequence is returned") {
  OkIntResultAwaiter awaiter1{1};
  ErrorIntResultAwaiter awaiter2{AllocationError{}};
  OkStringResultAwaiter awaiter3{"test"};

  auto result =
      when_all_succeed(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3))
          .block_on_with_reactor_drain();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: error at beginning is returned first") {
  ErrorIntResultAwaiter awaiter1{AllocationError{}};
  OkIntResultAwaiter awaiter2{2};
  OkStringResultAwaiter awaiter3{"test"};

  auto result =
      when_all_succeed(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3))
          .block_on_with_reactor_drain();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: multiple errors return first encountered") {
  ErrorIntResultAwaiter awaiter1{AllocationError{}};
  ErrorStringResultAwaiter awaiter2{AllocationError{}};
  OkIntResultAwaiter awaiter3{3};

  auto result =
      when_all_succeed(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3))
          .block_on_with_reactor_drain();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: void result returns monostate in tuple") {
  OkVoidResultAwaiter awaiter;
  auto result = when_all_succeed(reactor, std::move(awaiter)).block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [value] = result.value();
  COROSIG_REQUIRE(value == std::monostate{});
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: void error returns error") {
  ErrorVoidResultAwaiter awaiter{AllocationError{}};
  auto result = when_all_succeed(reactor, std::move(awaiter)).block_on_with_reactor_drain();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: mixed void and non-void results") {
  OkVoidResultAwaiter void_awaiter;
  OkIntResultAwaiter int_awaiter{42};
  OkStringResultAwaiter string_awaiter{"hello"};

  auto result =
      when_all_succeed(
          reactor, std::move(void_awaiter), std::move(int_awaiter), std::move(string_awaiter))
          .block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [void_val, int_val, str_val] = result.value();
  COROSIG_REQUIRE(void_val == std::monostate{});
  COROSIG_REQUIRE(int_val == 42);
  COROSIG_REQUIRE(str_val == "hello");
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: error in mixed void/non-void sequence") {
  OkVoidResultAwaiter void_awaiter;
  ErrorIntResultAwaiter int_awaiter{AllocationError{}};
  OkStringResultAwaiter string_awaiter{"hello"};

  auto result =
      when_all_succeed(
          reactor, std::move(void_awaiter), std::move(int_awaiter), std::move(string_awaiter))
          .block_on_with_reactor_drain();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: properly unwraps Result types") {
  OkIntResultAwaiter int_awaiter{100};
  OkStringResultAwaiter string_awaiter{"success"};

  auto result = when_all_succeed(reactor, std::move(int_awaiter), std::move(string_awaiter))
                    .block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [int_val, str_val] = result.value();
  COROSIG_REQUIRE(int_val == 100);
  COROSIG_REQUIRE(str_val == "success");

  static_assert(std::same_as<decltype(int_val), int>);
  static_assert(std::same_as<decltype(str_val), std::string_view>);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_vs_when_all_succeed: when_all preserves Result types") {
  OkIntResultAwaiter awaiter{50};

  auto when_all_result = when_all(reactor, std::move(awaiter)).block_on_with_reactor_drain();
  COROSIG_REQUIRE(when_all_result.is_ok());
  auto [value] = when_all_result.value();
  COROSIG_REQUIRE(value.is_ok());
  COROSIG_REQUIRE(value.value() == 50);

  static_assert(std::same_as<decltype(value), Result<int, AllocationError>>);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: handle large number of awaitables") {
  OkIntResultAwaiter awaiter1{1};
  OkIntResultAwaiter awaiter2{2};
  OkIntResultAwaiter awaiter3{3};
  OkIntResultAwaiter awaiter4{4};
  OkIntResultAwaiter awaiter5{5};

  auto result = when_all_succeed(reactor,
                                 std::move(awaiter1),
                                 std::move(awaiter2),
                                 std::move(awaiter3),
                                 std::move(awaiter4),
                                 std::move(awaiter5))
                    .block_on_with_reactor_drain();
  COROSIG_REQUIRE(result.is_ok());
  auto [v1, v2, v3, v4, v5] = result.value();
  COROSIG_REQUIRE(v1 == 1);
  COROSIG_REQUIRE(v2 == 2);
  COROSIG_REQUIRE(v3 == 3);
  COROSIG_REQUIRE(v4 == 4);
  COROSIG_REQUIRE(v5 == 5);
}
