#ifndef COROSIG_YIELD_HPP
#define COROSIG_YIELD_HPP

#include "corosig/reactor/CoroList.hpp"

#include <cassert>
#include <concepts>
#include <coroutine>

namespace corosig {

struct Yield {
  [[nodiscard]] static bool await_ready() noexcept {
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

#endif
