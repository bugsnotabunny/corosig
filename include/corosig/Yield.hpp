#pragma once

#include "corosig/reactor/CoroList.hpp"

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

struct Yield {
  bool await_ready() const noexcept {
    return false;
  }

  template <std::derived_from<CoroListNode> PROMISE>
  void await_suspend(std::coroutine_handle<PROMISE> h) const noexcept {
    h.promise().yield_to_reactor();
  }

  void await_resume() const noexcept {
  }
};

} // namespace corosig
