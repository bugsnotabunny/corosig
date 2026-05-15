#ifndef COROSIG_IO_UDP_SOCKET_HPP
#define COROSIG_IO_UDP_SOCKET_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <span>

namespace corosig {

/// @brief An asynchronous UDP socket
struct UdpSocket {
public:
  /// @brief Construct a UDP socket which refers to invalid os::Handle
  UdpSocket() noexcept = default;

  /// @brief Make a UDP socket which is not bound to any addr. Or get a syscall error
  static Result<UdpSocket, SyscallError> unbound(sa_family_t family = AF_INET) noexcept;

  /// @brief Make a UDP socket which is bound to a specific addr. Or get a syscall error
  static Result<UdpSocket, SyscallError> bound(SockaddrStorage const &local) noexcept;

  UdpSocket(const UdpSocket &) = delete;
  UdpSocket(UdpSocket &&) noexcept = default;
  UdpSocket &operator=(const UdpSocket &) = delete;
  UdpSocket &operator=(UdpSocket &&rhs) noexcept {
    if (this != &rhs) {
      this->~UdpSocket();
      new (this) UdpSocket{std::move(rhs)};
    }
    return *this;
  }

  ~UdpSocket();

  /// @brief Receive a package into buffer
  /// @returns Size of received datagram or a syscall error
  /// @returns 0 bytes read if EOF was reached
  /// @param source If source is not nullptr, it is set to tell where did the package come from
  /// @note If datagram size is greater than given buffer, it is truncated to fit the buffer.
  ///       Returned size is never truncated
  Fut<size_t, Error<AllocationError, SyscallError>>
  recv_from(Reactor &, std::span<char>, SockaddrStorage *source = nullptr) noexcept;

  /// @brief Receive a package into buffer if socket is read-ready
  /// @returns Size of received datagram or a syscall error
  /// @returns 0 bytes read if EOF was reached
  /// @param source If source is not nullptr, it is set to tell where did the package come from
  /// @note If datagram size is greater than given buffer, it is truncated to fit the buffer.
  ///       Returned size is never truncated
  Result<size_t, SyscallError> try_recv_from(std::span<char>,
                                             SockaddrStorage *source = nullptr) noexcept;

  /// @brief Send a package from buffer to specified dest
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>>
  send_to(Reactor &, std::span<char const>, SockaddrStorage const &dest) noexcept;

  /// @brief Send a package from buffer to specified dest if socket is write-ready
  /// @returns Number of bytes written or a syscall error
  Result<size_t, SyscallError> try_send_to(std::span<char const>,
                                           SockaddrStorage const &dest) noexcept;

  /// @brief Free allocated resources and invalidate underlying handle
  void close() noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

} // namespace corosig

#endif
