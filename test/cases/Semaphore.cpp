#include "corosig/Semaphore.hpp"

#include "corosig/Coro.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Construction and basic properties") {
  constexpr size_t MAX_UNITS = 5;
  Semaphore sem{reactor, MAX_UNITS};

  COROSIG_REQUIRE(sem.max_parallelism() == MAX_UNITS);
  COROSIG_REQUIRE(sem.current_parallelism() == 0);
  COROSIG_REQUIRE(!sem.would_block(1));
  COROSIG_REQUIRE(!sem.would_block(MAX_UNITS));
  COROSIG_REQUIRE(sem.would_block(MAX_UNITS + 1));
}

COROSIG_SIGHANDLER_TEST_CASE("try_hold success") {
  constexpr size_t MAX_UNITS = 5;
  Semaphore sem(reactor, MAX_UNITS);

  auto holder1 = sem.try_hold(3);
  COROSIG_REQUIRE(holder1.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 3);

  auto holder2 = sem.try_hold(2);
  COROSIG_REQUIRE(holder2.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 5);

  COROSIG_REQUIRE(sem.would_block(1));
}

COROSIG_SIGHANDLER_TEST_CASE("try_hold failure") {
  constexpr size_t MAX_UNITS = 5;
  Semaphore sem{reactor, MAX_UNITS};

  auto holder1 = sem.try_hold(MAX_UNITS);
  COROSIG_REQUIRE(holder1.has_value());

  auto holder2 = sem.try_hold(1);
  COROSIG_REQUIRE(!holder2.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == MAX_UNITS);
}

COROSIG_SIGHANDLER_TEST_CASE("Holder destruction frees units") {
  constexpr size_t MAX_UNITS = 5;
  Semaphore sem{reactor, MAX_UNITS};

  {
    auto holder = sem.try_hold(3);
    COROSIG_REQUIRE(holder.has_value());
    COROSIG_REQUIRE(sem.current_parallelism() == 3);
  }

  COROSIG_REQUIRE(sem.current_parallelism() == 0);

  auto holder = sem.try_hold(MAX_UNITS);
  COROSIG_REQUIRE(holder.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == MAX_UNITS);
}

COROSIG_SIGHANDLER_TEST_CASE("Multiple holders") {
  constexpr size_t MAX_UNITS = 5;
  Semaphore sem{reactor, MAX_UNITS};

  auto holder1 = sem.try_hold(2);
  auto holder2 = sem.try_hold(2);
  auto holder3 = sem.try_hold(1);

  COROSIG_REQUIRE(holder1.has_value());
  COROSIG_REQUIRE(holder2.has_value());
  COROSIG_REQUIRE(holder3.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 5);

  auto holder4 = sem.try_hold(1);
  COROSIG_REQUIRE(!holder4.has_value());

  holder1.reset();
  COROSIG_REQUIRE(sem.current_parallelism() == 3);

  auto holder5 = sem.try_hold(2);
  COROSIG_REQUIRE(holder5.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 5);
}

COROSIG_SIGHANDLER_TEST_CASE("awaiter ready when units available") {
  constexpr size_t MAX_UNITS = 3;

  Semaphore sem{reactor, MAX_UNITS};
  {
    auto awaiter = sem.hold(2);
    COROSIG_REQUIRE(awaiter.await_ready());
    // Calling await_ready without resume at some point later is not an intended use case. In
    // practice, these methods should only be called via co_await operator
    auto holder = awaiter.await_resume();
  }

  auto holder = sem.try_hold(2);
  COROSIG_REQUIRE(holder.has_value());

  auto awaiter2 = sem.hold(2);
  COROSIG_REQUIRE(!awaiter2.await_ready());
}

COROSIG_SIGHANDLER_TEST_CASE("awaiter suspends when no units available") {
  constexpr size_t MAX_UNITS = 3;
  Semaphore sem{reactor, MAX_UNITS};

  // Take all units
  auto holder = sem.try_hold(MAX_UNITS);
  COROSIG_REQUIRE(holder.has_value());

  auto awaiter = sem.hold(1);

  // Should not be ready
  COROSIG_REQUIRE(!awaiter.await_ready());
}

COROSIG_SIGHANDLER_TEST_CASE("FIFO ordering of waiters") {
  constexpr size_t MAX_UNITS = 2;
  Semaphore sem{reactor, MAX_UNITS};

  auto holder1 = sem.try_hold(MAX_UNITS);
  COROSIG_REQUIRE(holder1.has_value());

  auto await_hold = [](Reactor &, Semaphore &sem) -> Fut<void> {
    (void)co_await sem.hold(1);
    co_return Ok{};
  };

  auto awaiter1 = await_hold(reactor, sem);
  auto awaiter2 = await_hold(reactor, sem);
  auto awaiter3 = await_hold(reactor, sem);

  COROSIG_REQUIRE(!awaiter1.completed());
  COROSIG_REQUIRE(!awaiter2.completed());
  COROSIG_REQUIRE(!awaiter3.completed());

  holder1.reset();

  COROSIG_REQUIRE(reactor.do_event_loop_iteration());

  COROSIG_REQUIRE(awaiter1.completed());
  COROSIG_REQUIRE(awaiter2.completed());
  COROSIG_REQUIRE(awaiter3.completed());
}

COROSIG_SIGHANDLER_TEST_CASE("Exact unit matching") {
  Semaphore sem(reactor, 5);

  // Take specific units
  auto holder1 = sem.try_hold(3);
  COROSIG_REQUIRE(holder1.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 3);

  // Try to take more than available
  auto holder2 = sem.try_hold(3);
  COROSIG_REQUIRE(!holder2.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 3);

  // But should be able to take exactly what's left
  auto holder3 = sem.try_hold(2);
  COROSIG_REQUIRE(holder3.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 5);
}

COROSIG_SIGHANDLER_TEST_CASE("Large unit requests") {
  Semaphore sem(reactor, 100);

  auto holder1 = sem.try_hold(50);
  COROSIG_REQUIRE(holder1.has_value());

  auto holder2 = sem.try_hold(50);
  COROSIG_REQUIRE(holder2.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 100);

  COROSIG_REQUIRE(sem.would_block(1));

  holder1.reset();
  COROSIG_REQUIRE(sem.current_parallelism() == 50);

  auto holder3 = sem.try_hold(50);
  COROSIG_REQUIRE(holder3.has_value());
  COROSIG_REQUIRE(sem.current_parallelism() == 100);
}

COROSIG_SIGHANDLER_TEST_CASE("Zero max parallelism") {
  Semaphore sem{reactor, 0};

  COROSIG_REQUIRE(sem.max_parallelism() == 0);
  COROSIG_REQUIRE(sem.current_parallelism() == 0);
  COROSIG_REQUIRE(!sem.would_block(0));
  COROSIG_REQUIRE(sem.would_block(1));

  auto holder = sem.try_hold(0);
  COROSIG_REQUIRE(holder.has_value());
}

COROSIG_SIGHANDLER_TEST_CASE("Move semantics") {
  Semaphore sem{reactor, 5};

  auto holder1 = *sem.try_hold(3);
  auto holder2 = std::move(holder1);

  COROSIG_REQUIRE(sem.current_parallelism() == 3);

  holder1.reset();
  COROSIG_REQUIRE(sem.current_parallelism() == 3);

  holder2.reset();
  COROSIG_REQUIRE(sem.current_parallelism() == 0);
}
