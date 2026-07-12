#include "corosig/Background.hpp"
#include "corosig/Coro.hpp"
#include "corosig/Result.hpp"
#include "corosig/Yield.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <iostream>
#include <memory>
#include <ostream>

namespace {

using namespace corosig;

Fut<void> synchronous_task(Reactor &) noexcept {
  co_return Ok{};
}

auto optimized_synchronous_task(Reactor &) noexcept {
  return Fut<void>::make_ready(Ok{});
}

Fut<void> simple_task(Reactor &) noexcept {
  for (size_t i = 0; i < 3; ++i) {
    co_await Yield{};
  }
  co_return Ok{};
}

Fut<void> recursive_task(Reactor &r) noexcept {
  COROSIG_CO_TRYV(co_await simple_task(r));
  COROSIG_CO_TRYV(co_await simple_task(r));
  co_return Ok{};
}

Fut<void> recursive_synchronous_task(Reactor &r) noexcept {
  COROSIG_CO_TRYV(co_await synchronous_task(r));
  COROSIG_CO_TRYV(co_await synchronous_task(r));
  co_return Ok{};
}

Fut<void> optimized_recursive_synchronous_task(Reactor &r) noexcept {
  COROSIG_CO_TRYV(co_await optimized_synchronous_task(r));
  COROSIG_CO_TRYV(co_await optimized_synchronous_task(r));
  co_return Ok{};
}

constexpr auto REACTOR_MEMORY = size_t(1024) * 1024 * 400;
constexpr size_t ITERATIONS = 1000000;

auto g_mem = std::make_unique<Allocator::Memory<REACTOR_MEMORY>>();

template <typename F>
void generic_benchmark(char const *description, F &&coro_factory) {
  Reactor reactor{*g_mem};

  BENCHMARK(description) {
    for (size_t i = 0; i < ITERATIONS / 2; ++i) {
      auto task = run_in_background(reactor, coro_factory(reactor));

      REQUIRE(task);
    }

    for (size_t i = 0; i < 3; ++i) {
      REQUIRE(reactor.do_event_loop_iteration());
    }

    for (size_t i = 0; i < ITERATIONS / 2; ++i) {
      auto task = run_in_background(reactor, coro_factory(reactor));
      REQUIRE(task);
    }

    while (reactor.has_active_tasks()) {
      REQUIRE(reactor.do_event_loop_iteration());
    }
  };

  std::cout << "\nReactor peak memory is " << reactor.peak_memory() << '\n';
  std::cout << "Which means " << reactor.peak_memory() / ITERATIONS
            << " bytes per iteration on average\n";
}

} // namespace

TEST_CASE("Benchmark spawning coroutines") {
  generic_benchmark("Spawn and execute 1 million coroutines", simple_task);
}

TEST_CASE("Benchmark spawning synchronous coroutines") {
  generic_benchmark("Spawn and execute 1 million synchronous coroutines", synchronous_task);
}

TEST_CASE("Benchmark spawning optimized synchronous coroutines") {
  generic_benchmark("Spawn and execute 1 million optimized synchronous coroutines",
                    optimized_synchronous_task);
}

TEST_CASE("Benchmark spawning recursive coroutines") {
  generic_benchmark("Spawn and execute 1 million recursive coroutines", recursive_task);
}

TEST_CASE("Benchmark spawning recursive synchronous coroutines") {
  generic_benchmark("Spawn and execute 1 million recursive synchronous coroutines",
                    recursive_synchronous_task);
}

TEST_CASE("Benchmark spawning optimized recursive synchronous coroutines") {
  generic_benchmark("Spawn and execute 1 million optimized recursive synchronous coroutines",
                    optimized_recursive_synchronous_task);
}
