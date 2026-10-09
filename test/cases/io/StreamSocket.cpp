#include "corosig/io/StreamSocket.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <fcntl.h>
#include <span>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using namespace corosig;

static_assert(std::movable<StreamSocket>);
static_assert(!std::copyable<StreamSocket>);

struct NonblockingSocketpair {
  int self = -1;
  int peer = -1;

  NonblockingSocketpair() noexcept {
    std::array<int, 2> fds = {-1, -1};
    COROSIG_REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds.data()) == 0);
    self = fds[0];
    peer = fds[1];
    COROSIG_REQUIRE(make_nonblocking(self));
    COROSIG_REQUIRE(make_nonblocking(peer));
  }

  NonblockingSocketpair(NonblockingSocketpair const &) = delete;
  NonblockingSocketpair &operator=(NonblockingSocketpair const &) = delete;

  ~NonblockingSocketpair() noexcept {
    if (self != -1) {
      ::close(self);
    }
    if (peer != -1) {
      ::close(peer);
    }
  }

private:
  static bool make_nonblocking(int fd) noexcept {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
      return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
  }
};

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket default constructed socket has invalid handle") {
  StreamSocket sock;
  COROSIG_REQUIRE(sock.underlying_handle() == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket make_from_os_specific_handle owns given handle") {
  NonblockingSocketpair pair;

  StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
  COROSIG_REQUIRE(sock.underlying_handle() == pair.self);

  pair.self = -1; // handle ownership is transferred to sock
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket move constructor transfers ownership") {
  NonblockingSocketpair pair;

  StreamSocket sock1 = StreamSocket::make_from_os_specific_handle(pair.self);
  pair.self = -1; // handle ownership is transferred to sock1

  StreamSocket sock2(std::move(sock1));

  // NOLINTNEXTLINE(bugprone-use-after-move, clang-analyzer-cplusplus.Move)
  COROSIG_REQUIRE(sock1.underlying_handle() == -1);
  COROSIG_REQUIRE(sock2.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket move assignment transfers ownership") {
  NonblockingSocketpair pair;

  StreamSocket sock1 = StreamSocket::make_from_os_specific_handle(pair.self);
  pair.self = -1; // handle ownership is transferred to sock1

  StreamSocket sock2 = StreamSocket::make_from_os_specific_handle(pair.peer);
  pair.peer = -1; // handle ownership is transferred to sock2
  int old_sock2_handle = sock2.underlying_handle();

  sock2 = std::move(sock1);

  // NOLINTNEXTLINE(bugprone-use-after-move, clang-analyzer-cplusplus.Move)
  COROSIG_REQUIRE(sock1.underlying_handle() == -1);
  COROSIG_REQUIRE(sock2.underlying_handle() >= 0);
  // Previously owned handle must have been closed by the assignment
  COROSIG_REQUIRE(::fcntl(old_sock2_handle, F_GETFL) == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket destructor closes handle") {
  NonblockingSocketpair pair;
  int fd = pair.self;
  pair.self = -1; // handle ownership is transferred to sock

  {
    StreamSocket sock = StreamSocket::make_from_os_specific_handle(fd);
  }

  COROSIG_REQUIRE(::fcntl(fd, F_GETFL) == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket close invalidates handle") {
  NonblockingSocketpair pair;

  StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
  pair.self = -1; // handle ownership is transferred to sock

  sock.close();
  COROSIG_REQUIRE(sock.underlying_handle() == -1);

  std::array<char, 16> buf{};
  auto read_result = sock.try_read_some(buf);
  COROSIG_REQUIRE(!read_result.is_ok());

  auto write_result = sock.try_write_some(std::string_view{"test"});
  COROSIG_REQUIRE(!write_result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket close on invalid handle is a no-op") {
  StreamSocket sock;
  sock.close();
  COROSIG_REQUIRE(sock.underlying_handle() == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket write_some/read_some roundtrip") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    NonblockingSocketpair pair;

    StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
    pair.self = -1; // handle ownership is transferred to sock

    constexpr static std::string_view MSG = "hello";
    COROSIG_CO_TRY(auto written, co_await sock.write_some(r, MSG));
    COROSIG_REQUIRE(written == MSG.size());

    std::array<char, MSG.size()> peer_buf{};
    COROSIG_REQUIRE(::read(pair.peer, peer_buf.data(), peer_buf.size()) ==
                    static_cast<ssize_t>(MSG.size()));
    COROSIG_REQUIRE(std::string_view{peer_buf.data(), peer_buf.size()} == MSG);

    COROSIG_REQUIRE(::write(pair.peer, MSG.data(), MSG.size()) == static_cast<ssize_t>(MSG.size()));

    std::array<char, MSG.size()> buf{};
    COROSIG_CO_TRY(auto read, co_await sock.read_some(r, buf));
    COROSIG_REQUIRE(read == MSG.size());
    COROSIG_REQUIRE(std::string_view{buf.data(), read} == MSG);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket write writes whole buffer") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    NonblockingSocketpair pair;

    StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
    pair.self = -1; // handle ownership is transferred to sock

    std::array<char, 2048> msg{};
    for (size_t i = 0; i < msg.size(); ++i) {
      msg[i] = static_cast<char>(i % 251);
    }

    COROSIG_CO_TRY(auto written,
                   co_await sock.write(r, std::span<char const>{msg.data(), msg.size()}));
    COROSIG_REQUIRE(written == msg.size());

    std::array<char, msg.size()> buf{};
    size_t total_read = 0;
    while (total_read < buf.size()) {
      ssize_t n = ::read(pair.peer, buf.data() + total_read, buf.size() - total_read);
      COROSIG_REQUIRE(n > 0);
      total_read += static_cast<size_t>(n);
    }
    COROSIG_REQUIRE(std::equal(msg.begin(), msg.end(), buf.begin()));

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket read fills whole buffer") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    NonblockingSocketpair pair;

    StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
    pair.self = -1; // handle ownership is transferred to sock

    std::array<char, 2048> msg{};
    for (size_t i = 0; i < msg.size(); ++i) {
      msg[i] = static_cast<char>(i % 251);
    }
    COROSIG_REQUIRE(::write(pair.peer, msg.data(), msg.size()) == static_cast<ssize_t>(msg.size()));

    std::array<char, msg.size()> buf{};
    COROSIG_CO_TRY(auto read, co_await sock.read(r, buf));
    COROSIG_REQUIRE(read == buf.size());
    COROSIG_REQUIRE(std::equal(msg.begin(), msg.end(), buf.begin()));

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket string literal overloads exclude null terminator") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    NonblockingSocketpair pair;

    StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
    pair.self = -1; // handle ownership is transferred to sock

    COROSIG_CO_TRY(auto written, co_await sock.write(r, "hello"));
    COROSIG_REQUIRE(written == 5);

    std::array<char, 5> peer_buf{};
    COROSIG_REQUIRE(::read(pair.peer, peer_buf.data(), peer_buf.size()) ==
                    static_cast<ssize_t>(peer_buf.size()));
    COROSIG_REQUIRE(std::string_view{peer_buf.data(), peer_buf.size()} == "hello");

    COROSIG_CO_TRY(auto written_some, co_await sock.write_some(r, "world"));
    COROSIG_REQUIRE(written_some == 5);

    auto try_written = sock.try_write_some("!");
    COROSIG_REQUIRE(try_written.is_ok());
    COROSIG_REQUIRE(try_written.value() == 1);

    std::array<char, 6> tail_buf{};
    COROSIG_REQUIRE(::read(pair.peer, tail_buf.data(), tail_buf.size()) ==
                    static_cast<ssize_t>(tail_buf.size()));
    COROSIG_REQUIRE(std::string_view{tail_buf.data(), tail_buf.size()} == "world!");

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket read returns 0 on EOF") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    NonblockingSocketpair pair;

    StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
    pair.self = -1; // handle ownership is transferred to sock

    ::close(pair.peer);
    pair.peer = -1;

    std::array<char, 16> buf{};
    COROSIG_CO_TRY(auto read_some, co_await sock.read_some(r, buf));
    COROSIG_REQUIRE(read_some == 0);

    COROSIG_CO_TRY(auto read, co_await sock.read(r, buf));
    COROSIG_REQUIRE(read == 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket read stops at EOF with partial data") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    NonblockingSocketpair pair;

    StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
    pair.self = -1; // handle ownership is transferred to sock

    constexpr static std::string_view MSG = "part";
    COROSIG_REQUIRE(::write(pair.peer, MSG.data(), MSG.size()) == static_cast<ssize_t>(MSG.size()));
    ::close(pair.peer);
    pair.peer = -1;

    std::array<char, 64> buf{};
    COROSIG_CO_TRY(auto read, co_await sock.read(r, buf));
    COROSIG_REQUIRE(read == MSG.size());
    COROSIG_REQUIRE(std::string_view{buf.data(), read} == MSG);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket read_some returns less than buffer size") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    NonblockingSocketpair pair;

    StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
    pair.self = -1; // handle ownership is transferred to sock

    constexpr static std::string_view MSG = "small";
    COROSIG_REQUIRE(::write(pair.peer, MSG.data(), MSG.size()) == static_cast<ssize_t>(MSG.size()));

    std::array<char, 64> buf{};
    COROSIG_CO_TRY(auto read, co_await sock.read_some(r, buf));
    COROSIG_REQUIRE(read == MSG.size());
    COROSIG_REQUIRE(std::string_view{buf.data(), read} == MSG);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket try_write_some and try_read_some work on ready socket") {
  NonblockingSocketpair pair;

  StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
  pair.self = -1; // handle ownership is transferred to sock

  constexpr static std::string_view MSG = "try";
  auto write_result = sock.try_write_some(MSG);
  COROSIG_REQUIRE(write_result.is_ok());
  COROSIG_REQUIRE(write_result.value() == MSG.size());

  std::array<char, MSG.size()> peer_buf{};
  COROSIG_REQUIRE(::read(pair.peer, peer_buf.data(), peer_buf.size()) ==
                  static_cast<ssize_t>(MSG.size()));

  COROSIG_REQUIRE(::write(pair.peer, MSG.data(), MSG.size()) == static_cast<ssize_t>(MSG.size()));

  std::array<char, MSG.size()> buf{};
  auto read_result = sock.try_read_some(buf);
  COROSIG_REQUIRE(read_result.is_ok());
  COROSIG_REQUIRE(read_result.value() == MSG.size());
  COROSIG_REQUIRE(std::string_view{buf.data(), read_result.value()} == MSG);
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket try_read_some fails on invalid handle") {
  StreamSocket sock;

  std::array<char, 16> buf{};
  auto result = sock.try_read_some(buf);
  COROSIG_REQUIRE(!result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket try_write_some fails on invalid handle") {
  StreamSocket sock;

  auto result = sock.try_write_some(std::string_view{"test"});
  COROSIG_REQUIRE(!result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket address returns AF_UNIX for socketpair end") {
  NonblockingSocketpair pair;

  StreamSocket sock = StreamSocket::make_from_os_specific_handle(pair.self);
  pair.self = -1; // handle ownership is transferred to sock

  auto addr = sock.address();
  COROSIG_REQUIRE(addr.is_ok());
  COROSIG_REQUIRE(addr.value().native_storage.ss_family == AF_UNIX);
}

COROSIG_SIGHANDLER_TEST_CASE("StreamSocket address fails on invalid handle") {
  StreamSocket sock;

  auto addr = sock.address();
  COROSIG_REQUIRE(!addr.is_ok());
}
