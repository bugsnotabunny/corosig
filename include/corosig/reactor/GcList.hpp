#ifndef COROSIG_REACTOR_GC_LIST_HPP
#define COROSIG_REACTOR_GC_LIST_HPP

#include <boost/intrusive/list_hook.hpp>

namespace corosig {

/// @brief An object which shall be destroyed at some point later
struct GcListNode
    : boost::intrusive::list_base_hook<
          boost::intrusive::link_mode<boost::intrusive::link_mode_type::auto_unlink>> {
  /// @brief A customization point to account for cases when destruction shall not be done through
  ///        destructor
  virtual void destroy() noexcept {
    this->~GcListNode();
  }

  virtual ~GcListNode() = default;
};

} // namespace corosig

#endif
