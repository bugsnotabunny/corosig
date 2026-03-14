#include "corosig/ErrorTypes.hpp"

#include <cerrno>
#include <cstring> // IWYU pragma: keep

namespace corosig {

SyscallError SyscallError::current() noexcept {
  return SyscallError{errno};
}

std::string_view SyscallError::description() const noexcept {
  return ::strerrordesc_np(value);
}

} // namespace corosig
