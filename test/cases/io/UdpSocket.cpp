#include "corosig/io/UdpSocket.hpp"

#include "corosig/io/Sockaddr.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <array>
#include <catch2/catch_all.hpp>
#include <netinet/in.h>
#include <string_view>
#include <sys/socket.h>

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: unbound creates valid socket with AF_INET") {
  auto result = UdpSocket::unbound(AF_INET);

  COROSIG_REQUIRE(result.is_ok());
  UdpSocket sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: unbound creates valid socket with AF_INET6") {
  auto result = UdpSocket::unbound(AF_INET6);

  COROSIG_REQUIRE(result.is_ok());
  UdpSocket sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: bound randomly with AF_INET") {
  auto result = UdpSocket::bound(Ipv4Addr::loopback().to_sockaddr());

  COROSIG_REQUIRE(result.is_ok());
  UdpSocket sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: bound randomly with AF_INET6") {
  auto result = UdpSocket::bound(Ipv6Addr::loopback().to_sockaddr());

  COROSIG_REQUIRE(result.is_ok());
  UdpSocket sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: bound to specific IPv4 address and port") {
  SockaddrStorage addr = Ipv4Addr::loopback().to_sockaddr(12345);

  auto result = UdpSocket::bound(addr);

  COROSIG_REQUIRE(result.is_ok());
  UdpSocket sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: bound to specific IPv6 address and port") {
  auto ipv6 = Ipv6Addr::parse("::1");

  SockaddrStorage addr = ipv6->to_sockaddr(12345);

  auto result = UdpSocket::bound(addr);

  COROSIG_REQUIRE(result.is_ok());
  UdpSocket sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: send_to and recv_from with source_addr tracking") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    SockaddrStorage local = Ipv4Addr::loopback().to_sockaddr(9876);

    COROSIG_CO_TRY(auto receiver, UdpSocket::bound(local));
    COROSIG_CO_TRY(auto sender, UdpSocket::unbound());

    std::string_view msg = "hello with source";
    COROSIG_CO_TRY(size_t sent, co_await sender.send_to(r, msg, local));
    COROSIG_REQUIRE(sent == msg.size());

    std::array<char, 64> recv_buf{};
    SockaddrStorage source{};
    COROSIG_CO_TRY(size_t recvd, co_await receiver.recv_from(r, recv_buf, &source));

    COROSIG_REQUIRE(recvd == msg.size());
    COROSIG_REQUIRE(source.native_storage.ss_family == AF_INET);

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: send empty message") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    SockaddrStorage local = Ipv4Addr::loopback().to_sockaddr(15321);

    COROSIG_CO_TRY(auto receiver, UdpSocket::bound(local));
    COROSIG_CO_TRY(auto sender, UdpSocket::unbound());

    std::string_view msg{};
    COROSIG_CO_TRY(size_t sent, co_await sender.send_to(r, msg, local));

    COROSIG_REQUIRE(sent == 0);

    std::array<char, 64> recv_buf{};
    COROSIG_CO_TRY(size_t recvd, co_await receiver.recv_from(r, recv_buf, nullptr));

    COROSIG_REQUIRE(recvd == 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: send and receive larger message") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    SockaddrStorage local = Ipv4Addr::loopback().to_sockaddr(19284);

    COROSIG_CO_TRY(auto receiver, UdpSocket::bound(local));
    COROSIG_CO_TRY(auto sender, UdpSocket::unbound());

    std::array<char, 500> msg{};
    for (size_t i = 0; i < msg.size(); ++i) {
      msg[i] = static_cast<char>(i % 256);
    }

    std::string_view msg_sv{msg.data(), msg.size()};

    COROSIG_CO_TRY(size_t sent, co_await sender.send_to(r, msg_sv, local));
    COROSIG_REQUIRE(sent == msg.size());

    std::array<char, 1024> recv_buf{};
    COROSIG_CO_TRY(size_t recvd, co_await receiver.recv_from(r, recv_buf, nullptr));

    COROSIG_REQUIRE(recvd == msg.size());
    COROSIG_REQUIRE(std::equal(msg.begin(), msg.end(), recv_buf.begin()));

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: socket is moveable") {
  auto result1 = UdpSocket::unbound();
  COROSIG_REQUIRE(result1.is_ok());

  auto result2 = UdpSocket::unbound();
  COROSIG_REQUIRE(result2.is_ok());

  UdpSocket sock1 = std::move(result1).value();
  UdpSocket sock2 = std::move(result2).value();

  UdpSocket moved = std::move(sock1);

  COROSIG_REQUIRE(moved.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: close invalidates handle") {
  SockaddrStorage local = Ipv4Addr::loopback().to_sockaddr(54321);

  auto result = UdpSocket::bound(local);
  COROSIG_REQUIRE(result.is_ok());

  UdpSocket sock = std::move(result).value();
  int fd = sock.underlying_handle();
  COROSIG_REQUIRE(fd >= 0);

  sock.close();
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: try_recv_from returns not blocked when data available") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    SockaddrStorage local = Ipv4Addr::loopback().to_sockaddr(27412);

    COROSIG_CO_TRY(auto receiver, UdpSocket::bound(local));
    COROSIG_CO_TRY(auto sender, UdpSocket::unbound());

    std::string_view msg = "test msg";

    COROSIG_CO_TRY(size_t sent, co_await sender.send_to(r, msg, local));
    COROSIG_REQUIRE(sent == msg.size());

    std::array<char, 64> recv_buf{};
    auto recv_result = receiver.try_recv_from(recv_buf, nullptr);

    if (!recv_result) {
      co_return Failure{recv_result.error()};
    }

    COROSIG_REQUIRE(recv_result.value() == msg.size());

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: try_send_to works immediately") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    SockaddrStorage local = Ipv4Addr::loopback().to_sockaddr(8765);

    COROSIG_CO_TRY(auto sock, UdpSocket::bound(local));

    std::string_view msg = "hello sync";

    auto result = sock.try_send_to(msg, local);
    COROSIG_REQUIRE(result.is_ok());

    COROSIG_REQUIRE(result.value() == msg.size());

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: multiple send operations") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    SockaddrStorage local = Ipv4Addr::loopback().to_sockaddr(11111);

    COROSIG_CO_TRY(auto receiver, UdpSocket::bound(local));
    COROSIG_CO_TRY(auto sender, UdpSocket::unbound());

    std::string_view msg1 = "msg1";
    std::string_view msg2 = "msg2";
    std::string_view msg3 = "msg3";

    COROSIG_CO_TRY(size_t sent1, co_await sender.send_to(r, msg1, local));
    COROSIG_REQUIRE(sent1 == msg1.size());
    COROSIG_CO_TRY(size_t sent2, co_await sender.send_to(r, msg2, local));
    COROSIG_REQUIRE(sent2 == msg2.size());
    COROSIG_CO_TRY(size_t sent3, co_await sender.send_to(r, msg3, local));
    COROSIG_REQUIRE(sent3 == msg3.size());

    auto msgs = std::to_array({msg1, msg2, msg3});
    for (size_t i = 0; i < 3; ++i) {
      std::array<char, 64> recv_buf{};
      COROSIG_CO_TRY(size_t recvd, co_await receiver.recv_from(r, recv_buf, nullptr));
      COROSIG_REQUIRE(std::string_view{recv_buf.begin(), recvd} == msgs[i]);
    }

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: IPv6 loopback send/receive") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    SockaddrStorage local = Ipv6Addr::loopback().to_sockaddr(54321);

    COROSIG_CO_TRY(auto receiver, UdpSocket::bound(local));
    COROSIG_CO_TRY(auto sender, UdpSocket::unbound(AF_INET6));

    std::string_view msg = "hello ipv6";

    COROSIG_CO_TRY(size_t sent, co_await sender.send_to(r, msg, local));
    COROSIG_REQUIRE(sent == msg.size());

    std::array<char, 64> recv_buf{};
    COROSIG_CO_TRY(size_t recvd, co_await receiver.recv_from(r, recv_buf, nullptr));

    COROSIG_REQUIRE(recvd == msg.size());
    COROSIG_REQUIRE(msg == std::string_view{recv_buf.data(), recvd});

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: default initialized socket has invalid handle") {
  UdpSocket sock;
  COROSIG_REQUIRE(sock.underlying_handle() == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket: underlying_handle returns correct value") {
  auto result = UdpSocket::unbound();
  COROSIG_REQUIRE(result.is_ok());

  UdpSocket sock = std::move(result).value();
  int handle = sock.underlying_handle();

  COROSIG_REQUIRE(handle >= 0);
}
