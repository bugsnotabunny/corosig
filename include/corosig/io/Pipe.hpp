
#include "corosig/coro.hpp"
#include "corosig/error_types.hpp"
#include "corosig/result.hpp"
#include "corosig/util/set_on_move.hpp"
#include <fcntl.h>
#include <unistd.h>
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

  ~PipeRead() {
    close();
  }

  Result<size_t, SyscallError> try_read(void *buffer, size_t size) noexcept {
    ssize_t n = ::read(fd.value, buffer, size);
    if (n == -1) {
      return SyscallError{errno};
    }
    return size_t(n);
  }

  void close() noexcept {
    if (fd.value >= 0) {
      ::close(fd.value);
      fd.value = -1;
    }
  }

private:
  friend PipePair;
  SetDefaultOnMove<int, -1> fd;
};

struct PipeWrite {
public:
  PipeWrite() = default;

  PipeWrite(const PipeWrite &) = delete;
  PipeWrite(PipeWrite &&) = default;
  PipeWrite &operator=(const PipeWrite &) = delete;
  PipeWrite &operator=(PipeWrite &&) = default;

  ~PipeWrite() {
    close();
  }

  Result<size_t, SyscallError> try_write(const void *buffer, size_t size) noexcept {
    ssize_t n = ::write(fd.value, buffer, size);
    if (n == -1) {
      return SyscallError{errno};
    }
    return size_t(n);
  }

  void close() noexcept {
    if (fd.value >= 0) {
      ::close(fd.value);
      fd.value = -1;
    }
  }

private:
  friend PipePair;
  SetDefaultOnMove<int, -1> fd;
};

struct PipePair {
  Result<PipePair, SyscallError> make() noexcept {
    int fds[2];
    if (::pipe2(fds, O_NONBLOCK) == -1) {
      return SyscallError{errno};
    }
    PipeRead read;
    PipeWrite write;

    read.fd.value = fds[0];
    write.fd.value = fds[1];

    return PipePair{
        .read = std::move(read),
        .write = std::move(write),
    };
  }

  PipeRead read;
  PipeWrite write;
};

} // namespace corosig
