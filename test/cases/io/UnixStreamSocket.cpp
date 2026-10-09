#include "corosig/io/UnixStreamSocket.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/StreamSocket.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <array>
#include <concepts>
#include <cstring>
#include <span>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

using namespace corosig;

static_assert(std::convertible_to<UnixStreamSocket, StreamSocket>);
static_assert(std::movable<UnixStreamSocket>);
static_assert(!std::copyable<UnixStreamSocket>);

struct UnixTestPath {
  SockaddrStorage storage{};
  char path[48]{};

  UnixTestPath() noexcept {
    static_assert(sizeof(sockaddr_storage) >= sizeof(sockaddr_un));
    sockaddr_un path_probe{};

    constexpr static std::string_view TEMPLATE = "/tmp/corosig_test_";
    size_t i = 0;
    for (char c : TEMPLATE) {
      path[i++] = c;
    }

    pid_t pid = ::getpid();
    char digits[16]{};
    size_t num_digits = 0;
    do {
      digits[num_digits++] = static_cast<char>('0' + pid % 10);
      pid /= 10;
    } while (pid != 0);
    while (num_digits > 0) {
      path[i++] = digits[--num_digits];
    }

    path[i++] = '.';
    path[i++] = 's';
    path[i++] = 'o';
    path[i++] = 'c';
    path[i++] = 'k';
    path[i] = '\0';
    COROSIG_REQUIRE(i < sizeof(path_probe.sun_path));

    ::unlink(path);

    sockaddr_un un{};
    un.sun_family = AF_UNIX;
    std::memcpy(un.sun_path, path, i);
    std::memcpy(&storage.native_storage, &un, sizeof(un));
  }

  UnixTestPath(UnixTestPath const &) = delete;
  UnixTestPath &operator=(UnixTestPath const &) = delete;

  ~UnixTestPath() noexcept {
    ::unlink(path);
  }
};

struct RawUnixListener {
  int fd = -1;

  explicit RawUnixListener(SockaddrStorage const &addr) noexcept {
    fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    COROSIG_REQUIRE(fd >= 0);
    COROSIG_REQUIRE(::bind(fd,
                           reinterpret_cast<sockaddr const *>(&addr.native_storage),
                           sizeof(sockaddr_un)) == 0);
    COROSIG_REQUIRE(::listen(fd, 8) == 0);
  }

  RawUnixListener(RawUnixListener const &) = delete;
  RawUnixListener &operator=(RawUnixListener const &) = delete;

  ~RawUnixListener() noexcept {
    if (fd != -1) {
      ::close(fd);
    }
  }

