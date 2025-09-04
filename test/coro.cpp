

#include <csignal>
#define CATCH_CONFIG_MAIN

#include "boost/outcome/try.hpp"
#include "corosig/coro.hpp"
#include "corosig/error_types.hpp"
#include "corosig/reactor/custom.hpp"
#include "corosig/reactor/default.hpp"
#include "corosig/result.hpp"
#include "corosig/static_buf_allocator.hpp"
#include "corosig/yield.hpp"

#include "catch2/catch_all.hpp"

using namespace corosig;

Fut<int> foo() noexcept {
  co_return 20;
}

Fut<int> bar() noexcept {
  co_await Yield{};
  co_return co_await foo();
}

void sighandler(int) noexcept {
  Alloc::Memory<1024> mem;
  auto &reactor = reactor_provider<Reactor>::engine();
  reactor = Reactor{mem};

  if (bar().block_on().value() != 20) {
    _exit(1);
  }
}

TEST_CASE("hallo") {
  REQUIRE(std::signal(SIGINT, sighandler) != SIG_ERR);
  raise(SIGINT);
}
