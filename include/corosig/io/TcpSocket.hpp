#ifndef COROSIG_IO_TCP_SOCKET_HPP
#define COROSIG_IO_TCP_SOCKET_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <span>
#include <sys/socket.h>

namespace corosig {

/// @brief An asynchronous TCP socket
struct TcpSocket {
public:
  /// @brief Construct a TCP socket which refers to invalid os::Handle
  TcpSocket() noexcept = default;

  /// @brief Make a TCP connection to specified addr
  /// @returns Ready-for-write socket or syscall error
  static Fut<TcpSocket, Error<AllocationError, SyscallError>>
  connect(Reactor &, sockaddr_storage const &addr) noexcept;

  TcpSocket(const TcpSocket &) = delete;
  TcpSocket(TcpSocket &&) noexcept = default;
  TcpSocket &operator=(const TcpSocket &) = delete;
  TcpSocket &operator=(TcpSocket &&) noexcept = default;

  ~TcpSocket();

  /// @brief Read bytes into buffer until it is full
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> read(Reactor &, std::span<char>) noexcept;

  /// @brief Read bytes into buffer
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(Reactor &, std::span<char>) noexcept;

  /// @brief Read bytes into buffer if socket is read-ready
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Result<size_t, SyscallError> try_read_some(std::span<char>) noexcept;

  /// @brief Write all bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> write(Reactor &,
                                                          std::span<char const>) noexcept;

  /// @brief Write bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(Reactor &,
                                                               std::span<char const>) noexcept;

  /// @brief Write bytes from buffer if socket is write-ready
  /// @returns Number of bytes written or a syscall error
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  /// @brief Free allocated resources and invalidate underlying handle
  void close() noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

} // namespace corosig

#endif
