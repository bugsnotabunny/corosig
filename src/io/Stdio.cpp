#include "corosig/io/Stdio.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "posix/FdOps.hpp"

#include <fcntl.h>
#include <unistd.h>

namespace corosig {

Fut<size_t, Error<AllocationError, SyscallError>> StdIn::read(Reactor &r,
                                                              std::span<char> buf) const noexcept {
  return os::posix::read(r, m_fd, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
StdIn::read_some(Reactor &r, std::span<char> buf) const noexcept {
  return os::posix::read_some(r, m_fd, buf);
}

Result<size_t, SyscallError> StdIn::try_read_some(std::span<char> buf) const noexcept {
  return os::posix::try_read_some(m_fd, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
StdOut::write(Reactor &r, std::span<char const> buf) const noexcept {
  return os::posix::write(r, m_fd, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
StdOut::write_some(Reactor &r, std::span<char const> buf) const noexcept {
  return os::posix::write_some(r, m_fd, buf);
}

Result<size_t, SyscallError> StdOut::try_write_some(std::span<char const> buf) const noexcept {
  return os::posix::try_write_some(m_fd, buf);
}

os::Handle StdOut::underlying_handle() const noexcept {
  return m_fd;
}

} // namespace corosig
