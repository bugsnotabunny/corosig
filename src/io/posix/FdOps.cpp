#include "FdOps.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <cstddef>
#include <limits>
#include <span>

#ifndef __unix__
static_assert(false, "Platform-specific file included on wrong platform");
#endif

namespace corosig::os::posix {

Fut<size_t, Error<AllocationError, SyscallError>>
read(Reactor &, int fd, std::span<char> buf) noexcept {
  size_t read = 0;
  while (read < buf.size()) {

    co_await PollEvent{fd, poll_event_e::CAN_READ};
    Result current_read = try_read_some(fd, buf);
    if (current_read.is_ok()) {
      size_t value = current_read.value();
      if (value == 0) {
        break;
      }
      read += value;
    } else if (read == 0) {
      co_return Failure{current_read.error()};
    } else {
      break;
    }
  }
  co_return read;
}

Fut<size_t, Error<AllocationError, SyscallError>>
read_some(Reactor &, int fd, std::span<char> buf) noexcept {
  co_await PollEvent{fd, poll_event_e::CAN_READ};
  co_return try_read_some(fd, buf);
}

Result<size_t, SyscallError> try_read_some(int fd, std::span<char> buf) noexcept {
  ssize_t n = ::read(fd, buf.data(), buf.size());
  if (n == -1) {
    return Failure{SyscallError::current()};
  }
  return size_t(n);
}

Fut<size_t, Error<AllocationError, SyscallError>>
write(Reactor &, int fd, std::span<char const> buf) noexcept {
  size_t written = 0;
  while (written < buf.size()) {
    co_await PollEvent{fd, poll_event_e::CAN_WRITE};

    Result current_write = try_write_some(fd, buf);
    if (current_write.is_ok()) {
      written += current_write.value();
    } else if (written == 0) {
      co_return Failure{SyscallError::current()};
    } else {
      break;
    }
  }

  co_return written;
}

Fut<size_t, Error<AllocationError, SyscallError>>
write_some(Reactor &, int fd, std::span<char const> buf) noexcept {
  co_await PollEvent{fd, poll_event_e::CAN_WRITE};
  co_return try_write_some(fd, buf);
}

Result<size_t, SyscallError> try_write_some(int fd, std::span<char const> buf) noexcept {
  ssize_t n = ::write(fd, buf.data(), buf.size());
  if (n == -1) {
    return Failure{SyscallError::current()};
  }
  return size_t(n);
}

void close(int &fd) noexcept {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

socklen_t addr_length(sockaddr_storage const &storage) noexcept {
  switch (storage.ss_family) {
  case AF_INET:
    return sizeof(sockaddr_in);
  case AF_INET6:
    return sizeof(sockaddr_in6);
  case AF_UNIX:
    return sizeof(sockaddr_un);
  default:
    assert(false && "Unsupported address family");
    return std::numeric_limits<socklen_t>::max();
  }
}

} // namespace corosig::os::posix
