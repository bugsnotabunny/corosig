#include "corosig/io/TcpSocket.hpp"

#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <arpa/inet.h>
#include <catch2/catch_all.hpp>
#include <cstring>
#include <netinet/in.h>
#include <span>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

using namespace corosig;

int start_echo_server(uint16_t port) {
  int srv_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(srv_fd >= 0);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = ::htons(port);
  addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);

  int opt = 1;
  setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  REQUIRE(::bind(srv_fd, (sockaddr *)&addr, sizeof(addr)) == 0);
  REQUIRE(::listen(srv_fd, 1) == 0);

  std::thread([srv_fd]() {
    int client = ::accept(srv_fd, nullptr, nullptr);
    std::array<char, 1024> buf;
    while (true) {
      ssize_t n = ::read(client, buf.begin(), sizeof(buf));
      if (n <= 0) {
        break;
      }
      COROSIG_REQUIRE(::write(client, buf.begin(), n) == n);
    }
    ::close(client);
    ::close(srv_fd);
  }).detach();

  return srv_fd;
}

} // namespace

TEST_CASE("TcpSocket connect to server succeeds") {

  constexpr static uint16_t PORT = 5555;
  start_echo_server(PORT);

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [&](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
      sockaddr_in addr;
      addr.sin_port = htons(PORT);
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);

      SockaddrStorage ss{};
      std::memcpy(&ss, &addr, sizeof(addr));

      COROSIG_CO_TRY(auto socket, co_await TcpSocket::connect(r, ss));
      COROSIG_REQUIRE(socket.underlying_handle() >= 0);

      co_return Ok();
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("TcpSocket write/read roundtrip") {
  constexpr static uint16_t PORT = 5556;
  start_echo_server(PORT);

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [&](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(PORT);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

      SockaddrStorage ss{};

      std::memcpy(&ss, &addr, sizeof(addr));

      COROSIG_CO_TRY(auto sock, co_await TcpSocket::connect(r, ss));

      constexpr static std::string_view MSG = "hello";
      COROSIG_CO_TRY(auto written, co_await sock.write_some(r, MSG));
      COROSIG_REQUIRE(written == MSG.size());

      std::array<char, MSG.size()> buf;
      COROSIG_CO_TRY(auto read, co_await sock.read(r, buf));
      COROSIG_REQUIRE(std::string_view{buf.begin(), read} == MSG);

      co_return Ok();
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("TcpSocket connect to non-existent server fails") {
  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(1234);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

      SockaddrStorage ss{};
      std::memcpy(&ss, &addr, sizeof(addr));

      auto connect_result = co_await TcpSocket::connect(r, ss);
      COROSIG_REQUIRE(!connect_result.is_ok());
      COROSIG_REQUIRE(connect_result.error().holds<SyscallError>());

      co_return Ok();
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("TcpSocket move semantics") {
  constexpr static uint16_t PORT = 5563;
  start_echo_server(PORT);

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(PORT);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

      SockaddrStorage ss{};
      std::memcpy(&ss, &addr, sizeof(addr));

      COROSIG_CO_TRY(auto sock1, co_await TcpSocket::connect(r, ss));
      int fd_before = sock1.underlying_handle();

      TcpSocket sock2(std::move(sock1));

      COROSIG_REQUIRE(sock1.underlying_handle() == -1);
      COROSIG_REQUIRE(sock2.underlying_handle() >= 0);
      COROSIG_REQUIRE(sock2.underlying_handle() == fd_before);

      constexpr static std::string_view MSG = "move_test";
      COROSIG_CO_TRYV(co_await sock2.write_some(r, MSG));
      std::array<char, MSG.size()> buf{};
      COROSIG_CO_TRYV(co_await sock2.read(r, buf));

      co_return Ok();
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("TcpSocket move assignment") {
  constexpr static uint16_t PORT = 5564;
  start_echo_server(PORT);

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(PORT);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

      SockaddrStorage ss{};
      std::memcpy(&ss, &addr, sizeof(addr));

      COROSIG_CO_TRY(auto sock1, co_await TcpSocket::connect(r, ss));

      TcpSocket sock2;
      sock2 = std::move(sock1);

      COROSIG_REQUIRE(sock1.underlying_handle() == -1);
      COROSIG_REQUIRE(sock2.underlying_handle() >= 0);

      constexpr static std::string_view MSG = "move_assign_test";
      COROSIG_CO_TRYV(co_await sock2.write_some(r, MSG));
      std::array<char, MSG.size()> buf{};
      COROSIG_CO_TRY(auto read, co_await sock2.read(r, buf));
      COROSIG_REQUIRE(std::string_view{buf.begin(), read} == MSG);

      co_return Ok();
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("TcpSocket close() invalidates descriptor") {
  constexpr static uint16_t PORT = 5565;
  start_echo_server(PORT);

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(PORT);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

      SockaddrStorage ss{};
      std::memcpy(&ss, &addr, sizeof(addr));

      COROSIG_CO_TRY(auto sock, co_await TcpSocket::connect(r, ss));

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
  });
}

TEST_CASE("TcpSocket default constructed socket has invalid handle") {
  TcpSocket sock;
  COROSIG_REQUIRE(sock.underlying_handle() == -1);
}
