#ifndef COROSIG_IO_PIPE_HPP
#define COROSIG_IO_PIPE_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <cstddef>
#include <span>

namespace corosig {

struct PipePair;

/// @brief Pipe end capable of reading
struct PipeRead {
public:
  /// @brief Construct a PipeRead bound to invalid os::Handle
  PipeRead() noexcept = default;

  PipeRead(const PipeRead &) = delete;
  PipeRead(PipeRead &&) noexcept = default;
  PipeRead &operator=(const PipeRead &) = delete;
  PipeRead &operator=(PipeRead &&) noexcept = default;

  ~PipeRead();

  /// @brief Read bytes into buffer until it is full
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> read(Reactor &, std::span<char>) noexcept;

  /// @brief Read bytes into buffer
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(Reactor &, std::span<char>) noexcept;

  /// @brief Read bytes into buffer if input is read-ready
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Result<size_t, SyscallError> try_read_some(std::span<char>) noexcept;

  /// @brief Free allocated resources and invalidate underlying handle
  void close() noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  friend PipePair;
  SetDefaultOnMove<int, -1> m_fd;
};

/// @brief Pipe end capable of writing
struct PipeWrite {
public:
  /// @brief Construct a PipeWrite bound to invalid os::Handle
  PipeWrite() noexcept = default;

  PipeWrite(const PipeWrite &) = delete;
  PipeWrite(PipeWrite &&) noexcept = default;
  PipeWrite &operator=(const PipeWrite &) = delete;
  PipeWrite &operator=(PipeWrite &&) noexcept = default;

  ~PipeWrite();

  /// @brief Write all bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> write(Reactor &,
                                                          std::span<char const>) noexcept;

  /// @brief Write bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(Reactor &,
                                                               std::span<char const>) noexcept;

  /// @brief Write bytes from buffer if output is write-ready
  /// @returns Number of bytes written or a syscall error
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  /// @brief Free allocated resources and invalidate underlying handle
  void close() noexcept;

  /// @brief Get OS-specific underlying handle
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  friend PipePair;
  SetDefaultOnMove<int, -1> m_fd;
};

/// @brief Both ends of a pipe
struct PipePair {
  /// @brief Make a pipe or get a syscall error
  static Result<PipePair, SyscallError> make() noexcept;

  PipeRead read;
  PipeWrite write;
};

} // namespace corosig

#endif
