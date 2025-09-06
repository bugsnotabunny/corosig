#include "corosig/io/Pipe.hpp"
#include "corosig/ErrorTypes.hpp"
#include "posix/FdOps.hpp"
#include <fcntl.h>

namespace corosig {

PipeRead::~PipeRead() {
  close();
}

PipeWrite::~PipeWrite() {
  close();
}

Fut<size_t, Error<AllocationError, SyscallError>> PipeRead::read(std::span<char> buf) noexcept {
  return os::read(m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeRead::read_some(std::span<char> buf) noexcept {
  return os::read_some(m_fd.value, buf);
}

Result<size_t, SyscallError> PipeRead::try_read_some(std::span<char> buf) noexcept {
  return os::try_read_some(m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeWrite::write(std::span<char const> buf) noexcept {
  return os::write(m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeWrite::write_some(std::span<char const> buf) noexcept {
  return os::write_some(m_fd.value, buf);
}

Result<size_t, SyscallError> PipeWrite::try_write_some(std::span<char const> buf) noexcept {
  return os::try_write_some(m_fd.value, buf);
}

void PipeWrite::close() noexcept {
  return os::close(m_fd.value);
}

os::Handle PipeWrite::underlying_handle() const noexcept {
  return m_fd.value;
}

os::Handle PipeRead::underlying_handle() const noexcept {
  return m_fd.value;
}

void PipeRead::close() noexcept {
  return os::close(m_fd.value);
}

Result<PipePair, SyscallError> PipePair::make() noexcept {
  int fds[2];
  if (::pipe2(fds, O_NONBLOCK) == -1) {
    return SyscallError{errno};
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
