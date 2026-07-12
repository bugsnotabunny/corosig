#include "corosig/Parallel.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch_all.hpp>
#include <list>
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
  auto result = when_all(reactor, std::move(awaiter)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [value] = result.value();
  COROSIG_REQUIRE(value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all: multiple awaiters return correct results") {
  IntAwaiter awaiter1{1};
  IntAwaiter awaiter2{2};
  IntAwaiter awaiter3{3};

  auto result =
      when_all(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [a, b, c] = result.value();
  COROSIG_REQUIRE(a == 1);
  COROSIG_REQUIRE(b == 2);
  COROSIG_REQUIRE(c == 3);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all: mixed types return correct results") {
  IntAwaiter int_awaiter{42};
  VoidAwaiter void_awaiter;

  auto result = when_all(reactor, std::move(int_awaiter), std::move(void_awaiter)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [int_val, void_val] = result.value();
  COROSIG_REQUIRE(int_val == 42);
  static_assert(std::same_as<decltype(int_val), int>);
  static_assert(std::same_as<decltype(void_val), std::monostate>);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all: void awaiter returns monostate") {
  VoidAwaiter awaiter;
  auto result = when_all(reactor, std::move(awaiter)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [value] = result.value();
  COROSIG_REQUIRE(value == std::monostate{});
}

COROSIG_SIGHANDLER_TEST_CASE("when_all: handles async awaiters") {
  AsyncIntAwaiter awaiter1{10, 1};
  AsyncIntAwaiter awaiter2{20, 1};

  auto result = when_all(reactor, std::move(awaiter1), std::move(awaiter2)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [a, b] = result.value();
  COROSIG_REQUIRE(a == 10);
  COROSIG_REQUIRE(b == 20);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: single successful result") {
  OkIntResultAwaiter awaiter{42};
  auto result = when_all_succeed(reactor, std::move(awaiter)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [value] = result.value();
  COROSIG_REQUIRE(value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: single error result returns error") {
  ErrorIntResultAwaiter awaiter{AllocationError{}};
  auto result = when_all_succeed(reactor, std::move(awaiter)).block_on();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: multiple successful results return all values") {
  OkIntResultAwaiter awaiter1{1};
  OkIntResultAwaiter awaiter2{2};
  OkStringResultAwaiter awaiter3{"test"};

  auto result =
      when_all_succeed(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3))
          .block_on();
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
          .block_on();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: error at beginning is returned first") {
  ErrorIntResultAwaiter awaiter1{AllocationError{}};
  OkIntResultAwaiter awaiter2{2};
  OkStringResultAwaiter awaiter3{"test"};

  auto result =
      when_all_succeed(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3))
          .block_on();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: multiple errors return first encountered") {
  ErrorIntResultAwaiter awaiter1{AllocationError{}};
  ErrorStringResultAwaiter awaiter2{AllocationError{}};
  OkIntResultAwaiter awaiter3{3};

  auto result =
      when_all_succeed(reactor, std::move(awaiter1), std::move(awaiter2), std::move(awaiter3))
          .block_on();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: void result returns monostate in tuple") {
  OkVoidResultAwaiter awaiter;
  auto result = when_all_succeed(reactor, std::move(awaiter)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [value] = result.value();
  COROSIG_REQUIRE(value == std::monostate{});
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: void error returns error") {
  ErrorVoidResultAwaiter awaiter{AllocationError{}};
  auto result = when_all_succeed(reactor, std::move(awaiter)).block_on();
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
          .block_on();
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
          .block_on();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(std::holds_alternative<AllocationError>(result.error()));
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed: properly unwraps Result types") {
  OkIntResultAwaiter int_awaiter{100};
  OkStringResultAwaiter string_awaiter{"success"};

  auto result =
      when_all_succeed(reactor, std::move(int_awaiter), std::move(string_awaiter)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [int_val, str_val] = result.value();
  COROSIG_REQUIRE(int_val == 100);
  COROSIG_REQUIRE(str_val == "success");

  static_assert(std::same_as<decltype(int_val), int>);
  static_assert(std::same_as<decltype(str_val), std::string_view>);
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_vs_when_all_succeed: when_all preserves Result types") {
  OkIntResultAwaiter awaiter{50};

  auto when_all_result = when_all(reactor, std::move(awaiter)).block_on();
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
                    .block_on();
  COROSIG_REQUIRE(result.is_ok());
  auto [v1, v2, v3, v4, v5] = result.value();
  COROSIG_REQUIRE(v1 == 1);
  COROSIG_REQUIRE(v2 == 2);
  COROSIG_REQUIRE(v3 == 3);
  COROSIG_REQUIRE(v4 == 4);
  COROSIG_REQUIRE(v5 == 5);
}

namespace {

using namespace corosig;
using namespace std::chrono_literals;

struct ImmediateIntAwaiter {
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

struct ImmediateVoidAwaiter {
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  void await_resume() const noexcept {
  }
};

struct ImmediateResultAwaiter {
  Result<int, AllocationError> value;
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  Result<int, AllocationError> await_resume() const noexcept {
    return value;
  }
};

template <AnAwaitable AWAITABLE>
Fut<typename AwaitResult<AWAITABLE>::ok_type,
    extend_error<AllocationError, typename AwaitResult<AWAITABLE>::failure_type>>
as_future(Reactor &, AWAITABLE &&awaitable) noexcept {
  co_return co_await std::forward<AWAITABLE>(awaitable);
}

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: immediate completion before deadline with int") {
  ImmediateIntAwaiter awaiter{42};
  auto deadline = 1s;

  auto result = as_future(reactor, with_deadline(reactor, std::move(awaiter), deadline)).block_on();

  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(result.value() == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: immediate completion before deadline with void") {
  ImmediateVoidAwaiter awaiter;
  auto deadline = 1s;

  auto result = as_future(reactor, with_deadline(reactor, std::move(awaiter), deadline)).block_on();
  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: immediate completion returns Result") {
  ImmediateResultAwaiter awaiter{Ok{99}};
  auto deadline = 1s;

  auto result = as_future(reactor, with_deadline(reactor, std::move(awaiter), deadline)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(result.value().is_ok());
  COROSIG_REQUIRE(result.value().value() == 99);
}

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: immediate completion with Result error") {
  ImmediateResultAwaiter awaiter{Failure{AllocationError{}}};
  auto deadline = 1s;

  auto result = as_future(reactor, with_deadline(reactor, std::move(awaiter), deadline)).block_on();

  COROSIG_REQUIRE(result.is_ok());

  COROSIG_REQUIRE(!result.value().is_ok());
  COROSIG_REQUIRE(result.value().error() == AllocationError{});
}

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: sleeping awaitable completes before deadline") {
  auto sleeping_coro = [](Reactor &) -> Fut<int> {
    co_await Sleep{5ms};
    co_return 100;
  };

  auto deadline = 20ms;

  auto result =
      as_future(reactor, with_deadline(reactor, sleeping_coro(reactor), deadline)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(result.value().is_ok());
  COROSIG_REQUIRE(result.value().value() == 100);
}

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: sleeping awaitable completes exactly at deadline") {
  auto sleeping_coro = [](Reactor &) -> Fut<int> {
    co_await Sleep{10ms};
    co_return 200;
  };

  auto deadline = 10ms;

  auto result =
      as_future(reactor, with_deadline(reactor, sleeping_coro(reactor), deadline)).block_on();

  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(result.value().is_ok());
  COROSIG_REQUIRE(result.value().value() == 200);
}

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: sleeping awaitable times out") {
  auto sleeping_coro = [](Reactor &) -> Fut<int> {
    co_await Sleep{50ms};
    co_return 300;
  };

  auto deadline = 10ms;

  auto result =
      as_future(reactor, with_deadline(reactor, sleeping_coro(reactor), deadline)).block_on();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(result.error().holds<TimedOutError>());
}

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: very short deadline times out immediately") {
  auto sleeping_coro = [](Reactor &) -> Fut<int> {
    co_await Sleep{100ms};
    co_return 600;
  };

  auto deadline = SteadyClock::now();

  auto result =
      as_future(reactor, with_deadline(reactor, sleeping_coro(reactor), deadline)).block_on();
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(result.error().holds<TimedOutError>());
}

COROSIG_SIGHANDLER_TEST_CASE("with_deadline: very long deadline allows completion") {
  auto sleeping_coro = [](Reactor &) -> Fut<int> {
    co_await Sleep{10ms};
    co_return 700;
  };

  auto deadline = 1h;

  auto result =
      as_future(reactor, with_deadline(reactor, sleeping_coro(reactor), deadline)).block_on();
  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(result.value().is_ok());
  COROSIG_REQUIRE(result.value().value() == 700);
}

namespace {

using namespace corosig;
using namespace std::chrono_literals;

struct ImmediateVoidResultAwaiter {
  Result<void, AllocationError> value;
  bool await_ready() const noexcept {
    return true;
  }
  void await_suspend(std::coroutine_handle<>) const noexcept {
  }
  Result<void, AllocationError> await_resume() const noexcept {
    return value;
  }
};

Fut<void, AllocationError> trivial_void_task(Reactor &) {
  co_return Ok{};
}

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: empty range succeeds") {
  std::vector<int> empty;

  auto result = parallel_foreach(reactor, empty, [](Reactor &, int) -> Fut<void, AllocationError> {
                  co_return Ok{};
                }).block_on();

  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: single element range succeeds") {
  std::array<int, 1> values{1};

  auto result = parallel_foreach(reactor, values, [](Reactor &, int) -> Fut<void, AllocationError> {
                  co_return Ok{};
                }).block_on();

  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: multiple elements range succeeds") {
  std::array<int, 5> values{1, 2, 3, 4, 5};

  auto result = parallel_foreach(reactor, values, [](Reactor &, int) -> Fut<void, AllocationError> {
                  co_return Ok{};
                }).block_on();

  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: uses provided tasks") {
  std::array<int, 3> values{1, 2, 3};

  auto result =
      parallel_foreach(reactor,
                       values,
                       [&, i = 0](Reactor &r, int val) mutable -> Fut<void, AllocationError> {
                         COROSIG_REQUIRE(values[i] == val);
                         ++i;
                         return trivial_void_task(r);
                       })
          .block_on();

  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: works with sized range") {
  auto result =
      parallel_foreach(reactor,
                       std::to_array({1, 2, 3}),
                       [](Reactor &, int) -> Fut<void, AllocationError> { co_return Ok{}; })
          .block_on();

  COROSIG_REQUIRE(result.is_ok());
}

TEST_CASE("parallel_foreach: works with unsized range") {
  static std::list<int> values{1, 2, 3};

  run_in_sighandler([](Reactor &reactor) {
    auto result =
        parallel_foreach(reactor, values, [](Reactor &, int) -> Fut<void, AllocationError> {
          co_return Ok{};
        }).block_on();
    COROSIG_REQUIRE(result.is_ok());
  });
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: tasks execute concurrently") {
  auto start = SteadyClock::now();
  std::array<int, 3> values{1, 2, 3};

  auto result =
      parallel_foreach(reactor, values, [](Reactor &r, int) -> Fut<void, AllocationError> {
        co_await Sleep{10ms};
        co_return Ok{};
      }).block_on();

  auto duration = SteadyClock::now() - start;

  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(duration >= 10ms);
  COROSIG_REQUIRE(duration < 20ms);
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: tasks with different completion times") {
  auto start = SteadyClock::now();
  std::array<int, 3> delays{5, 15, 10};

  auto result =
      parallel_foreach(reactor, delays, [](Reactor &r, int delay) -> Fut<void, AllocationError> {
        co_await Sleep{std::chrono::milliseconds(delay)};
        co_return Ok{};
      }).block_on();

  auto duration = SteadyClock::now() - start;

  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(duration >= 15ms);
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: fails when single task fails") {
  std::array<int, 3> values{1, 2, 3};

  int call_count = 0;
  auto result =
      parallel_foreach(reactor, values, [&](Reactor &r, int idx) -> Fut<void, AllocationError> {
        call_count++;
        if (idx == 2) {
          co_return Failure{AllocationError{}};
        }
        co_return Ok{};
      }).block_on();

  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(result.error().holds<AllocationError>());
  COROSIG_REQUIRE(call_count == 3);
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: fails when first task fails") {
  std::array<int, 3> values{1, 2, 3};

  int call_count = 0;
  auto result =
      parallel_foreach(reactor, values, [&](Reactor &r, int idx) -> Fut<void, AllocationError> {
        call_count++;
        if (idx == 1) {
          co_return Failure{AllocationError{}};
        }
        co_return Ok{};
      }).block_on();

  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(result.error().holds<AllocationError>());
  COROSIG_REQUIRE(call_count == 3);
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: preserves value references") {
  std::array<int, 3> values{1, 2, 3};

  auto result =
      parallel_foreach(reactor, values, [](Reactor &r, int &val) -> Fut<void, AllocationError> {
        val *= 2;
        co_return Ok{};
      }).block_on();

  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(values[0] == 2);
  COROSIG_REQUIRE(values[1] == 4);
  COROSIG_REQUIRE(values[2] == 6);
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: large range works if there is enough memory") {
  std::array<int, 100> values{};

  auto result = parallel_foreach(reactor, values, [](Reactor &, int) {
                  return Fut<void, AllocationError>::make_ready(Ok{});
                }).block_on();

  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: sized range uses reserve") {
  std::array<int, 50> values;
  std::fill(values.begin(), values.end(), 1);

  auto result = parallel_foreach(reactor, values, [](Reactor &, int) -> Fut<void, AllocationError> {
                  co_return Ok{};
                }).block_on();

  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: with constant ref range") {
  const std::array<int, 3> values{1, 2, 3};

  auto result = parallel_foreach(reactor,
                                 values,
                                 [](Reactor &, const int &val) -> Fut<void, AllocationError> {
                                   COROSIG_REQUIRE(val > 0);
                                   co_return Ok{};
                                 })
                    .block_on();

  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("parallel_foreach: nested parallel_foreach ") {
  std::array<std::array<int, 3>, 2> matrix{{{1, 2, 3}, {4, 5, 6}}};

  int total = 0;
  auto result =
      parallel_foreach(reactor, matrix, [&](Reactor &r, const std::array<int, 3> &row) {
        return parallel_foreach(r, row, [&](Reactor &, int val) -> Fut<void, AllocationError> {
          total += val;
          co_return Ok{};
        });
      }).block_on();

  COROSIG_REQUIRE(result.is_ok());
  COROSIG_REQUIRE(total == 21);
}
