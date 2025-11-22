#include "corosig/io/UdpSocket.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/PollList.hpp"
#include "posix/FdOps.hpp"

#include <cstddef>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace corosig {

Result<UdpSocket, SyscallError> UdpSocket::writer() noexcept {
  int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  if (fd == -1) {
    return Failure{SyscallError::current()};
  }

  UdpSocket self;
  self.m_fd.value = fd;
  return self;
}

Result<UdpSocket, SyscallError> UdpSocket::readwriter(sockaddr_storage const &local) noexcept {
  COROSIG_TRY(auto self, writer());
  auto len = os::posix::addr_length(local);
  if (bind(self.m_fd.value, (struct sockaddr *)&local, len) == -1) {
    return Failure{SyscallError::current()};
  }
  return self;
}

UdpSocket::~UdpSocket() {
  close();
}

Fut<size_t, Error<AllocationError, SyscallError>>
UdpSocket::recv_from(Reactor &, std::span<char> out, sockaddr_storage *source_addr) noexcept {
  co_await PollEvent{m_fd.value, poll_event_e::CAN_READ};
  co_return try_recv_from(out, source_addr);
}

Result<size_t, SyscallError> UdpSocket::try_recv_from(std::span<char> out,
                                                      sockaddr_storage *source_addr) noexcept {
  socklen_t addrlen;
  socklen_t *addrlen_ptr = source_addr == nullptr ? nullptr : &addrlen;
  ssize_t result =
      ::recvfrom(m_fd.value, out.data(), out.size(), 0, (sockaddr *)source_addr, addrlen_ptr);
  if (result == -1) {
    return Failure{SyscallError::current()};
  }
  return size_t(result);
}

Fut<size_t, Error<AllocationError, SyscallError>> UdpSocket::send_to(
    Reactor &, std::span<char const> message, sockaddr_storage const &dest) noexcept {
  co_await PollEvent{m_fd.value, poll_event_e::CAN_WRITE};
  co_return try_send_to(message, dest);
}

Result<size_t, SyscallError> UdpSocket::try_send_to(std::span<char const> message,
                                                    sockaddr_storage const &dest) noexcept {
  ssize_t result = ::sendto(m_fd.value,
                            message.data(),
                            message.size(),
                            0,
                            (sockaddr const *)&dest,
                            os::posix::addr_length(dest));
  if (result == -1) {
    return Failure{SyscallError::current()};
  }
  return size_t(result);
}

void UdpSocket::close() noexcept {
  return os::posix::close(m_fd.value);
}

[[nodiscard]] os::Handle UdpSocket::underlying_handle() const noexcept {
  return m_fd.value;
}

} // namespace corosig
