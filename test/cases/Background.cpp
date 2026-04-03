#include "corosig/Background.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/Sleep.hpp"
#include "corosig/Yield.hpp"
#include "corosig/io/Pipe.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

namespace {

using namespace corosig;

BackgroundTask bg_yield(Reactor &) noexcept {
  co_await Yield{};
}

BackgroundTask bg_sleep(Reactor &) noexcept {
  using namespace std::chrono_literals;
  co_await Sleep{10ms};
}

BackgroundTask bg_noop(Reactor &) noexcept {
  co_return;
}

Fut<void, Error<AllocationError, SyscallError>> pipe_roundtrip(Reactor &r) noexcept {
  COROSIG_CO_TRY(auto pipes, PipePair::make());

  constexpr std::string_view MSG = "hello world!";
  COROSIG_CO_TRY(size_t written, co_await pipes.write.write(r, MSG));
  COROSIG_REQUIRE(written == MSG.size());

  std::array<char, MSG.size()> buf;
  COROSIG_CO_TRY(size_t read, co_await pipes.read.read(r, buf));
  COROSIG_REQUIRE(std::string_view{buf.begin(), read} == MSG);

  co_return Ok{};
};

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("BackgroundTask can be created") {
  auto task = BackgroundTask{Ok{}};
  COROSIG_REQUIRE(task);
}

COROSIG_SIGHANDLER_TEST_CASE("BackgroundTask can represent failure") {
  auto task = BackgroundTask{Failure{AllocationError{}}};
  COROSIG_REQUIRE(!task);
  COROSIG_REQUIRE(task.error() == AllocationError{});
}

COROSIG_SIGHANDLER_TEST_CASE("BackgroundTask is nothrow movable") {
  auto task1 = BackgroundTask{Ok{}};
  auto task2 = std::move(task1);
  COROSIG_REQUIRE(task2);
}

COROSIG_SIGHANDLER_TEST_CASE("run_in_background basic behavior") {
  COROSIG_REQUIRE(bg_noop(reactor));
  COROSIG_REQUIRE(bg_sleep(reactor));
  COROSIG_REQUIRE(bg_yield(reactor));
  COROSIG_REQUIRE(run_in_background(reactor, pipe_roundtrip(reactor)));
  COROSIG_REQUIRE(run_in_background(reactor, pipe_roundtrip(reactor)));
  COROSIG_REQUIRE(run_in_background(reactor, pipe_roundtrip(reactor)));
  COROSIG_REQUIRE(run_in_background(reactor, pipe_roundtrip(reactor)));

  auto foo = [](Reactor &) -> Fut<void> { co_return Ok{}; };
  COROSIG_REQUIRE(foo(reactor).block_on_with_reactor_drain());
}
