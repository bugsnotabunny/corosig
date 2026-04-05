#ifndef COROSIG_REACTOR_CORO_LIST_HPP
#define COROSIG_REACTOR_CORO_LIST_HPP

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/intrusive/options.hpp>
#include <coroutine>

namespace corosig {

/// @brief A node type for CoroList
struct CoroListNode
    : boost::intrusive::list_base_hook<
          boost::intrusive::link_mode<boost::intrusive::link_mode_type::auto_unlink>> {
  /// @brief Cast this object to a resumable coroutine handle
  virtual std::coroutine_handle<> coro_from_this() noexcept = 0;

  virtual ~CoroListNode() = default;
};

/// @brief A list of objects which are castable to coroutines via coro_from_this virtual method
using CoroList = boost::intrusive::list<CoroListNode,
                                        boost::intrusive::constant_time_size<false>,
                                        boost::intrusive::cache_last<true>,
                                        boost::intrusive::linear<true>>;

} // namespace corosig

#endif
