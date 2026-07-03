#include "corosig/Promise.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/LifetimeCounter.hpp"
#include "corosig/testing/NonCopyable.hpp"
#include "corosig/testing/Signals.hpp"

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Promise creation succeeds") {
  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Promise creation can fail allocation") {
  Allocator::Memory<size_t(16)> small_mem;
  Reactor small_reactor{small_mem};

  auto result = Promise<int>::make(small_reactor);
  COROSIG_REQUIRE(!result.is_ok());
  COROSIG_REQUIRE(result.error() == AllocationError{});
}

COROSIG_SIGHANDLER_TEST_CASE("Promise move constructor transfers ownership") {
  auto result1 = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result1.is_ok());

  Promise<int> promise2{std::move(result1.value())};
}

COROSIG_SIGHANDLER_TEST_CASE("Promise move assignment transfers ownership") {
  auto result1 = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result1.is_ok());
  Promise<int> promise2 = std::move(result1.value());

  auto result3 = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result3.is_ok());
  promise2 = std::move(result3.value());
}

COROSIG_SIGHANDLER_TEST_CASE("Promise with custom type") {
  struct CustomType {
    int value;
  };

  auto result = Promise<CustomType>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Get awaiter from promise") {
  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<int> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();
  COROSIG_REQUIRE(!awaiter.await_ready());
}

COROSIG_SIGHANDLER_TEST_CASE("Set value makes awaiter ready") {
  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<int> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();
  COROSIG_REQUIRE(!awaiter.await_ready());

  promise.set(42);

  COROSIG_REQUIRE(awaiter.await_ready());
  COROSIG_REQUIRE(awaiter.await_resume() == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Awaiter move constructor") {
  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<int> promise = std::move(result.value());

  auto awaiter1 = promise.get_awaiter();
  decltype(awaiter1) awaiter2{std::move(awaiter1)};

  promise.set(100);
  COROSIG_REQUIRE(awaiter2.await_ready());
  COROSIG_REQUIRE(awaiter2.await_resume() == 100);
}

COROSIG_SIGHANDLER_TEST_CASE("Awaiter move assignment") {
  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<int> promise = std::move(result.value());

  auto awaiter1 = promise.get_awaiter();
  decltype(awaiter1) awaiter2;
  awaiter2 = std::move(awaiter1);

  promise.set(200);
  COROSIG_REQUIRE(awaiter2.await_ready());
  COROSIG_REQUIRE(awaiter2.await_resume() == 200);
}

COROSIG_SIGHANDLER_TEST_CASE("Promise with non-copyable type") {
  auto result = Promise<NonCopyable>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<NonCopyable> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();
  promise.set(NonCopyable{42});

  COROSIG_REQUIRE(awaiter.await_ready());
  NonCopyable value = awaiter.await_resume();
  COROSIG_REQUIRE(value.value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Awaiter destructor decrements refcount") {
  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());

  {
    Promise<int> promise = std::move(result.value());
    auto awaiter = promise.get_awaiter();
    promise.set(42);
  }

  COROSIG_REQUIRE(!reactor.has_active_tasks());
}

COROSIG_SIGHANDLER_TEST_CASE("Promise destructor with broken promise for Result type") {
  struct TestError {
    [[nodiscard]] static std::string_view description() noexcept {
      return "Test error";
    }
  };

  using TestResult = Result<int, Error<TestError, BrokenPromise>>;
  static_assert(std::convertible_to<Failure<BrokenPromise &&>, TestResult>);

  auto result = Promise<TestResult>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());

  Promise<TestResult> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();
  // Destroy promise without setting value, should set broken promise
}

COROSIG_SIGHANDLER_TEST_CASE("Promise with Result that holds BrokenPromise") {
  auto producer = [](Reactor &r) -> Fut<> {
    auto result = Promise<Result<int, BrokenPromise>>::make(r);
    COROSIG_REQUIRE(result.is_ok());
    Promise<Result<int, BrokenPromise>> promise = std::move(result.value());

    auto awaiter = promise.get_awaiter();

    // Set value
    promise.set(Ok{42});

    co_return Ok{};
  };

  auto res = producer(reactor).block_on();
  COROSIG_REQUIRE(res.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Set value with different but convertible type") {
  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<int> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();

  double value = 42.5;
  promise.set(value);

  COROSIG_REQUIRE(awaiter.await_ready());
  int result_value = awaiter.await_resume();
  COROSIG_REQUIRE(result_value == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("Promise used in coroutine producer-consumer") {
  auto producer = [](Reactor &r) -> Fut<> {
    auto result = Promise<int>::make(r);
    COROSIG_REQUIRE(result.is_ok());
    Promise<int> promise = std::move(result.value());

    // Get awaiter and move it to consumer
    auto awaiter = promise.get_awaiter();

    // Set value
    promise.set(123);

    co_return Ok{};
  };

  auto res = producer(reactor).block_on();
  COROSIG_REQUIRE(res.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Set value with const reference type") {
  const int const_value = 777;

  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<int> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();
  promise.set(const_value);

  COROSIG_REQUIRE(awaiter.await_ready());
  int result_value = awaiter.await_resume();
  COROSIG_REQUIRE(result_value == 777);
}

COROSIG_SIGHANDLER_TEST_CASE("Set value with rvalue reference") {
  struct MovableType {
    MovableType() = default;
    MovableType(MovableType &&other) noexcept
        : value{std::exchange(other.value, 0)} {
    }
    MovableType &operator=(MovableType &&) = default;

    int value = 100;
  };

  auto result = Promise<MovableType>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<MovableType> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();
  promise.set(MovableType{});

  COROSIG_REQUIRE(awaiter.await_ready());
  MovableType received = awaiter.await_resume();
  COROSIG_REQUIRE(received.value == 100);
}

COROSIG_SIGHANDLER_TEST_CASE("Promise can be used with large types") {
  struct LargeType {
    std::array<uint8_t, 1024> data{};

    LargeType(uint8_t fill_value) {
      data.fill(fill_value);
    }
  };

  auto result = Promise<LargeType>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<LargeType> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();
  promise.set(LargeType{0xAB});

  COROSIG_REQUIRE(awaiter.await_ready());
  LargeType received = awaiter.await_resume();
  COROSIG_REQUIRE(received.data[0] == 0xAB);
  COROSIG_REQUIRE(received.data[1023] == 0xAB);
}

COROSIG_SIGHANDLER_TEST_CASE("Promise with empty optional keeps awaiter not ready") {
  auto result = Promise<int>::make(reactor);
  COROSIG_REQUIRE(result.is_ok());
  Promise<int> promise = std::move(result.value());

  auto awaiter = promise.get_awaiter();

  // await_ready should return false since value is not set yet
  COROSIG_REQUIRE(!awaiter.await_ready());
}
