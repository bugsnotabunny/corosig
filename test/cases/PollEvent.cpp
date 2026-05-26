#include "corosig/PollEvent.hpp"

#include "corosig/Background.hpp"
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Sleep.hpp"
#include "corosig/Yield.hpp"
#include "corosig/io/Pipe.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace corosig;
using namespace std::chrono_literals;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("PollEvent CAN_READ awaits until data available") {
  auto foo = [](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
    COROSIG_CO_TRY(auto pipes, PipePair::make());

    // Start a background task that will write data after a delay
    auto writer = [](Reactor &reactor, PipeWrite write_pipe) -> BackgroundTask {
      co_await Sleep{10ms};
      constexpr std::string_view MSG = "test data";
      (void)co_await write_pipe.write(reactor, MSG);
    };
    COROSIG_REQUIRE(writer(r, std::move(pipes.write)));

    // Poll for read readiness - should wait until writer writes
    co_await PollEvent{pipes.read.underlying_handle(), PollEventExpectance::CAN_READ};

    // Verify we can now read the data
    std::array<char, 9> buf;
    COROSIG_CO_TRY(size_t read, co_await pipes.read.read(r, buf));
    COROSIG_REQUIRE(read == 9);

    co_return Ok(42);
  };

  auto res = foo(reactor).block_on_with_reactor_drain();
  COROSIG_REQUIRE(res);
  COROSIG_REQUIRE(res.value() == 42);
}

COROSIG_SIGHANDLER_TEST_CASE("PollEvent CAN_WRITE awaits until writable") {
  auto foo = [](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
    COROSIG_CO_TRY(auto pipes, PipePair::make());

    // Poll for write readiness - should be immediately available for empty pipe
    co_await PollEvent{pipes.write.underlying_handle(), PollEventExpectance::CAN_WRITE};

    // Verify we can write
    constexpr std::string_view MSG = "test";
    COROSIG_CO_TRY(size_t written, co_await pipes.write.write(r, MSG));
    COROSIG_REQUIRE(written == MSG.size());

    co_return Ok(99);
  };

  auto res = foo(reactor).block_on_with_reactor_drain();
  COROSIG_REQUIRE(res);
  COROSIG_REQUIRE(res.value() == 99);
}
