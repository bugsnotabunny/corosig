#include "corosig/io/UdpSocket.hpp"

#include "corosig/Result.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch_all.hpp>
#include <cstddef>
#include <netinet/in.h>

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket writer() creates a valid socket", "[udp]") {
  auto r = UdpSocket::writer();
  COROSIG_REQUIRE(r.is_ok());
  UdpSocket sock = std::move(r.value());
  COROSIG_REQUIRE(sock.underlying_handle() != -1);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket readwriter() binds to local port", "[udp]") {
  sockaddr_storage addr{};
  auto *in = reinterpret_cast<sockaddr_in *>(&addr);
  in->sin_family = AF_INET;
  in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  in->sin_port = 0;

  auto r = UdpSocket::readwriter(addr);
  REQUIRE(r.is_ok());
  UdpSocket sock = std::move(r.value());
  REQUIRE(sock.underlying_handle() != -1);
}

COROSIG_SIGHANDLER_TEST_CASE("UdpSocket send_to/recv_from async loopback", "[udp]") {
  auto foo = [](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
    sockaddr_storage local{};
    auto *in = reinterpret_cast<sockaddr_in *>(&local);
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    in->sin_port = htons(9912);

    COROSIG_CO_TRY(auto receiver, UdpSocket::readwriter(local));
    COROSIG_CO_TRY(auto sender, UdpSocket::writer());

    constexpr std::string_view MSG = "hello udp";
    std::array<char, MSG.size()> buffer;

    COROSIG_CO_TRY(size_t sent, co_await sender.send_to(r, MSG, local));
    COROSIG_REQUIRE(sent == MSG.size());

    COROSIG_CO_TRY(size_t recvd, co_await receiver.recv_from(r, std::span<char>(buffer), nullptr));
    COROSIG_REQUIRE(recvd == MSG.size());
    COROSIG_REQUIRE(MSG == std::string_view{buffer.data(), buffer.size()});

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}
