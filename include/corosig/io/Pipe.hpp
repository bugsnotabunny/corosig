
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <cstddef>
#include <utility>

namespace corosig {

struct PipePair;

struct PipeRead {
public:
  PipeRead() = default;

  PipeRead(const PipeRead &) = delete;
  PipeRead(PipeRead &&) = default;
  PipeRead &operator=(const PipeRead &) = delete;
  PipeRead &operator=(PipeRead &&) = default;

  ~PipeRead();

  Fut<size_t, Error<AllocationError, SyscallError>> read(std::span<char>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(std::span<char>) noexcept;
  Result<size_t, SyscallError> try_read_some(std::span<char>) noexcept;

  void close() noexcept;

  decltype(auto) platform_specific_descriptor() const noexcept {
    return m_fd.value;
  }

private:
  friend PipePair;
  SetDefaultOnMove<int, -1> m_fd;
};

struct PipeWrite {
public:
  PipeWrite() = default;

  PipeWrite(const PipeWrite &) = delete;
  PipeWrite(PipeWrite &&) = default;
  PipeWrite &operator=(const PipeWrite &) = delete;
  PipeWrite &operator=(PipeWrite &&) = default;

  ~PipeWrite();

  Fut<size_t, Error<AllocationError, SyscallError>> write(std::span<char const>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(std::span<char const>) noexcept;
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  void close() noexcept;
  os::Handle underlying_handle() const noexcept;

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
