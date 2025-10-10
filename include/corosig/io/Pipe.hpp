
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <cstddef>

namespace corosig {

struct PipePair;

struct PipeRead {
public:
  PipeRead() noexcept = default;

  PipeRead(const PipeRead &) = delete;
  PipeRead(PipeRead &&) noexcept = default;
  PipeRead &operator=(const PipeRead &) = delete;
  PipeRead &operator=(PipeRead &&) noexcept = default;

  ~PipeRead();

  Fut<size_t, Error<AllocationError, SyscallError>> read(std::span<char>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(std::span<char>) noexcept;
  Result<size_t, SyscallError> try_read_some(std::span<char>) noexcept;

  void close() noexcept;
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  friend PipePair;
  SetDefaultOnMove<int, -1> m_fd;
};

struct PipeWrite {
public:
  PipeWrite() noexcept = default;

  PipeWrite(const PipeWrite &) = delete;
  PipeWrite(PipeWrite &&) noexcept = default;
  PipeWrite &operator=(const PipeWrite &) = delete;
  PipeWrite &operator=(PipeWrite &&) noexcept = default;

  ~PipeWrite();

  Fut<size_t, Error<AllocationError, SyscallError>> write(std::span<char const>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(std::span<char const>) noexcept;
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  void close() noexcept;
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  friend PipePair;
  SetDefaultOnMove<int, -1> m_fd;
};

struct PipePair {
  static Result<PipePair, SyscallError> make() noexcept;

  PipeRead read;
  PipeWrite write;
};

} // namespace corosig
