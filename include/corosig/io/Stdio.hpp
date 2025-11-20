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

struct StdOut {
public:
  constexpr static StdOut stdout() noexcept {
    StdOut result;
    result.m_fd = STDOUT_FILENO;
    return result;
  }

  constexpr static StdOut stderr() noexcept {
    StdOut result;
    result.m_fd = STDERR_FILENO;
    return result;
  }

  Fut<size_t, Error<AllocationError, SyscallError>> write(Reactor &,
                                                          std::span<char const>) const noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>>
  write_some(Reactor &, std::span<char const>) const noexcept;
  [[nodiscard]] Result<size_t, SyscallError> try_write_some(std::span<char const>) const noexcept;

  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  StdOut() = default;
  int m_fd = -1;
};

struct StdIn {
public:
  constexpr static StdIn stdin() noexcept {
    StdIn result;
    result.m_fd = STDIN_FILENO;
    return result;
  }

  Fut<size_t, Error<AllocationError, SyscallError>> read(Reactor &, std::span<char>) const noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(Reactor &,
                                                              std::span<char>) const noexcept;
  [[nodiscard]] Result<size_t, SyscallError> try_read_some(std::span<char>) const noexcept;

  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  StdIn() = default;

  int m_fd = -1;
};

constexpr StdOut STDOUT = StdOut::stdout();
constexpr StdOut STDERR = StdOut::stderr();
constexpr StdIn STDIN = StdIn::stdin();

} // namespace corosig

#endif
