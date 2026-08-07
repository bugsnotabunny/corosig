#ifndef COROSIG_IO_LISTENER_SOCKET_HPP
#define COROSIG_IO_LISTENER_SOCKET_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/TcpSocket.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <limits>

namespace corosig {

struct AcceptResult {
  TcpSocket incoming_connection;
  SockaddrStorage incoming_connection_addr;
};

/// @brief An asynchronous listener socket
struct ListenerSocket {
  struct Options {
    SockaddrStorage addr;
    size_t backlog_size = std::numeric_limits<size_t>::max();
    bool reuse_addr = true;
    bool reuse_port = true;
  };

  /// @brief Construct a ListenerSocket bound to invalid os::Handle
  ListenerSocket() noexcept = default;

  /// @brief Make new listener socket with given options. If any of underlying syscalls fails, an
  ///        error is returned instead
  static Result<ListenerSocket, SyscallError> make(Options const &options) noexcept;

  /// @brief Construct listener end which owns given os::Handle
  /// @warn This is user's responsibility to provide a handle to an actually valid listener socket
  static ListenerSocket make_from_os_specific_handle(os::Handle handle) noexcept;

  ListenerSocket(ListenerSocket const &) = delete;
  ListenerSocket(ListenerSocket &&) noexcept = default;
  ListenerSocket &operator=(ListenerSocket const &) = delete;
  ListenerSocket &operator=(ListenerSocket &&rhs) noexcept {
    if (this != &rhs) {
      this->~ListenerSocket();
      new (this) ListenerSocket{std::move(rhs)};
    }
    return *this;
  }

  ~ListenerSocket();

  /// @brief Accept next incoming connection
  Fut<AcceptResult, Error<AllocationError, SyscallError>> accept(Reactor &) noexcept;

  /// @brief Free allocated resources and invalidate underlying handle
  void close() noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

} // namespace corosig

#endif
