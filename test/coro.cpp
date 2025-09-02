

#define CATCH_CONFIG_MAIN

#include "corosig/coro.hpp"
#include "boost/outcome/try.hpp"
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

TEST_CASE("hallo") {
  Alloc::Memory<1024 * 1024> mem;
  auto &reactor = reactor_provider<Reactor>::engine();
  reactor = Reactor{mem};

  REQUIRE(bar().block_on().value() == 20);
}
