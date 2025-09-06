#define CATCH_CONFIG_MAIN

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Yield.hpp"
#include "corosig/io/Pipe.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch.hpp>
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

TEST_CASE("Coroutine yielded") {
  test_in_sighandler([] {
    auto foo = []() -> Fut<int> {
      co_await Yield{};
      co_return 20;
    };

    auto res = foo().block_on();
    COROSIG_REQUIRE(res);
    COROSIG_REQUIRE(res.value() == 20);
  });
}

TEST_CASE("Coroutine awaits another coroutine") {
  test_in_sighandler([] {
    constexpr static auto foo = []() -> Fut<int> {
      co_await Yield{};
      co_return 20;
    };

    constexpr static auto bar = []() -> Fut<int> {
      co_await Yield{};
      co_return 20 + (co_await foo()).value();
    };

    auto res = bar().block_on();
    COROSIG_REQUIRE(res);
    COROSIG_REQUIRE(res.value() == 40);
  });
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
