#include "FdOps.hpp"
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/Result.hpp"

#include <cstddef>
#include <span>

#ifndef __unix__
static_assert(false, "Platform-specific file included on wrong platform");
#endif

namespace corosig::os {

Fut<size_t, Error<AllocationError, SyscallError>> read(int fd, std::span<char> buf) noexcept {
  size_t read = 0;
  while (read < buf.size()) {
    co_await PollEvent{fd, poll_event_e::READ};

    Result current_read = try_read_some(fd, buf);
    if (current_read.has_value()) {
      size_t value = current_read.assume_value();
      if (value == 0) {
        break;
      }
      read += value;
    } else if (read == 0) {
      co_return current_read.assume_error();
    } else {
      break;
    }
  }
  co_return read;
}

Fut<size_t, Error<AllocationError, SyscallError>> read_some(int fd, std::span<char> buf) noexcept {
  co_await PollEvent{fd, poll_event_e::READ};
  co_return try_read_some(fd, buf);
}

Result<size_t, SyscallError> try_read_some(int fd, std::span<char> buf) noexcept {
  ssize_t n = ::read(fd, buf.data(), buf.size());
  if (n == -1) {
    return SyscallError::current();
  }
  return size_t(n);
}

Fut<size_t, Error<AllocationError, SyscallError>> write(int fd,
                                                        std::span<char const> buf) noexcept {
  size_t written = 0;
  while (written < buf.size()) {
    co_await PollEvent{fd, poll_event_e::WRITE};

    Result current_write = try_write_some(fd, buf);
    if (current_write.has_value()) {
      written += current_write.assume_value();
    } else if (written == 0) {
      co_return SyscallError::current();
    } else {
      break;
    }
  }
  co_return written;
}

Fut<size_t, Error<AllocationError, SyscallError>> write_some(int fd,
                                                             std::span<char const> buf) noexcept {
  co_await PollEvent{fd, poll_event_e::WRITE};
  co_return try_write_some(fd, buf);
}

Result<size_t, SyscallError> try_write_some(int fd, std::span<char const> buf) noexcept {
  ssize_t n = ::write(fd, buf.data(), buf.size());
  if (n == -1) {
    return SyscallError::current();
  }
  return size_t(n);
}

void close(int &fd) noexcept {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

} // namespace corosig::os
