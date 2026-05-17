#include "corosig/meta/Futurize.hpp"

#include "corosig/Coro.hpp"
#include "corosig/testing/Signals.hpp"

#include <string>

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("futurize: awaitable value simply returned") {
  static_assert(AnAwaitable<Fut<int, AllocationError>>);

  auto f = [](Reactor &) -> Fut<int, AllocationError> { co_return 1; }(reactor);
  auto &&f2 = futurize(f);
  static_assert(std::same_as<std::decay_t<decltype(f2)>, std::decay_t<decltype(f)>>);
  COROSIG_REQUIRE(&f == &f2);
}

COROSIG_SIGHANDLER_TEST_CASE("futurize: wraps non-awaitable value in ReadyAwaitable") {
  int value = 42;
  auto awaitable = futurize(value);

  COROSIG_REQUIRE(awaitable.await_ready() == true);
  COROSIG_REQUIRE(awaitable.await_resume() == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("ReadyAwaitable: await_ready always returns true") {
  auto awaitable = futurize(42);
  COROSIG_REQUIRE(awaitable.await_ready() == true);
}

COROSIG_SIGHANDLER_TEST_CASE("ReadyAwaitable: await_resume moves value") {
  std::string str = "test";
  auto awaitable = futurize(std::move(str));

  std::string result = awaitable.await_resume();
  COROSIG_REQUIRE(result == "test");
  COROSIG_REQUIRE(str.empty()); // NOLINT (bugprone-use-after-move)
}
