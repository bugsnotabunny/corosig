#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/os/Handle.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <sys/socket.h>

namespace corosig {

struct TcpSocket {
public:
  TcpSocket() noexcept = default;

  static Fut<TcpSocket, Error<AllocationError, SyscallError>>
  connect(sockaddr_storage const &addr) noexcept;

  TcpSocket(const TcpSocket &) = delete;
  TcpSocket(TcpSocket &&) noexcept = default;
  TcpSocket &operator=(const TcpSocket &) = delete;
  TcpSocket &operator=(TcpSocket &&) noexcept = default;

  ~TcpSocket();

  Fut<size_t, Error<AllocationError, SyscallError>> read(std::span<char>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> read_some(std::span<char>) noexcept;
  Result<size_t, SyscallError> try_read_some(std::span<char>) noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>> write(std::span<char const>) noexcept;
  Fut<size_t, Error<AllocationError, SyscallError>> write_some(std::span<char const>) noexcept;
  Result<size_t, SyscallError> try_write_some(std::span<char const>) noexcept;

  void close() noexcept;
  [[nodiscard]] os::Handle underlying_handle() const noexcept;

private:
  SetDefaultOnMove<int, -1> m_fd;
};

} // namespace corosig
