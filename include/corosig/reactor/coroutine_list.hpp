#pragma once

#include "boost/intrusive/options.hpp"
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <coroutine>

namespace corosig {

namespace bi = boost::intrusive;

struct CoroListNode : bi::list_base_hook<bi::link_mode<bi::link_mode_type::auto_unlink>> {
  virtual std::coroutine_handle<> coro_from_this() noexcept = 0;
};

using CoroList = bi::list<CoroListNode, bi::constant_time_size<false>>;

} // namespace corosig
