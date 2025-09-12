
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <cstddef>
#include <utility>

namespace corosig {

struct Socket {
public:
  Socket() noexcept = default;

  Fut<Socket, Error<AllocationError, SyscallError>> connect() noexcept {
  }

  Socket(const Socket &) = delete;
  Socket(Socket &&) noexcept = default;
  Socket &operator=(const Socket &) = delete;
  Socket &operator=(Socket &&) noexcept = default;

  ~Socket();

  Fut<size_t, Error<AllocationError, SyscallError>> read(std::span<char>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(std::span<char>) noexcept;
  Result<size_t, SyscallError> try_read_some(std::span<char>) noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>> write(std::span<char const>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(std::span<char const>) noexcept;
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  void close() noexcept;
  os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

} // namespace corosig
