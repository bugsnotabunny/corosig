#pragma once

#include "corosig/os/Handle.hpp"

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <coroutine>

namespace corosig {

namespace bi = boost::intrusive;

enum class poll_event_e {
  READ,
  WRITE,
};

struct PollListNode : bi::slist_base_hook<bi::link_mode<bi::link_mode_type::safe_link>> {
  std::coroutine_handle<> waiting_coro;
  os::Handle handle;
  poll_event_e event;
};

using PollList =
    bi::slist<PollListNode, bi::constant_time_size<false>, bi::linear<true>, bi::cache_last<true>>;

} // namespace corosig
