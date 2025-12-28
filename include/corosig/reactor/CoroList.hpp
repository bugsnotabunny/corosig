#ifndef COROSIG_REACTOR_CORO_LIST_HPP
#define COROSIG_REACTOR_CORO_LIST_HPP

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <coroutine>

namespace corosig {

/// @brief A node type for CoroList
struct CoroListNode
    : boost::intrusive::slist_base_hook<
          boost::intrusive::link_mode<boost::intrusive::link_mode_type::safe_link>> {
  /// @brief Cast this object to a resumable coroutine handle
  virtual std::coroutine_handle<> coro_from_this() noexcept = 0;

  virtual ~CoroListNode() = default;
};

/// @brief A list of objects which are castable to coroutines via coro_from_this virtual method
using CoroList = boost::intrusive::slist<CoroListNode,
                                         boost::intrusive::constant_time_size<false>,
                                         boost::intrusive::cache_last<true>>;

} // namespace corosig

#endif
