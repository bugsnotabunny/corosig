#ifndef COROSIG_PRIVATE_FDOPS_HPP
#define COROSIG_PRIVATE_FDOPS_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/PollEvent.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/StreamSocket.hpp"

#include <cstddef>
#include <netinet/in.h>
#include <span>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef _POSIX_VERSION
static_assert(false, "Platform-specific file included on wrong platform");
#endif

namespace corosig::os::posix {

Fut<size_t, Error<AllocationError, SyscallError>>
write(Reactor &, int fd, std::span<char const>) noexcept;
Fut<size_t, Error<AllocationError, SyscallError>>
write_some(Reactor &, int fd, std::span<char const>) noexcept;
Result<size_t, SyscallError> try_write_some(int fd, std::span<char const>) noexcept;

Fut<size_t, Error<AllocationError, SyscallError>> read(Reactor &, int fd, std::span<char>) noexcept;
Fut<size_t, Error<AllocationError, SyscallError>>
read_some(Reactor &, int fd, std::span<char>) noexcept;
Result<size_t, SyscallError> try_read_some(int fd, std::span<char>) noexcept;

void close(int &fd) noexcept;

socklen_t addr_length(sockaddr_storage const &storage) noexcept;
Result<SockaddrStorage, SyscallError> socket_address(int fd) noexcept;

Result<void, SyscallError> set_nonblocking_mode(int fd) noexcept;

Result<StreamSocket, SyscallError> make_nonblocking_stream_socket(sa_family_t family,
                                                                  int protocol) noexcept;

struct PollConnect : PollEvent {
  static Result<PollConnect, SyscallError> make(os::Handle sock,
                                                SockaddrStorage const &target) noexcept;
  Result<void, SyscallError> await_resume() noexcept;

private:
  using PollEvent::PollEvent;
};

} // namespace corosig::os::posix

#endif
