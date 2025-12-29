#ifndef COROSIG_SLEEP_HPP
#define COROSIG_SLEEP_HPP

#include "corosig/Clock.hpp"
#include "corosig/reactor/SleepList.hpp"

#include <csignal>
#include <cstdlib>

namespace corosig {

/// @brief  Break the execution of a coroutine until specified amount of time passes
struct Sleep : SleepListNode {
  Sleep(Clock::time_point until) {
    this->awake_time = until;
  }

  template <typename REP, typename PERIOD>
  Sleep(std::chrono::duration<REP, PERIOD> sleep_for)
      : Sleep{Clock::now() + sleep_for} {
  }

  [[nodiscard]] static bool await_ready() noexcept {
    return false;
  }

  template <typename PROMISE>
  void await_suspend(std::coroutine_handle<PROMISE> h) noexcept {
    this->waiting_coro = h;
    h.promise().queue_to_reactor(*this);
  }

  void await_resume() const noexcept {
  }
};

} // namespace corosig

#endif
