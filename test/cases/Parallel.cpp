#include "corosig/Parallel.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/testing/Signals.hpp"
#include "corosig/testing/TestError.hpp"

#include <boost/outcome/try.hpp>

using namespace corosig;

TEST_CASE("when_all aggregates multiple futures", "[when_all]") {
  corosig::test_in_sighandler([] {
    auto foo = []() -> Fut<> {
      auto fut1 = []() -> Fut<int> { co_return success(1); }();
      auto fut2 = []() -> Fut<int> { co_return success(2); }();
      auto fut3 = []() -> Fut<int> { co_return success(3); }();

      BOOST_OUTCOME_CO_TRY(auto result,
                           co_await when_all(std::move(fut1), std::move(fut2), std::move(fut3)));

      REQUIRE(std::get<0>(result).assume_value() == 1);
      REQUIRE(std::get<1>(result).assume_value() == 2);
      REQUIRE(std::get<2>(result).assume_value() == 3);

      co_return success();
    };
    COROSIG_REQUIRE(foo().block_on().has_value());
  });
}

TEST_CASE("when_all_succeed returns all values if no errors", "[when_all_succeed]") {
  corosig::test_in_sighandler([] {
    auto foo = []() -> Fut<> {
      auto fut1 = []() -> Fut<int> { co_return success(10); }();
      auto fut2 = []() -> Fut<int, Error<AllocationError, TestError>> { co_return success(20); }();

      auto result = co_await when_all_succeed(std::move(fut1), std::move(fut2));

      REQUIRE_FALSE(result.has_error());
      auto values = result.assume_value();
      REQUIRE(std::get<0>(values) == 10);
      REQUIRE(std::get<1>(values) == 20);

      co_return success();
    };
    COROSIG_REQUIRE(foo().block_on().has_value());
  });
}

TEST_CASE("when_all_succeed propagates error if any future fails", "[when_all_succeed]") {
  corosig::test_in_sighandler([]() {
    auto foo = []() -> Fut<> {
      auto fut1 = []() -> Fut<int> { co_return success(100); }();
      auto fut2 = []() -> Fut<int, Error<AllocationError, TestError>> {
        co_return failure(TestError{});
      }();

      Result result = co_await when_all_succeed(std::move(fut1), std::move(fut2));
      REQUIRE(result.has_error());
      REQUIRE(result.assume_error().holds<TestError>());
      co_return success();
    };
    COROSIG_REQUIRE(foo().block_on().has_value());
  });
}
