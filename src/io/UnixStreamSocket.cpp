#include "corosig/io/UnixStreamSocket.hpp"

#include "posix/FdOps.hpp"

namespace {

using namespace corosig;

static_assert(std::convertible_to<UnixStreamSocket, StreamSocket>);

} // namespace

namespace corosig {

UnixStreamSocket::UnixStreamSocket(StreamSocket socket) noexcept
    : StreamSocket{std::move(socket)} {
}

Fut<UnixStreamSocket, Error<AllocationError, SyscallError>>
UnixStreamSocket::connect(Reactor &r, SockaddrStorage const &target) noexcept {
  COROSIG_CO_TRY(StreamSocket stream_socket, os::posix::make_nonblocking_stream_socket(AF_UNIX, 0));
  UnixStreamSocket socket{std::move(stream_socket)};

  COROSIG_CO_TRY(auto poll, os::posix::PollConnect::make(socket.underlying_handle(), target));
  COROSIG_CO_TRYV(co_await poll);

  co_return socket;
}

UnixStreamSocket UnixStreamSocket::make_from_os_specific_handle(os::Handle handle) noexcept {
  return UnixStreamSocket{StreamSocket::make_from_os_specific_handle(handle)};
}

} // namespace corosig
