#include "corosig/io/TcpSocket.hpp"

#include "corosig/Background.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/TcpListener.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <array>
#include <netinet/in.h>
#include <span>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using namespace corosig;

Fut<void, Error<AllocationError, SyscallError>> run_echo_server(Reactor &r,
                                                                TcpListener listener) noexcept {
  COROSIG_CO_TRY(AcceptResult ar, co_await listener.accept(r));
  TcpSocket client = std::move(ar.incoming_connection);
  std::array<char, 1024> buf;
  while (true) {
    COROSIG_CO_TRY(size_t n, co_await client.read_some(r, buf));
    if (n <= 0) {
      break;
    }
    COROSIG_CO_TRYV(co_await client.write(r, {buf.data(), n}));
  }
  co_return Ok{};
}

struct StartServerResult {
  SockaddrStorage addr;
  Fut<void, Error<AllocationError, SyscallError>> fut;
};

StartServerResult start_echo_server(Reactor &r) noexcept {
  auto listener = TcpListener::make({.addr = Ipv4Addr::loopback().to_sockaddr()});

  COROSIG_REQUIRE(listener);

  auto addr = listener.value().address();
  COROSIG_REQUIRE(addr);

  return StartServerResult{
      .addr = addr.value(),
      .fut = run_echo_server(r, std::move(listener).value()),
  };
}

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("TcpSocket connect to server succeeds") {
  auto foo = [&](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
    auto [addr, _] = start_echo_server(r);
    COROSIG_CO_TRY(auto socket, co_await TcpSocket::connect(r, addr));
    COROSIG_REQUIRE(socket.underlying_handle() >= 0);

    co_return Ok();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpSocket write/read roundtrip") {
  auto foo = [&](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
    auto [addr, server_finished] = start_echo_server(r);

    COROSIG_CO_TRY(auto sock, co_await TcpSocket::connect(r, addr));

    constexpr static std::string_view MSG = "hello";
    COROSIG_CO_TRY(auto written, co_await sock.write_some(r, MSG));
    COROSIG_REQUIRE(written == MSG.size());

    std::array<char, MSG.size()> buf;
    COROSIG_CO_TRY(auto read, co_await sock.read(r, buf));
    COROSIG_REQUIRE(std::string_view{buf.begin(), read} == MSG);

    sock.close();
    COROSIG_REQUIRE(co_await std::move(server_finished));

    co_return Ok();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpSocket connect to non-existent server fails") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    SockaddrStorage ss = Ipv4Addr::loopback().to_sockaddr(1234);

    auto connect_result = co_await TcpSocket::connect(r, ss);
    COROSIG_REQUIRE(!connect_result.is_ok());
    COROSIG_REQUIRE(connect_result.error().holds<SyscallError>());

    co_return Ok();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpSocket move semantics") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    auto [addr, server_finished] = start_echo_server(r);

    COROSIG_CO_TRY(auto sock1, co_await TcpSocket::connect(r, addr));
    int fd_before = sock1.underlying_handle();

    TcpSocket sock2(std::move(sock1));

    COROSIG_REQUIRE(sock1.underlying_handle() == -1); // NOLINT (bugprone-use-after-move)
    COROSIG_REQUIRE(sock2.underlying_handle() >= 0);
    COROSIG_REQUIRE(sock2.underlying_handle() == fd_before);

    constexpr static std::string_view MSG = "move_test";
    COROSIG_CO_TRYV(co_await sock2.write_some(r, MSG));
    std::array<char, MSG.size()> buf{};
    COROSIG_CO_TRYV(co_await sock2.read(r, buf));

    sock2.close();
    COROSIG_REQUIRE(co_await std::move(server_finished));

    co_return Ok();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpSocket move assignment") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    auto [addr, server_finished] = start_echo_server(r);

    COROSIG_CO_TRY(auto sock1, co_await TcpSocket::connect(r, addr));

    TcpSocket sock2;
    sock2 = std::move(sock1);

    COROSIG_REQUIRE(sock1.underlying_handle() == -1); // NOLINT (bugprone-use-after-move)
    COROSIG_REQUIRE(sock2.underlying_handle() >= 0);

    constexpr static std::string_view MSG = "move_assign_test";
    COROSIG_CO_TRYV(co_await sock2.write_some(r, MSG));
    std::array<char, MSG.size()> buf{};
    COROSIG_CO_TRY(auto read, co_await sock2.read(r, buf));
    COROSIG_REQUIRE(std::string_view{buf.begin(), read} == MSG);

    sock2.close();
    COROSIG_REQUIRE(co_await std::move(server_finished));

    co_return Ok();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpSocket close() invalidates descriptor") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    auto [addr, _] = start_echo_server(r);

    COROSIG_CO_TRY(auto sock, co_await TcpSocket::connect(r, addr));

    sock.close();

    COROSIG_REQUIRE(sock.underlying_handle() == -1);

    std::array<char, 64> buf{};
    auto write_result = sock.try_write_some(std::string_view{"test"});
    COROSIG_REQUIRE(!write_result.is_ok());

    auto read_result = sock.try_read_some(buf);
    COROSIG_REQUIRE(!read_result.is_ok());

    co_return Ok();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpSocket default constructed socket has invalid handle") {
  TcpSocket sock;
  COROSIG_REQUIRE(sock.underlying_handle() == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpSocket connect_from") {
  auto foo = [](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
    auto listener = TcpListener::make({.addr = Ipv4Addr::loopback().to_sockaddr(0)}).value();

    SockaddrStorage target = listener.address().value();
    SockaddrStorage local = Ipv4Addr::loopback().to_sockaddr(8912);

    COROSIG_REQUIRE(run_in_background(r, TcpSocket::connect_from(r, local, target)));
    COROSIG_CO_TRY(AcceptResult ar, co_await listener.accept(r));

    COROSIG_REQUIRE(ar.incoming_connection_addr.native_storage.ss_family == AF_INET);
    auto const *in_a =
        reinterpret_cast<sockaddr_in const *>(&ar.incoming_connection_addr.native_storage);
    auto const *in_b = reinterpret_cast<sockaddr_in const *>(&local.native_storage);
    COROSIG_REQUIRE(in_a->sin_port == in_b->sin_port);
    COROSIG_REQUIRE(in_a->sin_addr.s_addr == in_b->sin_addr.s_addr);

    co_return Ok();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}