  int accept() noexcept {
    return ::accept(fd, nullptr, nullptr);
  }
};

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket default constructed socket has invalid handle") {
  UnixStreamSocket sock;
  COROSIG_REQUIRE(sock.underlying_handle() == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket make_from_os_specific_handle owns given handle") {
  int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  COROSIG_REQUIRE(fd >= 0);

  UnixStreamSocket sock = UnixStreamSocket::make_from_os_specific_handle(fd);
  COROSIG_REQUIRE(sock.underlying_handle() == fd);
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket connect to listener succeeds") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;
    RawUnixListener listener{path.storage};

    COROSIG_CO_TRY(auto sock, co_await UnixStreamSocket::connect(r, path.storage));
    COROSIG_REQUIRE(sock.underlying_handle() >= 0);

    int incoming = listener.accept();
    COROSIG_REQUIRE(incoming >= 0);
    ::close(incoming);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket connect to non-existent path fails") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;

    auto connect_result = co_await UnixStreamSocket::connect(r, path.storage);
    COROSIG_REQUIRE(!connect_result.is_ok());
    COROSIG_REQUIRE(connect_result.error().holds<SyscallError>());

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket write/read roundtrip") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;
    RawUnixListener listener{path.storage};

    COROSIG_CO_TRY(auto sock, co_await UnixStreamSocket::connect(r, path.storage));

    int peer = listener.accept();
    COROSIG_REQUIRE(peer >= 0);

    constexpr static std::string_view MSG = "hello unix";
    COROSIG_CO_TRY(auto written, co_await sock.write_some(r, MSG));
    COROSIG_REQUIRE(written == MSG.size());

    std::array<char, MSG.size()> peer_buf{};
    COROSIG_REQUIRE(::read(peer, peer_buf.data(), peer_buf.size()) ==
                    static_cast<ssize_t>(MSG.size()));
    COROSIG_REQUIRE(std::string_view{peer_buf.data(), peer_buf.size()} == MSG);

    COROSIG_REQUIRE(::write(peer, MSG.data(), MSG.size()) == static_cast<ssize_t>(MSG.size()));

    std::array<char, MSG.size()> buf{};
    COROSIG_CO_TRY(auto read, co_await sock.read(r, buf));
    COROSIG_REQUIRE(read == MSG.size());
    COROSIG_REQUIRE(std::string_view{buf.data(), read} == MSG);

    ::close(peer);
    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket read returns 0 on EOF") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;
    RawUnixListener listener{path.storage};

    COROSIG_CO_TRY(auto sock, co_await UnixStreamSocket::connect(r, path.storage));

    int peer = listener.accept();
    COROSIG_REQUIRE(peer >= 0);
    ::close(peer);

    std::array<char, 16> buf{};
    COROSIG_CO_TRY(auto read, co_await sock.read_some(r, buf));
    COROSIG_REQUIRE(read == 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket try_write_some and try_read_some") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;
    RawUnixListener listener{path.storage};

    COROSIG_CO_TRY(auto sock, co_await UnixStreamSocket::connect(r, path.storage));

    int peer = listener.accept();
    COROSIG_REQUIRE(peer >= 0);

    auto write_result = sock.try_write_some("try unix");
    COROSIG_REQUIRE(write_result.is_ok());
    COROSIG_REQUIRE(write_result.value() == 8);

    std::array<char, 8> peer_buf{};
    COROSIG_REQUIRE(::read(peer, peer_buf.data(), peer_buf.size()) ==
                    static_cast<ssize_t>(peer_buf.size()));
    COROSIG_REQUIRE(std::string_view{peer_buf.data(), peer_buf.size()} == "try unix");

    COROSIG_REQUIRE(::write(peer, peer_buf.data(), peer_buf.size()) ==
                    static_cast<ssize_t>(peer_buf.size()));

    std::array<char, 8> buf{};
    auto read_result = sock.try_read_some(buf);
    COROSIG_REQUIRE(read_result.is_ok());
    COROSIG_REQUIRE(read_result.value() == buf.size());
    COROSIG_REQUIRE(std::string_view{buf.data(), buf.size()} == "try unix");

    ::close(peer);
    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket move semantics") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;
    RawUnixListener listener{path.storage};

    COROSIG_CO_TRY(auto sock1, co_await UnixStreamSocket::connect(r, path.storage));
    int fd_before = sock1.underlying_handle();

    UnixStreamSocket sock2(std::move(sock1));

    COROSIG_REQUIRE(sock1.underlying_handle() == -1); // NOLINT(bugprone-use-after-move)
    COROSIG_REQUIRE(sock2.underlying_handle() == fd_before);

    UnixStreamSocket sock3;
    sock3 = std::move(sock2);

    COROSIG_REQUIRE(sock2.underlying_handle() == -1); // NOLINT(bugprone-use-after-move)
    COROSIG_REQUIRE(sock3.underlying_handle() == fd_before);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket downcast from StreamSocket preserves socket") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;
    RawUnixListener listener{path.storage};

    COROSIG_CO_TRY(auto connected, co_await UnixStreamSocket::connect(r, path.storage));
    int fd_before = connected.underlying_handle();

    StreamSocket base = std::move(connected);
    COROSIG_REQUIRE(base.underlying_handle() == fd_before);

    UnixStreamSocket sock{std::move(base)};
    COROSIG_REQUIRE(base.underlying_handle() == -1); // NOLINT(bugprone-use-after-move)
    COROSIG_REQUIRE(sock.underlying_handle() == fd_before);

    int peer = listener.accept();
    COROSIG_REQUIRE(peer >= 0);

    constexpr static std::string_view MSG = "downcast";
    COROSIG_CO_TRYV(co_await sock.write_some(r, MSG));

    std::array<char, MSG.size()> buf{};
    COROSIG_REQUIRE(::read(peer, buf.data(), buf.size()) == static_cast<ssize_t>(MSG.size()));
    COROSIG_REQUIRE(std::string_view{buf.data(), buf.size()} == MSG);

    ::close(peer);
    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket close invalidates handle") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;
    RawUnixListener listener{path.storage};

    COROSIG_CO_TRY(auto sock, co_await UnixStreamSocket::connect(r, path.storage));

    sock.close();
    COROSIG_REQUIRE(sock.underlying_handle() == -1);

    auto write_result = sock.try_write_some(std::string_view{"test"});
    COROSIG_REQUIRE(!write_result.is_ok());

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UnixStreamSocket address returns AF_UNIX family") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    UnixTestPath path;
    RawUnixListener listener{path.storage};

    COROSIG_CO_TRY(auto sock, co_await UnixStreamSocket::connect(r, path.storage));

    COROSIG_CO_TRY(auto addr, sock.address());
    COROSIG_REQUIRE(addr.native_storage.ss_family == AF_UNIX);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}
