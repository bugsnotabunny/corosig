#ifndef COROSIG_POLL_EVENT_HPP
#define COROSIG_POLL_EVENT_HPP

#include "corosig/os/Handle.hpp"
#include "corosig/reactor/PollList.hpp"

#include <cassert>
#include <coroutine>

namespace corosig {

/// @brief Await for event to occur for handle
/// @note Event is only actually polled when this struct is co_awaited
struct PollEvent : PollListNode {
  PollEvent(os::Handle handle, poll_event_e event) {
    this->handle = handle;
    this->event = event;
  }

  static bool await_ready() noexcept {
    return false;
  }

  template <typename PROMISE>
  void await_suspend(std::coroutine_handle<PROMISE> h) noexcept {
    waiting_coro = h;
    h.promise().poll_to_reactor(*this);
  }

  static void await_resume() noexcept {
  }
};

} // namespace corosig

#endif
