#ifndef COROSIG_REACTOR_SLEEP_LIST_HPP
#define COROSIG_REACTOR_SLEEP_LIST_HPP

#include "corosig/Clock.hpp"

#include <boost/intrusive/avl_set.hpp>
#include <boost/intrusive/avl_set_hook.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/options.hpp>
#include <coroutine>

namespace corosig {

/// @brief A node type for sleep task which may be pending or ready
struct SleepListNode : boost::intrusive::avl_set_base_hook<
                           boost::intrusive::link_mode<boost::intrusive::link_mode_type::safe_link>,
                           boost::intrusive::optimize_size<true>> {
  auto operator<=>(SleepListNode const &rhs) const noexcept {
    return awake_time <=> rhs.awake_time;
  }

  /// @brief Coro to be resumed
  std::coroutine_handle<> waiting_coro = nullptr;

  /// @brief When to resume a coro
  Clock::time_point awake_time;
};

/// @brief A list of objects which are castable to coroutines via coro_from_this virtual method
using SleepList =
    boost::intrusive::avl_multiset<SleepListNode, boost::intrusive::constant_time_size<false>>;

} // namespace corosig

#endif
