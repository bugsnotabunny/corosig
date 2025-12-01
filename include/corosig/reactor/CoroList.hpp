#ifndef COROSIG_REACTOR_CORO_LIST_HPP
#define COROSIG_REACTOR_CORO_LIST_HPP

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <coroutine>

namespace corosig {

namespace bi = boost::intrusive;

/// @brief A node type for CoroList
struct CoroListNode : bi::slist_base_hook<bi::link_mode<bi::link_mode_type::safe_link>> {
  /// @brief Cast this object to a resumable coroutine handle
  virtual std::coroutine_handle<> coro_from_this() noexcept = 0;

  virtual ~CoroListNode() = default;
};

/// @brief A list of objects which are castable to coroutines via coro_from_this virtual method
using CoroList = bi::slist<CoroListNode, bi::constant_time_size<false>, bi::cache_last<true>>;

} // namespace corosig

#endif
