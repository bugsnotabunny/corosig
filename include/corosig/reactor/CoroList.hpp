#pragma once

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <coroutine>

namespace corosig {

namespace bi = boost::intrusive;

struct CoroListNode : bi::slist_base_hook<bi::link_mode<bi::link_mode_type::safe_link>> {
  virtual std::coroutine_handle<> coro_from_this() noexcept = 0;
};

using CoroList =
    bi::slist<CoroListNode, bi::constant_time_size<false>, bi::linear<true>, bi::cache_last<true>>;

} // namespace corosig
