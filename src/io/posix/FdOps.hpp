#pragma once

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include <cstddef>
#include <span>

#ifndef __unix__
static_assert(false, "Platform-specific file included on wrong platform");
#endif

namespace corosig::os::posix {

Fut<size_t, Error<AllocationError, SyscallError>> write(int fd, std::span<char const>) noexcept;
Fut<size_t, Error<AllocationError, SyscallError>> write_some(int fd,
                                                             std::span<char const>) noexcept;
Result<size_t, SyscallError> try_write_some(int fd, std::span<char const>) noexcept;

Fut<size_t, Error<AllocationError, SyscallError>> read(int fd, std::span<char>) noexcept;
Fut<size_t, Error<AllocationError, SyscallError>> read_some(int fd, std::span<char>) noexcept;
Result<size_t, SyscallError> try_read_some(int fd, std::span<char>) noexcept;

void close(int &fd) noexcept;

} // namespace corosig::os::posix
