#pragma once

#include "corosig/os/Handle.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/PollList.hpp"

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

struct PollEvent : PollListNode {
  PollEvent(os::Handle handle, poll_event_e event) {
    this->handle = handle;
    this->event = event;
  }

  bool await_ready() const noexcept {
    return false;
  }

  template <std::derived_from<CoroListNode> PROMISE>
  void await_suspend(std::coroutine_handle<PROMISE> h) noexcept {
    waiting_coro = h;
    h.promise().poll_to_reactor(*this);
  }

  void await_resume() const noexcept {
  }
};

} // namespace corosig
