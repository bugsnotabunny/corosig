#ifndef COROSIG_IO_TCP_LISTENER_HPP
#define COROSIG_IO_TCP_LISTENER_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/UnixStreamSocket.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <limits>

namespace corosig {

/// @brief An asynchronous listener for incoming unix-domain connections
struct UnixStreamListener {
  struct Options {
    /// @brief An address to listen at
    SockaddrStorage const &addr;

    /// @brief Size of a listener's backlog. When there are more pending connections than that,
    ///        incoming connections may be refused
    size_t backlog_size = std::numeric_limits<size_t>::max();

    /// @brief Allow listening on occupied port if addresses differ with socket occupying that port
    bool reuse_addr = true;

    /// @brief Allow listening on the same port even if addresses are the same
    bool reuse_port = false;
  };

  struct AcceptResult {
    UnixStreamSocket incoming_connection;
    SockaddrStorage incoming_connection_addr;
  };

  /// @brief Construct a ListenerSocket bound to invalid os::Handle
  UnixStreamListener() noexcept = default;

  /// @brief Make new listener socket with given options. If any of underlying syscalls fails, an
  ///        error is returned instead
  static Result<UnixStreamListener, SyscallError> make(Options options) noexcept;

  /// @brief Construct listener end which owns given os::Handle
  /// @warn This is user's responsibility to provide a handle to an actually valid listener socket
  static UnixStreamListener make_from_os_specific_handle(os::Handle handle) noexcept;

  UnixStreamListener(UnixStreamListener const &) = delete;
  UnixStreamListener(UnixStreamListener &&) noexcept = default;
  UnixStreamListener &operator=(UnixStreamListener const &) = delete;
  UnixStreamListener &operator=(UnixStreamListener &&rhs) noexcept {
    if (this != &rhs) {
      this->~UnixStreamListener();
      new (this) UnixStreamListener{std::move(rhs)};
    }
    return *this;
  }

  ~UnixStreamListener();

  /// @brief Accept next incoming connection or get a syscall error trying
  Fut<AcceptResult, Error<AllocationError, SyscallError>> accept(Reactor &) noexcept;

  /// @brief Accept next incoming connection if it is already in a backlog
  Result<AcceptResult, SyscallError> try_accept() noexcept;

  /// @brief Get an address to which socket has been actually bound
  Result<SockaddrStorage, SyscallError> address() const noexcept;

  /// @brief Free allocated resources and invalidate underlying handle
  void close() noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

} // namespace corosig

#endif
