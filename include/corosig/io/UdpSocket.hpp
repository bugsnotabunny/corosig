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

struct UdpSocket {
public:
  UdpSocket() noexcept = default;

  static Result<UdpSocket, SyscallError> writer() noexcept;
  static Result<UdpSocket, SyscallError> readwriter(sockaddr_storage const &local) noexcept;

  UdpSocket(const UdpSocket &) = delete;
  UdpSocket(UdpSocket &&) noexcept = default;
  UdpSocket &operator=(const UdpSocket &) = delete;
  UdpSocket &operator=(UdpSocket &&) noexcept = default;

  ~UdpSocket();

  Fut<size_t, Error<AllocationError, SyscallError>>
  recv_from(Reactor &, std::span<char>, sockaddr_storage *source = nullptr) noexcept;

  Result<size_t, SyscallError> try_recv_from(std::span<char>,
                                             sockaddr_storage *source = nullptr) noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>> send_to(Reactor &, std::span<char const>,
                                                            sockaddr_storage const &dest) noexcept;

  Result<size_t, SyscallError> try_send_to(std::span<char const>,
                                           sockaddr_storage const &dest) noexcept;

  void close() noexcept;
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

} // namespace corosig

#endif
