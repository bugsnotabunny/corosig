#ifndef COROSIG_REACTOR_POLL_LIST_HPP
#define COROSIG_REACTOR_POLL_LIST_HPP

#include "corosig/os/Handle.hpp"

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <coroutine>
#include <sys/poll.h>

namespace corosig {

/// @brief Strictly-typed wrapper for poll events bitmask. Meant to abstract some OS-specific stuff
/// @warning Exact values and underlying type are OS-specific
enum class poll_event_e : short {
  CAN_READ = POLLIN,
  CAN_WRITE = POLLOUT,
};

/// @brief Node type for PollList
struct PollListNode
    : boost::intrusive::slist_base_hook<
          boost::intrusive::link_mode<boost::intrusive::link_mode_type::safe_link>> {
  /// @brief A coroutine stopped due to await. Will be resumed after event is present in handle
  std::coroutine_handle<> waiting_coro = std::noop_coroutine();

  /// @brief A handle which waits for event to occur
  os::Handle handle;

  /// @brief An event for which handle waits
  poll_event_e event;
};

/// @brief A list of coroutines stopped due to await of some IO-related events
using PollList = boost::intrusive::slist<PollListNode,
                                         boost::intrusive::constant_time_size<false>,
                                         boost::intrusive::linear<true>,
                                         boost::intrusive::cache_last<true>>;

} // namespace corosig

#endif
