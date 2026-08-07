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

  /// @brief Construct pipe read end which owns given os::Handle
  /// @warn This is user's responsibility to provide a handle to an actually valid pipe read end
  static PipeRead make_from_os_specific_handle(os::Handle handle) noexcept;

  PipeRead(PipeRead const &) = delete;
  PipeRead(PipeRead &&) noexcept = default;
  PipeRead &operator=(PipeRead const &) = delete;
  PipeRead &operator=(PipeRead &&rhs) noexcept {
    if (this != &rhs) {
      this->~PipeRead();
      new (this) PipeRead{std::move(rhs)};
    }
    return *this;
  }

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

  /// @brief Construct pipe write end which owns given os::Handle
  /// @warn This is user's responsibility to provide a handle to an actually valid pipe write end
  static PipeWrite make_from_os_specific_handle(os::Handle handle) noexcept;

  PipeWrite(PipeWrite const &) = delete;
  PipeWrite(PipeWrite &&) noexcept = default;
  PipeWrite &operator=(PipeWrite const &) = delete;
  PipeWrite &operator=(PipeWrite &&rhs) noexcept {
    if (this != &rhs) {
      this->~PipeWrite();
      new (this) PipeWrite{std::move(rhs)};
    }
    return *this;
  }

  ~PipeWrite();

  /// @brief Write all bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> write(Reactor &,
                                                          std::span<char const>) noexcept;

  /// @brief Write all bytes from string literal, excluding null-terminator
  /// @returns Number of bytes written or a syscall error
  template <size_t N>
  Fut<size_t, Error<AllocationError, SyscallError>>
  write(Reactor &r,
        char const (&arr)[N]) noexcept // NOLINT(modernize-avoid-c-arrays)
  {
    return write(r, std::string_view{arr});
  }

  /// @brief Write bytes from buffer
  /// @returns Number of bytes written or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(Reactor &,
                                                               std::span<char const>) noexcept;

  /// @brief Write bytes from string literal, excluding null-terminator
  /// @returns Number of bytes written or a syscall error
  template <size_t N>
  Fut<size_t, Error<AllocationError, SyscallError>>
  write_some(Reactor &r,
             char const (&arr)[N]) noexcept // NOLINT(modernize-avoid-c-arrays)
  {
    return write_some(r, std::string_view{arr});
  }

  /// @brief Write bytes from buffer if output is write-ready
  /// @returns Number of bytes written or a syscall error
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  /// @brief Try write bytes from string literal, excluding null-terminator
  /// @returns Number of bytes written or a syscall error
  template <size_t N>
  Result<size_t, SyscallError>
  try_write_some(char const (&arr)[N]) noexcept // NOLINT(modernize-avoid-c-arrays)
  {
    return try_write_some(std::string_view{arr});
  }

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
  /// @brief Create a new pipe with read and write ends
  static Result<PipePair, SyscallError> make() noexcept;

  PipeRead read;
  PipeWrite write;
};

} // namespace corosig

#endif
