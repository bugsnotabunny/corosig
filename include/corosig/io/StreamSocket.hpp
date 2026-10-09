#ifndef COROSIG_IO_STREAM_SOCKET_HPP
#define COROSIG_IO_STREAM_SOCKET_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <span>
#include <string_view>

namespace corosig {

/// @brief An asynchronous stream socket
struct StreamSocket {
public:
  /// @brief Construct a stream socket which refers to invalid os::Handle
  StreamSocket() noexcept = default;

  /// @brief Construct a stream socket which owns given os::Handle
  /// @warn This is user's responsibility to provide a handle to an actually valid StreamSocket
  static StreamSocket make_from_os_specific_handle(os::Handle handle) noexcept;

  StreamSocket(StreamSocket const &) = delete;
  StreamSocket(StreamSocket &&) noexcept = default;
  StreamSocket &operator=(StreamSocket const &) = delete;
  StreamSocket &operator=(StreamSocket &&rhs) noexcept {
    if (this != &rhs) {
      this->~StreamSocket();
      new (this) StreamSocket{std::move(rhs)};
    }
    return *this;
  }

  ~StreamSocket();

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

  /// @brief Write all bytes from string literal, excluding null-terminator
  /// @returns Number of bytes written or a syscall error
  template <size_t N>
  Fut<size_t, Error<AllocationError, SyscallError>> write(Reactor &r,
                                                          char const (&arr)[N]) // NOLINT
      noexcept {
    return write(r, std::string_view{arr});
  }

  /// @brief Write bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(Reactor &,
                                                               std::span<char const>) noexcept;

  /// @brief Write bytes from string literal, excluding null-terminator
  /// @returns Number of bytes written or a syscall error
  template <size_t N>
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(Reactor &r,
                                                               char const (&arr)[N]) // NOLINT
      noexcept {
    return write_some(r, std::string_view{arr});
  }

  /// @brief Write bytes from buffer if socket is write-ready
  /// @returns Number of bytes written or a syscall error
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  /// @brief Try write bytes from string literal, excluding null-terminator
  /// @returns Number of bytes written or a syscall error
  template <size_t N>
  Result<size_t, SyscallError> try_write_some(char const (&arr)[N]) // NOLINT
      noexcept {
    return try_write_some(std::string_view{arr});
  }

  /// @brief Get an address to which socket has been bound
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
