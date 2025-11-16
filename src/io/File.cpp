#include "corosig/io/File.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "posix/FdOps.hpp"

#include <fcntl.h>
#include <unistd.h>

namespace corosig {

File::~File() {
  close();
}

Fut<size_t, Error<AllocationError, SyscallError>> File::read(Reactor &r,
                                                             std::span<char> buf) noexcept {
  return os::posix::read(r, m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>> File::read_some(Reactor &r,
                                                                  std::span<char> buf) noexcept {
  return os::posix::read_some(r, m_fd.value, buf);
}

Result<size_t, SyscallError> File::try_read_some(std::span<char> buf) noexcept {
  return os::posix::try_read_some(m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>> File::write(Reactor &r,
                                                              std::span<char const> buf) noexcept {
  return os::posix::write(r, m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
File::write_some(Reactor &r, std::span<char const> buf) noexcept {
  return os::posix::write_some(r, m_fd.value, buf);
}

Result<size_t, SyscallError> File::try_write_some(std::span<char const> buf) noexcept {
  return os::posix::try_write_some(m_fd.value, buf);
}

os::Handle File::underlying_handle() const noexcept {
  return m_fd.value;
}

void File::close() noexcept {
  if (m_fd.value >= 0) {
    ::fsync(m_fd.value);
    ::close(m_fd.value);
    m_fd.value = -1;
  }
}

Fut<File, Error<AllocationError, SyscallError>>
File::open(Reactor &, char const *path, OpenFlags flags, OpenPerms perms) noexcept {
  // Actually a blocking open. Future is used as interface in order to provide capability for other
  // systems to implement nonblocking open
  int fd = ::open(path, int(flags) | O_NONBLOCK, int(perms) | S_IRWXU | S_IRWXG | S_IRWXO);
  if (fd == -1) {
    co_return Failure{SyscallError::current()};
  }
  File self;
  self.m_fd = fd;
  co_return self;
}

} // namespace corosig
