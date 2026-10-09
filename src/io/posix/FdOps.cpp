#include "FdOps.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/StreamSocket.hpp"
#include "corosig/reactor/PollList.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <boost/predef.h>
#include <cassert>
#include <cstddef>
#include <fcntl.h>
#include <limits>
#include <netinet/in.h>
#include <span>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef _POSIX_VERSION
static_assert(false, "Platform-specific file included on wrong platform");
#endif

namespace corosig::os::posix {

Fut<size_t, Error<AllocationError, SyscallError>>
read(Reactor &, int fd, std::span<char> buf) noexcept {
  size_t read = 0;
  while (read < buf.size()) {
    co_await PollEvent{fd, PollEventExpectance::CAN_READ};
    Result current_read = try_read_some(fd, buf.subspan(read));
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
  co_await PollEvent{fd, PollEventExpectance::CAN_READ};
  co_return try_read_some(fd, buf);
}

Result<size_t, SyscallError> try_read_some(int fd, std::span<char> buf) noexcept {
  ssize_t n = ::read(fd, buf.data(), buf.size());
  if (n == -1) {
    return Failure{SyscallError::current()};
  }
  return static_cast<size_t>(n);
}

Fut<size_t, Error<AllocationError, SyscallError>>
write(Reactor &, int fd, std::span<char const> buf) noexcept {
  size_t written = 0;
  while (written < buf.size()) {
    co_await PollEvent{fd, PollEventExpectance::CAN_WRITE};

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
  co_await PollEvent{fd, PollEventExpectance::CAN_WRITE};
  co_return try_write_some(fd, buf);
}

Result<size_t, SyscallError> try_write_some(int fd, std::span<char const> buf) noexcept {
  ssize_t n = ::write(fd, buf.data(), buf.size());
  if (n == -1) {
    return Failure{SyscallError::current()};
  }
  return static_cast<size_t>(n);
}

void close(int &fd) noexcept {
  if (fd != -1) {
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
    return SUN_LEN(reinterpret_cast<sockaddr_un const *>(&storage));
  default:
    assert(false && "Unsupported address family");
    return std::numeric_limits<socklen_t>::max();
  }
}

Result<SockaddrStorage, SyscallError> socket_address(int fd) noexcept {
  SockaddrStorage addr;
  socklen_t addr_len = sizeof(SockaddrStorage);

  if (getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr.native_storage), &addr_len) == -1) {
    return Failure{SyscallError::current()};
  }
  return addr;
}

Result<void, SyscallError> set_nonblocking_mode(int fd) noexcept {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return Failure{SyscallError::current()};
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return Failure{SyscallError::current()};
  }

  return Ok{};
}

Result<StreamSocket, SyscallError> make_nonblocking_stream_socket(sa_family_t family,
                                                                  int protocol) noexcept {
  int sock = ::socket(family, SOCK_STREAM, protocol);
  if (sock == -1) {
    return Failure{SyscallError::current()};
  }

  auto socket = StreamSocket::make_from_os_specific_handle(sock);
  COROSIG_TRYV(os::posix::set_nonblocking_mode(sock));
  return socket;
}

Result<PollConnect, SyscallError> PollConnect::make(os::Handle sock,
                                                    SockaddrStorage const &target) noexcept {
  auto len = os::posix::addr_length(target.native_storage);
  if (::connect(sock, reinterpret_cast<sockaddr const *>(&target.native_storage), len) == -1) {
    auto current_error = SyscallError::current();
    if (current_error.value != EINPROGRESS) {
      return Failure{current_error};
    }
  }

  return PollConnect{sock, PollEventExpectance::CAN_READ | PollEventExpectance::CAN_WRITE};
}

Result<void, SyscallError> PollConnect::await_resume() noexcept {
  int socket_error = 0;
  socklen_t socket_error_len = sizeof(socket_error);
  if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) == -1) {
    return Failure{SyscallError::current()};
  }
  if (socket_error != 0) {
    return Failure{SyscallError{socket_error}};
  }

  return Ok{};
}

} // namespace corosig::os::posix
