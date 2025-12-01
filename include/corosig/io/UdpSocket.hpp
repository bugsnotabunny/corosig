#ifndef COROSIG_IO_UDP_SOCKET_HPP
#define COROSIG_IO_UDP_SOCKET_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <span>
#include <sys/socket.h>

namespace corosig {

/// @brief An asynchronous UDP socket
struct UdpSocket {
public:
  /// @brief Construct a UDP socket which refers to invalid os::Handle
  UdpSocket() noexcept = default;

  /// @brief Make a UDP socket capable of sending packages. Or get a syscall error
  static Result<UdpSocket, SyscallError> writer() noexcept;

  /// @brief Make a UDP socket capable of sending packages and receiving then on address local. Or
  ///        get a syscall error
  static Result<UdpSocket, SyscallError> readwriter(sockaddr_storage const &local) noexcept;

  UdpSocket(const UdpSocket &) = delete;
  UdpSocket(UdpSocket &&) noexcept = default;
  UdpSocket &operator=(const UdpSocket &) = delete;
  UdpSocket &operator=(UdpSocket &&) noexcept = default;

  ~UdpSocket();

  /// @brief Receive a package into buffer
  /// @returns Number of bytes read or a syscall error
  /// @returns 0 bytes read if EOF was reached
  /// @param source If source is not nullptr, it is set to tell where did the package come from
  Fut<size_t, Error<AllocationError, SyscallError>>
  recv_from(Reactor &, std::span<char>, sockaddr_storage *source = nullptr) noexcept;

  /// @brief Receive a package into buffer if socket is read-ready
  /// @returns Number of bytes read or a syscall error
  /// @returns 0 bytes read if EOF was reached
  /// @param source If source is not nullptr, it is set to tell where did the package come from
  Result<size_t, SyscallError> try_recv_from(std::span<char>,
                                             sockaddr_storage *source = nullptr) noexcept;

  /// @brief Send a package from buffer to specified dest
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>>
  send_to(Reactor &, std::span<char const>, sockaddr_storage const &dest) noexcept;

  /// @brief Send a package from buffer to specified dest if socket is write-ready
  /// @returns Number of bytes written or a syscall error
  Result<size_t, SyscallError> try_send_to(std::span<char const>,
                                           sockaddr_storage const &dest) noexcept;

  /// @brief Free allocated resources and invalidate underlying handle
  void close() noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

} // namespace corosig

#endif
