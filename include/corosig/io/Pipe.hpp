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

struct PipeRead {
public:
  PipeRead() noexcept = default;

  static PipeRead stdin() noexcept;

  PipeRead(const PipeRead &) = delete;
  PipeRead(PipeRead &&) noexcept = default;
  PipeRead &operator=(const PipeRead &) = delete;
  PipeRead &operator=(PipeRead &&) noexcept = default;

  ~PipeRead();

  Fut<size_t, Error<AllocationError, SyscallError>> read(Reactor &, std::span<char>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(Reactor &, std::span<char>) noexcept;
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

  static PipeWrite stdout() noexcept;
  static PipeWrite stderr() noexcept;

  PipeWrite(const PipeWrite &) = delete;
  PipeWrite(PipeWrite &&) noexcept = default;
  PipeWrite &operator=(const PipeWrite &) = delete;
  PipeWrite &operator=(PipeWrite &&) noexcept = default;

  ~PipeWrite();

  Fut<size_t, Error<AllocationError, SyscallError>> write(Reactor &,
                                                          std::span<char const>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(Reactor &,
                                                               std::span<char const>) noexcept;
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

#endif
