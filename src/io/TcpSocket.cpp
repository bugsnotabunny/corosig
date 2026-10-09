#include "corosig/io/TcpSocket.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/StreamSocket.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "posix/FdOps.hpp"

#include <netinet/tcp.h>
#include <sys/socket.h>

namespace {

using namespace corosig;

static_assert(std::convertible_to<TcpSocket, StreamSocket>);

Result<TcpSocket, SyscallError> make_disconnected_tcp_socket(sa_family_t family) noexcept {
  COROSIG_TRY(StreamSocket stream_socket,
              os::posix::make_nonblocking_stream_socket(family, IPPROTO_TCP));

  TcpSocket socket{std::move(stream_socket)};

  int on = 1;
  // Not a hard failure. Just a little bit of performance loss
  (void)::setsockopt(socket.underlying_handle(), IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

  return socket;
}

} // namespace

namespace corosig {

TcpSocket::TcpSocket(StreamSocket socket) noexcept
    : StreamSocket(std::move(socket)) {
}

Fut<TcpSocket, Error<AllocationError, SyscallError>>
TcpSocket::connect(Reactor &r, SockaddrStorage const &target) noexcept {
  COROSIG_CO_TRY(TcpSocket socket, make_disconnected_tcp_socket(target.native_storage.ss_family));

  COROSIG_CO_TRY(auto poll, os::posix::PollConnect::make(socket.underlying_handle(), target));
  COROSIG_CO_TRYV(co_await poll);

  co_return socket;
}

Fut<TcpSocket, Error<AllocationError, SyscallError>> TcpSocket::connect_from(
    Reactor &r, SockaddrStorage const &local, SockaddrStorage const &target) noexcept {
  assert(local.native_storage.ss_family == target.native_storage.ss_family);

  COROSIG_CO_TRY(TcpSocket socket, make_disconnected_tcp_socket(target.native_storage.ss_family));

  if (::bind(socket.underlying_handle(),
             reinterpret_cast<sockaddr const *>(&local.native_storage),
             os::posix::addr_length(local.native_storage)) != 0) {
    co_return Failure{SyscallError::current()};
  }

  COROSIG_CO_TRY(auto poll, os::posix::PollConnect::make(socket.underlying_handle(), target));
  COROSIG_CO_TRYV(co_await poll);

  co_return socket;
}

TcpSocket TcpSocket::make_from_os_specific_handle(os::Handle handle) noexcept {
  return TcpSocket{StreamSocket::make_from_os_specific_handle(handle)};
}

} // namespace corosig
