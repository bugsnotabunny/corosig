#include "corosig/io/TcpSocket.hpp"

#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <arpa/inet.h>
#include <boost/outcome/try.hpp>
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

// TEST_CASE("TcpSocket connect to server succeeds") {

//   constexpr static uint16_t PORT = 5555;
//   start_echo_server(PORT);

//   run_in_sighandler([](Reactor &reactor) {
//     auto foo = [&](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
//       sockaddr_in addr;
//       addr.sin_port = htons(PORT);
//       addr.sin_family = AF_INET;
//       addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);

//       sockaddr_storage ss{};
//       std::memcpy(&ss, &addr, sizeof(addr));

//       BOOST_OUTCOME_CO_TRY(auto socket, co_await TcpSocket::connect(r, ss));
//       COROSIG_REQUIRE(socket.underlying_handle() >= 0);

//       co_return success();
//     };
//     COROSIG_REQUIRE(foo(reactor).block_on().has_value());
//   });
// }

TEST_CASE("TcpSocket write/read roundtrip") {
  constexpr static uint16_t PORT = 5556;
  start_echo_server(PORT);

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [&](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(PORT);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

      sockaddr_storage ss{};

      std::memcpy(&ss, &addr, sizeof(addr));

      BOOST_OUTCOME_CO_TRY(auto sock, co_await TcpSocket::connect(r, ss));

      constexpr static std::string_view MSG = "hello";
      BOOST_OUTCOME_CO_TRY(auto written, co_await sock.write_some(r, MSG));
      COROSIG_REQUIRE(written == MSG.size());

      // std::array<char, MSG.size()> buf;
      // BOOST_OUTCOME_CO_TRY(auto read, co_await sock.read(r, buf));
      // COROSIG_REQUIRE(std::string_view{buf.begin(), read} == MSG);

      co_return success();
    };
    COROSIG_REQUIRE(foo(reactor).block_on().has_value());
  });
}
