#include "corosig/io/Pipe.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/reactor/PollList.hpp"
#include <fcntl.h>

namespace corosig {

PipeRead::~PipeRead() {
  close();
}

void PipeRead::close() noexcept {
  if (m_fd.value >= 0) {
    ::close(m_fd.value);
    m_fd.value = -1;
  }
}

PipeWrite::~PipeWrite() {
  close();
}

Fut<size_t, Error<AllocationError, SyscallError>> PipeRead::read(std::span<char> buf) noexcept {
  size_t read = 0;
  while (read < buf.size()) {
    co_await PollEvent{m_fd.value, poll_event_e::READ};

    Result current_read = try_read_some(buf);
    if (current_read.has_value()) {
      size_t value = current_read.assume_value();
      if (value == 0) {
        break;
      }
      read += value;
    } else if (read != 0) {
      break;
    } else {
      co_return current_read.assume_error();
    }
  }
  co_return read;
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeRead::read_some(std::span<char> buf) noexcept {
  co_await PollEvent{m_fd.value, poll_event_e::READ};
  co_return try_read_some(buf);
}

Result<size_t, SyscallError> PipeRead::try_read_some(std::span<char> buf) noexcept {
  ssize_t n = ::read(m_fd.value, buf.data(), buf.size());
  if (n == -1) {
    return SyscallError::current();
  }
  return size_t(n);
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeWrite::write(std::span<char const> buf) noexcept {
  size_t written = 0;
  while (written < buf.size()) {
    co_await PollEvent{m_fd.value, poll_event_e::WRITE};

    Result current_write = try_write_some(buf);
    if (current_write.has_value()) {
      written += current_write.assume_value();
    } else if (written == 0) {
      co_return SyscallError::current();
    } else {
      break;
    }
  }
  co_return written;
}

Fut<size_t, Error<AllocationError, SyscallError>>
PipeWrite::write_some(std::span<char const> buf) noexcept {
  co_await PollEvent{m_fd.value, poll_event_e::WRITE};
  co_return try_write_some(buf);
}

Result<size_t, SyscallError> PipeWrite::try_write_some(std::span<char const> buf) noexcept {
  ssize_t n = ::write(m_fd.value, buf.data(), buf.size());
  if (n == -1) {
    return SyscallError::current();
  }
  return size_t(n);
}

void PipeWrite::close() noexcept {
  if (m_fd.value >= 0) {
    ::close(m_fd.value);
    m_fd.value = -1;
  }
}

os::Handle PipeWrite::underlying_handle() const noexcept {
  return m_fd.value;
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
