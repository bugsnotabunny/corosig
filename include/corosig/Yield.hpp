#pragma once

#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Custom.hpp"
#include "corosig/reactor/Default.hpp"

#include <cassert>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace corosig {

template <AReactor REACTOR = Reactor>
struct Yield {
  bool await_ready() const noexcept {
    return false;
  }

  template <std::derived_from<CoroListNode> PROMISE>
  void await_suspend(std::coroutine_handle<PROMISE> h) const noexcept {
    reactor_provider<REACTOR>::engine().yield(h.promise());
  }

  void await_resume() const noexcept {
  }
};

} // namespace corosig
