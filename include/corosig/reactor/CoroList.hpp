#ifndef COROSIG_REACTOR_CORO_LIST_HPP
#define COROSIG_REACTOR_CORO_LIST_HPP

#include "corosig/reactor/GcList.hpp"

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/intrusive/options.hpp>
#include <coroutine>

namespace corosig {

/// @brief A node type for CoroList
/// @details Types that should be managed by CoroList must inherit from this and implement
///          coro_from_this() method
struct CoroListNode : GcListNode {
  /// @brief Cast this object to a resumable coroutine handle
  virtual std::coroutine_handle<> coro_from_this() noexcept = 0;

  ~CoroListNode() override = default;
};

} // namespace corosig

#endif
