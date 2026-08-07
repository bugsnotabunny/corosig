#include "corosig/io/ListenerSocket.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/TcpSocket.hpp"
#include "corosig/reactor/PollList.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "posix/FdOps.hpp"

#include <fcntl.h>
#include <limits>
#include <netinet/tcp.h>
#include <sys/socket.h>

namespace corosig {

Result<ListenerSocket, SyscallError> ListenerSocket::make(Options const &options) noexcept {
  int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (fd == -1) {
    return Failure{SyscallError::current()};
  }

  auto listener = ListenerSocket::make_from_os_specific_handle(fd);

  int reuse_addr = static_cast<int>(options.reuse_addr);
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) != 0) {
    return Failure{SyscallError::current()};
  }

  int reuse_port = static_cast<int>(options.reuse_port);
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port)) != 0) {
    return Failure{SyscallError::current()};
  }

  if (::bind(fd,
             reinterpret_cast<sockaddr const *>(&options.addr),
             os::posix::addr_length(options.addr.native_storage)) != 0) {
    return Failure{SyscallError::current()};
  }

  auto backlog =
      static_cast<int>(std::max<size_t>(options.backlog_size, std::numeric_limits<int>::max()));
  if (::listen(fd, backlog) != 0) {
    return Failure{SyscallError::current()};
  }

  int on = 1;
  // Not a hard failure. Just a little bit of performance loss
  (void)::setsockopt(fd, SOL_TCP, TCP_NODELAY, &on, sizeof(on));

  return listener;
}

ListenerSocket ListenerSocket::make_from_os_specific_handle(os::Handle handle) noexcept {
  ListenerSocket sock;
  sock.m_fd = handle;
  return sock;
}

ListenerSocket::~ListenerSocket() {
  close();
}

Fut<AcceptResult, Error<AllocationError, SyscallError>> ListenerSocket::accept(Reactor &) noexcept {
  co_await PollEvent{m_fd.value, PollEventExpectance::CAN_READ};

  AcceptResult result;

  socklen_t incoming_addr_len = sizeof(SockaddrStorage);
  int fd = ::accept(m_fd.value,
                    reinterpret_cast<sockaddr *>(&result.incoming_connection_addr.native_storage),
                    &incoming_addr_len);
  if (fd == -1) {
    co_return Failure{SyscallError::current()};
  }

  result.incoming_connection = TcpSocket::make_from_os_specific_handle(fd);

  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    co_return Failure{SyscallError::current()};
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    co_return Failure{SyscallError::current()};
  }

  int on = 1;
  // Not a hard failure. Just a little bit of performance loss
  (void)::setsockopt(fd, SOL_TCP, TCP_NODELAY, &on, sizeof(on));

  co_return result;
}

void ListenerSocket::close() noexcept {
  return os::posix::close(m_fd.value);
}

[[nodiscard]] os::Handle ListenerSocket::underlying_handle() const noexcept {
  return m_fd.value;
}

} // namespace corosig
