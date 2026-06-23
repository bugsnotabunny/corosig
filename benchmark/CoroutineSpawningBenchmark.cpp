#include "corosig/Background.hpp"
#include "corosig/Coro.hpp"
#include "corosig/Yield.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <memory>

namespace {

using namespace corosig;

BackgroundTask simple_task(Reactor &) noexcept {
  for (size_t i = 0; i < 3; ++i) {
    co_await Yield{};
  }
}

} // namespace

TEST_CASE("Benchmark spawning coroutines") {
  constexpr auto REACTOR_MEMORY = size_t(1024) * 1024 * 100;
  auto mem = std::make_unique<Allocator::Memory<REACTOR_MEMORY>>();
  Reactor reactor{*mem};

  constexpr size_t ITERATIONS = 1000000;
  BENCHMARK("Spawn and execute 1 million coroutines") {
    for (size_t i = 0; i < ITERATIONS; ++i) {
      auto task = simple_task(reactor);
      REQUIRE(task);
    }
    while (reactor.has_active_tasks()) {
      REQUIRE(reactor.do_event_loop_iteration());
    }
  };

  std::cout << "\nReactor peak memory is " << reactor.peak_memory() << '\n';
  std::cout << "Which means " << reactor.peak_memory() / ITERATIONS
            << " bytes per coroutine on average\n";
}
