#pragma once

#include <cstring>
namespace corosig {

struct AllocationError {};

struct SyscallError {
  static SyscallError current() noexcept;

  char const *description() const noexcept;

  int value;
};

} // namespace corosig
