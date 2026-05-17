#ifndef COROSIG_SLEEP_HPP
#define COROSIG_SLEEP_HPP

#include "corosig/Clock.hpp"
#include "corosig/reactor/SleepList.hpp"

namespace corosig {

/// @brief  Break the execution of a coroutine until specified amount of time passes
struct [[nodiscard("forgot to await?")]] Sleep : SleepListNode {
  explicit Sleep(SteadyClock::time_point until) noexcept {
    this->awake_time = until;
  }

  template <typename REP, typename PERIOD>
  explicit Sleep(std::chrono::duration<REP, PERIOD> sleep_for) noexcept
      : Sleep{SteadyClock::now() + sleep_for} {
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
