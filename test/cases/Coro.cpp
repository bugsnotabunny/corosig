

#define CATCH_CONFIG_MAIN

#include "corosig/Coro.hpp"
#include "corosig/Alloc.hpp"
#include "corosig/Result.hpp"
#include "corosig/Yield.hpp"
#include "corosig/reactor/Default.hpp"
#include "corosig/testing/Require.hpp"

#include <csignal>

#include "catch2/catch_all.hpp"

using namespace corosig;

Fut<int> foo() noexcept {
  co_return 20;
}

Fut<int> bar() noexcept {
  co_await Yield{};
  co_return co_await foo();
}

void sighandler(int sig) noexcept {
  std::signal(sig, SIG_DFL);

  Alloc::Memory<1024> mem;
  auto &reactor = reactor_provider<Reactor>::engine();
  reactor = Reactor{mem};

  SIG_REQUIRE(bar().block_on().value() == 20);
}

TEST_CASE("hallo") {
  REQUIRE(std::signal(SIGINT, sighandler) != SIG_ERR);
  raise(SIGINT);
}
