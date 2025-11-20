#ifndef COROSIG_REACTOR_POLL_LIST_HPP
#define COROSIG_REACTOR_POLL_LIST_HPP

#include "corosig/os/Handle.hpp"

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <coroutine>
#include <sys/poll.h>

namespace corosig {

namespace bi = boost::intrusive;

/// Exact values and underlying type are os-specific
enum class poll_event_e : short {
  CAN_READ = POLLIN,
  CAN_WRITE = POLLOUT,
};

struct PollListNode : bi::slist_base_hook<bi::link_mode<bi::link_mode_type::safe_link>> {
  std::coroutine_handle<> waiting_coro = std::noop_coroutine();
  os::Handle handle;
  poll_event_e event;
};

using PollList =
    bi::slist<PollListNode, bi::constant_time_size<false>, bi::linear<true>, bi::cache_last<true>>;

} // namespace corosig

#endif
