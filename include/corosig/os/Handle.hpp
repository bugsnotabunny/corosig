#ifndef COROSIG_OS_HANDLE_HPP
#define COROSIG_OS_HANDLE_HPP

#include <unistd.h>

#ifdef _POSIX_VERSION

namespace corosig::os {

/// @brief An OS-specific handle for IO descriptors
using Handle = int;

} // namespace corosig::os

#else
static_assert(false, "Non-POSIX systems are not currently supported");
#endif

#endif
