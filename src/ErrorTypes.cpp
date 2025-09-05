#include "corosig/ErrorTypes.hpp"
#include <cerrno>
#include <cstring>

namespace corosig {

SyscallError SyscallError::current() noexcept {
  return SyscallError{errno};
}

char const *SyscallError::description() const noexcept {
  return ::strerror(value);
}

} // namespace corosig
