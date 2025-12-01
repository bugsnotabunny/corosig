#ifndef COROSIG_OS_HANDLE_HPP
#define COROSIG_OS_HANDLE_HPP

#ifdef __unix__

namespace corosig::os {

/// @brief An OS-specific handle for IO descriptors
using Handle = int;

} // namespace corosig::os

#endif

#endif
