
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/Bitmask.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <cstddef>
#include <fcntl.h>

namespace corosig {

struct File {
public:
  /// Exact values are os-specific
  enum class OpenFlags {
    UNSPECIFIED = 0,
    APPEND = O_APPEND,
    CREATE = O_CREAT,
    TRUNCATE = O_TRUNC,
    WRONLY = O_WRONLY,
    RDONLY = O_RDONLY,
  };

  /// Exact values are os-specific
  enum class OpenPerms : int {
    UNSPECIFIED,
    // TODO
  };

  File() noexcept = default;

  static Fut<File, Error<AllocationError, SyscallError>>
  open(char const *path, OpenFlags = OpenFlags::UNSPECIFIED,
       OpenPerms = OpenPerms::UNSPECIFIED) noexcept;

  File(const File &) = delete;
  File(File &&) noexcept = default;
  File &operator=(const File &) = delete;
  File &operator=(File &&) noexcept = default;

  ~File();

  Fut<size_t, Error<AllocationError, SyscallError>> read(std::span<char>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(std::span<char>) noexcept;
  Result<size_t, SyscallError> try_read_some(std::span<char>) noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>> write(std::span<char const>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(std::span<char const>) noexcept;
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  void close() noexcept;
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

template <>
struct IsBitmask<File::OpenFlags> : std::true_type {};

template <>
struct IsBitmask<File::OpenPerms> : std::true_type {};

} // namespace corosig
