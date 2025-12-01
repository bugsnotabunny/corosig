#ifndef COROSIG_IO_FILE_HPP
#define COROSIG_IO_FILE_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/util/Bitmask.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <cstddef>
#include <fcntl.h>
#include <span>

namespace corosig {

/// @brief An asynchronous filesystem entry
struct File {
public:
  /// @brief Flags for open operation
  /// @note Exact values are os-specific
  enum class OpenFlags {
    UNSPECIFIED = 0,
    APPEND = O_APPEND,
    CREATE = O_CREAT,
    TRUNCATE = O_TRUNC,
    WRONLY = O_WRONLY,
    RDONLY = O_RDONLY,
  };

  /// @brief Permissions to open file with
  /// @note Exact values are os-specific
  enum class OpenPerms : int {
    UNSPECIFIED,
    // TODO
  };

  /// @brief Construct a File bound to invalid os::Handle
  File() noexcept = default;

  /// @brief Open a file at path with specified flags and permissions
  static Fut<File, Error<AllocationError, SyscallError>>
  open(Reactor &,
       char const *path,
       OpenFlags = OpenFlags::UNSPECIFIED,
       OpenPerms = OpenPerms::UNSPECIFIED) noexcept;

  File(const File &) = delete;
  File(File &&) noexcept = default;
  File &operator=(const File &) = delete;
  File &operator=(File &&) noexcept = default;

  ~File();

  /// @brief Read bytes into buffer until it is full
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> read(Reactor &, std::span<char>) noexcept;

  /// @brief Read bytes into buffer
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(Reactor &, std::span<char>) noexcept;

  /// @brief Read bytes into buffer if file is read-ready
  /// @returns 0 bytes read if EOF was reached
  /// @returns Number of bytes read or a syscall error
  Result<size_t, SyscallError> try_read_some(std::span<char>) noexcept;

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
  SetDefaultOnMove<int, -1> m_fd;
};

template <>
struct IsBitmask<File::OpenFlags> : std::true_type {};

template <>
struct IsBitmask<File::OpenPerms> : std::true_type {};

} // namespace corosig

#endif
