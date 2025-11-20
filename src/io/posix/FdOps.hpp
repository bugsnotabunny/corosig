#ifndef COROSIG_PRIVATE_FDOPS_HPP
#define COROSIG_PRIVATE_FDOPS_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"

#include <cstddef>
#include <netinet/in.h>
#include <span>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef __unix__
static_assert(false, "Platform-specific file included on wrong platform");
#endif

namespace corosig::os::posix {

Fut<size_t, Error<AllocationError, SyscallError>> write(Reactor &, int fd,
                                                        std::span<char const>) noexcept;
Fut<size_t, Error<AllocationError, SyscallError>> write_some(Reactor &, int fd,
                                                             std::span<char const>) noexcept;
Result<size_t, SyscallError> try_write_some(int fd, std::span<char const>) noexcept;

Fut<size_t, Error<AllocationError, SyscallError>> read(Reactor &, int fd, std::span<char>) noexcept;
Fut<size_t, Error<AllocationError, SyscallError>> read_some(Reactor &, int fd,
                                                            std::span<char>) noexcept;
Result<size_t, SyscallError> try_read_some(int fd, std::span<char>) noexcept;

void close(int &fd) noexcept;

socklen_t addr_length(sockaddr_storage const &storage) noexcept;

} // namespace corosig::os::posix

#endif
