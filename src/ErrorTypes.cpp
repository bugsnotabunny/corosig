#include "corosig/ErrorTypes.hpp"

#include <cerrno>
#include <cstring> // IWYU pragma: keep
#include <string_view>

namespace corosig {

SyscallError SyscallError::current() noexcept {
  return SyscallError{errno};
}

std::string_view SyscallError::description() const noexcept {
  return ::strerrordesc_np(value);
}

std::string_view AllocationError::description() noexcept {
  return "AllocationError";
}

} // namespace corosig
