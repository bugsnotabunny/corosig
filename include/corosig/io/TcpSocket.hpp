#ifndef COROSIG_IO_TCP_SOCKET_HPP
#define COROSIG_IO_TCP_SOCKET_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/StreamSocket.hpp"
#include "corosig/os/Handle.hpp"

namespace corosig {

/// @brief An asynchronous TCP socket
struct TcpSocket : StreamSocket {
  /// @brief Construct a TCP socket which refers to invalid os::Handle
  TcpSocket() noexcept = default;

  /// @brief Downcast StreamSocket to TcpSocket
  /// @warn  This is user's responsibility to ensure that StreamSocket is an actual TCP socket
  explicit TcpSocket(StreamSocket socket) noexcept;

  struct ConnectFromOptions {
    /// @brief An address to bind local socket to
    SockaddrStorage const &local;

    /// @brief An address to establish connection with
    SockaddrStorage const &target;
  };

  /// @brief Make a TCP connection to specified target addr
  /// @returns Ready-for-write socket or syscall error
  static Fut<TcpSocket, Error<AllocationError, SyscallError>>
  connect(Reactor &, SockaddrStorage const &target) noexcept;

  /// @brief Make a TCP connection from local addr to specified target addr
  /// @returns Ready-for-write socket or syscall error
  static Fut<TcpSocket, Error<AllocationError, SyscallError>>
  connect_from(Reactor &, SockaddrStorage const &local, SockaddrStorage const &target) noexcept;

  /// @brief Construct a TCP socket which owns given os::Handle
  /// @warn This is user's responsibility to provide a handle to an actually valid TcpSocket
  static TcpSocket make_from_os_specific_handle(os::Handle handle) noexcept;
};

} // namespace corosig

#endif
