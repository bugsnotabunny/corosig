#include "corosig/io/StreamSocket.hpp"

#include "posix/FdOps.hpp"

namespace corosig {

StreamSocket StreamSocket::make_from_os_specific_handle(os::Handle handle) noexcept {
  StreamSocket self;
  self.m_fd = handle;
  return self;
}

StreamSocket::~StreamSocket() {
  close();
}

Fut<size_t, Error<AllocationError, SyscallError>> StreamSocket::read(Reactor &r,
                                                                     std::span<char> buf) noexcept {
  return os::posix::read(r, m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
StreamSocket::read_some(Reactor &r, std::span<char> buf) noexcept {
  return os::posix::read_some(r, m_fd.value, buf);
}

Result<size_t, SyscallError> StreamSocket::try_read_some(std::span<char> buf) noexcept {
  return os::posix::try_read_some(m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
StreamSocket::write(Reactor &r, std::span<char const> buf) noexcept {
  return os::posix::write(r, m_fd.value, buf);
}

Fut<size_t, Error<AllocationError, SyscallError>>
StreamSocket::write_some(Reactor &r, std::span<char const> buf) noexcept {
  return os::posix::write_some(r, m_fd.value, buf);
}

Result<size_t, SyscallError> StreamSocket::try_write_some(std::span<char const> buf) noexcept {
  return os::posix::try_write_some(m_fd.value, buf);
}

os::Handle StreamSocket::underlying_handle() const noexcept {
  return m_fd.value;
}

void StreamSocket::close() noexcept {
  return os::posix::close(m_fd.value);
}

Result<SockaddrStorage, SyscallError> StreamSocket::address() const noexcept {
  return os::posix::socket_address(m_fd.value);
}

} // namespace corosig
