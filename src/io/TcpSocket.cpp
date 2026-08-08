#include "corosig/io/TcpSocket.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/reactor/PollList.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "posix/FdOps.hpp"

#include <cerrno>
#include <cstddef>
#include <netinet/tcp.h>
#include <span>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using namespace corosig;

Fut<TcpSocket, Error<AllocationError, SyscallError>>
connect_impl(Reactor &r, SockaddrStorage const &target, TcpSocket sock) noexcept {
  auto len = os::posix::addr_length(target.native_storage);
  if (::connect(sock.underlying_handle(),
                reinterpret_cast<sockaddr const *>(&target.native_storage),
                len) == -1) {
    auto current_error = SyscallError::current();
    if (current_error.value != EINPROGRESS) {
      co_return Failure{current_error};
    }
  }

  int on = 1;
  // Not a hard failure. Just a little bit of performance loss
  (void)::setsockopt(sock.underlying_handle(), SOL_TCP, TCP_NODELAY, &on, sizeof(on));

  co_await PollEvent{sock.underlying_handle(),
                     PollEventExpectance::CAN_WRITE | PollEventExpectance::CAN_READ};

  int socket_error = 0;
  socklen_t socket_error_len = sizeof(socket_error);
  if (::getsockopt(
          sock.underlying_handle(), SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) == -1) {
    co_return Failure{SyscallError::current()};
  }
  if (socket_error != 0) {
    co_return Failure{SyscallError{socket_error}};
  }

  co_return sock;
}

} // namespace

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
TcpSocket::connect(Reactor &r, SockaddrStorage const &target) noexcept {
  using Fut = Fut<TcpSocket, Error<AllocationError, SyscallError>>;
  int sock = ::socket(target.native_storage.ss_family, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
  if (sock == -1) {
    return Fut::make_ready(Failure{SyscallError::current()});
  }

  return connect_impl(r, target, TcpSocket::make_from_os_specific_handle(sock));
}

Fut<TcpSocket, Error<AllocationError, SyscallError>> TcpSocket::connect_from(
    Reactor &r, SockaddrStorage const &local, SockaddrStorage const &target) noexcept {
  using Fut = Fut<TcpSocket, Error<AllocationError, SyscallError>>;

  int sock = ::socket(local.native_storage.ss_family, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
  if (sock == -1) {
    return Fut::make_ready(Failure{SyscallError::current()});
  }

  if (::bind(sock,
             reinterpret_cast<sockaddr const *>(&local.native_storage),
             os::posix::addr_length(local.native_storage)) != 0) {
    return Fut::make_ready(Failure{SyscallError::current()});
  }

  return connect_impl(r, target, TcpSocket::make_from_os_specific_handle(sock));
}

TcpSocket TcpSocket::make_from_os_specific_handle(os::Handle handle) noexcept {
  TcpSocket sock;
  sock.m_fd = handle;
  return sock;
}

} // namespace corosig
