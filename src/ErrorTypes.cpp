#include "corosig/ErrorTypes.hpp"

#include <cerrno>
#include <cstring>

namespace corosig {

SyscallError SyscallError::current() noexcept {
  return SyscallError{errno};
}

std::string_view SyscallError::description() const noexcept {
  return ::strerror(value);
}

} // namespace corosig
