#ifndef COROSIG_REACTOR_SLEEP_LIST_HPP
#define COROSIG_REACTOR_SLEEP_LIST_HPP

#include "corosig/Clock.hpp"

#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/rbtree.hpp>
#include <boost/intrusive/set_hook.hpp>
#include <coroutine>

namespace corosig {

/// @brief A node type for sleep task which may be pending or ready
struct SleepListNode
    : boost::intrusive::set_base_hook<
          boost::intrusive::link_mode<boost::intrusive::link_mode_type::auto_unlink>> {
  auto operator<=>(SleepListNode const &rhs) const noexcept {
    return awake_time <=> rhs.awake_time;
  }

  /// @brief Coro to be resumed
  std::coroutine_handle<> waiting_coro = nullptr;

  /// @brief When to resume a coro
  SteadyClock::time_point awake_time;
};

} // namespace corosig

#endif
