#ifndef COROSIG_IO_UNIX_STREAM_SOCKET_HPP
#define COROSIG_IO_UNIX_STREAM_SOCKET_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/StreamSocket.hpp"
#include "corosig/os/Handle.hpp"

namespace corosig {

/// @brief An asynchronous TCP socket
struct UnixStreamSocket : StreamSocket {
  /// @brief Construct a TCP socket which refers to invalid os::Handle
  UnixStreamSocket() noexcept = default;

  /// @brief Downcast StreamSocket to UnixStreamSocket
  /// @warn  This is user's responsibility to ensure that StreamSocket is an actual unix-domain
  ///        socket
  explicit UnixStreamSocket(StreamSocket socket) noexcept;

  /// @brief Make a connection to specified target unix-domain addr
  /// @returns Ready-for-write socket or syscall error
  static Fut<UnixStreamSocket, Error<AllocationError, SyscallError>>
  connect(Reactor &, SockaddrStorage const &target) noexcept;

  /// @brief Construct a TCP socket which owns given os::Handle
  /// @warn This is user's responsibility to provide a handle to an actually valid UnixStreamSocket
  static UnixStreamSocket make_from_os_specific_handle(os::Handle handle) noexcept;
};

} // namespace corosig

#endif
