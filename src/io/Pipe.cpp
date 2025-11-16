#include "corosig/io/Pipe.hpp"

#include "corosig/ErrorTypes.hpp"
#include "posix/FdOps.hpp"

#include <fcntl.h>
#include <utility>

namespace corosig {

PipeRead::~PipeRead() {
  close();
}

PipeWrite::~PipeWrite() {
  close();
}

Fut<size_t, Error<AllocationError, SyscallError>> PipeRead::read(Reactor &r,
                                                                 std::span<char> buf) noexcept {
  return os::posix::read(r, m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeRead::read_some(Reactor &r, std::span<char> buf) noexcept {
  return os::posix::read_some(r, m_fd.value, buf);
}

Result<size_t, SyscallError> PipeRead::try_read_some(std::span<char> buf) noexcept {
  return os::posix::try_read_some(m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeWrite::write(Reactor &r, std::span<char const> buf) noexcept {
  return os::posix::write(r, m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeWrite::write_some(Reactor &r, std::span<char const> buf) noexcept {
  return os::posix::write_some(r, m_fd.value, buf);
}

Result<size_t, SyscallError> PipeWrite::try_write_some(std::span<char const> buf) noexcept {
  return os::posix::try_write_some(m_fd.value, buf);
}

void PipeWrite::close() noexcept {
  return os::posix::close(m_fd.value);
}

os::Handle PipeWrite::underlying_handle() const noexcept {
  return m_fd.value;
}

os::Handle PipeRead::underlying_handle() const noexcept {
  return m_fd.value;
}

void PipeRead::close() noexcept {
  return os::posix::close(m_fd.value);
}

Result<PipePair, SyscallError> PipePair::make() noexcept {
  std::array<int, 2> fds;
  if (::pipe2(fds.data(), O_NONBLOCK) == -1) {
    return failure(SyscallError{errno});
  }
  PipeRead read;
  PipeWrite write;

  read.m_fd.value = fds[0];
  write.m_fd.value = fds[1];

  return PipePair{
      .read = std::move(read),
      .write = std::move(write),
  };
}

} // namespace corosig
