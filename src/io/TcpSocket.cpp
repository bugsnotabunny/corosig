#include "corosig/io/TcpSocket.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/reactor/PollList.hpp"
#include "posix/FdOps.hpp"

#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <limits>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace corosig {

TcpSocket::~TcpSocket() {
  close();
}

Fut<size_t, Error<AllocationError, SyscallError>> TcpSocket::read(Reactor &r,
                                                                  std::span<char> buf) noexcept {
  return os::posix::read(r, m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
TcpSocket::read_some(Reactor &r, std::span<char> buf) noexcept {
  return os::posix::read_some(r, m_fd.value, buf);
}

Result<size_t, SyscallError> TcpSocket::try_read_some(std::span<char> buf) noexcept {
  return os::posix::try_read_some(m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
TcpSocket::write(Reactor &r, std::span<char const> buf) noexcept {
  return os::posix::write(r, m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
TcpSocket::write_some(Reactor &r, std::span<char const> buf) noexcept {
  return os::posix::write_some(r, m_fd.value, buf);
}

Result<size_t, SyscallError> TcpSocket::try_write_some(std::span<char const> buf) noexcept {
  return os::posix::try_write_some(m_fd.value, buf);
}

os::Handle TcpSocket::underlying_handle() const noexcept {
  return m_fd.value;
}

void TcpSocket::close() noexcept {
  return os::posix::close(m_fd.value);
}

Fut<TcpSocket, Error<AllocationError, SyscallError>>
TcpSocket::connect(Reactor &, sockaddr_storage const &addr) noexcept {
  int sock = ::socket(addr.ss_family, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
  if (sock == -1) {
    co_return Failure{SyscallError::current()};
  }

  TcpSocket self;
  self.m_fd.value = sock;

  int on = 1;
  // Not a hard failure. Just a little bit of performance loss
  (void)::setsockopt(sock, SOL_SOCKET, O_NDELAY, &on, sizeof(on));

  auto len = [family = addr.ss_family]() -> socklen_t {
    switch (family) {
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
  }();

  if (::connect(sock, (sockaddr const *)&addr, len) == -1) {
    auto current_error = SyscallError::current();
    if (current_error.value != EINPROGRESS) {
      co_return Failure{current_error};
    }
  }

  co_await PollEvent{sock, poll_event_e::CAN_WRITE};
  co_return self;
}

} // namespace corosig
