#pragma once

#include "corosig/error_types.hpp"
#include "corosig/reactor/coroutine_list.hpp"
#include "corosig/reactor/custom.hpp"
#include "corosig/reactor/default.hpp"
#include "corosig/result.hpp"

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
