#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Pipe.hpp"
#include "corosig/testing/Signals.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <catch2/catch.hpp>
#include <cmath>
#include <csignal>

using namespace corosig;

TEST_CASE("Coroutine executed synchronously") {
  test_in_sighandler([] {
    auto foo = []() -> Fut<int> { co_return 20; };
    auto res = foo().block_on();
    COROSIG_REQUIRE(res);
    COROSIG_REQUIRE(res.value() == 20);
  });
}

TEST_CASE("Fut move constructor transfers ownership") {
  test_in_sighandler([] {
    auto foo = []() -> Fut<> { co_return success(); };

    Fut<> fut1 = foo();
    COROSIG_REQUIRE(fut1.has_value());

    Fut<> fut2{std::move(fut1)};
    COROSIG_REQUIRE(fut2.has_value());
  });
}

TEST_CASE("Fut move constructor with nontrivial move") {
  test_in_sighandler([] {
    using MoveType = SetDefaultOnMove<double, double(2 << 10)>;

    auto foo = []() -> Fut<MoveType> { co_return success(MoveType{123.0}); };

    Fut<MoveType> fut1 = foo();
    COROSIG_REQUIRE(fut1.has_value());

    Fut<MoveType> fut2{std::move(fut1)};
    COROSIG_REQUIRE(fut2.has_value());
  });
}

TEST_CASE("Reactor allocation error") {
}

TEST_CASE("Fut::block_on returns error when reactor fails") {
  test_in_sighandler([] {
    auto foo = []() -> Fut<> {
      [[maybe_unused]] char buf[1024 * 1024];
      co_return success();
    };

    Result res = foo().block_on();
    COROSIG_REQUIRE(!res.has_value());
    COROSIG_REQUIRE(res.has_error());
    COROSIG_REQUIRE(res.assume_error().holds<AllocationError>());
  });
}

TEST_CASE("Fut can be co_awaited inside a coroutine") {
  constexpr static auto foo = []() -> Fut<int> { co_return 99; };
  constexpr static auto bar = []() -> Fut<int> { co_return 123 + (co_await foo()).value(); };

  auto result = bar().block_on();
  REQUIRE(result.has_value());
  REQUIRE(result.assume_value() == 123 + 99);
}

TEST_CASE("Coroutine polled pipe for read") {
  test_in_sighandler([] {
    auto foo = []() -> Fut<int, Error<AllocationError, SyscallError>> {
      BOOST_OUTCOME_CO_TRY(auto pipes, PipePair::make());

      constexpr std::string_view msg = "hello world!";
      BOOST_OUTCOME_CO_TRY(size_t written, co_await pipes.write.write(msg));
      COROSIG_REQUIRE(written == msg.size());

      char buf[msg.size()];
      BOOST_OUTCOME_CO_TRY(size_t read, co_await pipes.read.read(buf));
      COROSIG_REQUIRE(std::string_view{buf, read} == msg);

      co_return 10;
    };

    auto res = foo().block_on();
    COROSIG_REQUIRE(res);
    COROSIG_REQUIRE(res.value() == 10);
  });
}
