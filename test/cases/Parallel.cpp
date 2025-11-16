#include "corosig/Parallel.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"
#include "corosig/testing/TestError.hpp"

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("when_all aggregates multiple futures", "[when_all]") {
  auto foo = [](Reactor &r) -> Fut<> {
    auto fut1 = [](Reactor &) -> Fut<int> { co_return success(1); }(r);
    auto fut2 = [](Reactor &) -> Fut<int> { co_return success(2); }(r);
    auto fut3 = [](Reactor &) -> Fut<int> { co_return success(3); }(r);

    COROSIG_CO_TRY(auto result,
                   co_await when_all(r, std::move(fut1), std::move(fut2), std::move(fut3)));

    REQUIRE(std::get<0>(result).value() == 1);
    REQUIRE(std::get<1>(result).value() == 2);
    REQUIRE(std::get<2>(result).value() == 3);

    co_return success();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed returns all values if no errors",
                             "[when_all_succeed]") {
  auto foo = [](Reactor &r) -> Fut<> {
    auto fut1 = [](Reactor &) -> Fut<int> { co_return success(10); }(r);
    auto fut2 = [](Reactor &) -> Fut<int, Error<AllocationError, TestError>> {
      co_return success(20);
    }(r);

    auto result = co_await when_all_succeed(r, std::move(fut1), std::move(fut2));

    REQUIRE(result.is_ok());
    auto values = result.value();
    REQUIRE(std::get<0>(values) == 10);
    REQUIRE(std::get<1>(values) == 20);

    co_return success();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("when_all_succeed propagates error if any future fails",
                             "[when_all_succeed]") {
  auto foo = [](Reactor &r) -> Fut<> {
    auto fut1 = [](Reactor &) -> Fut<int> { co_return success(100); }(r);
    auto fut2 = [](Reactor &) -> Fut<int, Error<AllocationError, TestError>> {
      co_return failure(TestError{});
    }(r);

    Result result = co_await when_all_succeed(r, std::move(fut1), std::move(fut2));
    REQUIRE(!result.is_ok());
    REQUIRE(result.error().holds<TestError>());
    co_return success();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}
