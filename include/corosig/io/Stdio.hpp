#ifndef COROSIG_IO_STDIO_HPP
#define COROSIG_IO_STDIO_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <cstddef>
#include <span>
#include <unistd.h>

namespace corosig {

/// @brief Asynchronous API for writing into stdout/stderr
struct StdOut {
public:
  /// @brief Get writer bound to stdout
  constexpr static StdOut stdout() noexcept {
    StdOut result;
    result.m_fd = STDOUT_FILENO;
    return result;
  }

  /// @brief Get writer bound to stderr
  constexpr static StdOut stderr() noexcept {
    StdOut result;
    result.m_fd = STDERR_FILENO;
    return result;
  }

  /// @brief Write all bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> write(Reactor &,
                                                          std::span<char const>) const noexcept;

  /// @brief Write bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>>
  write_some(Reactor &, std::span<char const>) const noexcept;

  /// @brief Write bytes from buffer if output is write-ready
  /// @returns Number of bytes written or a syscall error
  [[nodiscard]] Result<size_t, SyscallError> try_write_some(std::span<char const>) const noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  StdOut() = default;
  int m_fd = -1;
};

/// @brief Asynchronous API for reading from stdin
struct StdIn {
public:
  /// @brief Get reader bound to stdin
  constexpr static StdIn stdin() noexcept {
    StdIn result;
    result.m_fd = STDIN_FILENO;
    return result;
  }

  /// @brief Read bytes into buffer until it is full
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> read(Reactor &, std::span<char>) const noexcept;

  /// @brief Read bytes into buffer
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(Reactor &,
                                                              std::span<char>) const noexcept;

  /// @brief Read bytes into buffer if input is read-ready
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  [[nodiscard]] Result<size_t, SyscallError> try_read_some(std::span<char>) const noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  StdIn() = default;

  int m_fd = -1;
};

/// @brief Shorthand for calling StdOut::stdout()
constexpr StdOut STDOUT = StdOut::stdout();

/// @brief Shorthand for calling StdOut::stderr()
constexpr StdOut STDERR = StdOut::stderr();

/// @brief Shorthand for calling StdIn::stdin()
constexpr StdIn STDIN = StdIn::stdin();

} // namespace corosig

#endif
