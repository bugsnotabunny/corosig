#include "corosig/Background.hpp"
#include "corosig/Yield.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/TcpListener.hpp"
#include "corosig/io/TcpSocket.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using namespace corosig;

constexpr std::string_view MESSAGE =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
    "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in "
    "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
    "cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

void benchmark_body(std::span<char> mem, size_t num_connections) {
  constexpr static auto CLIENT_TASK = [](Reactor &r,
                                         SockaddrStorage const &addr) -> BackgroundTask {
    auto client_opt = co_await TcpSocket::connect(r, addr);
    if (!client_opt) {
      FAIL(client_opt.error().description());
      co_return;
    }
    auto client = std::move(client_opt.value());

    auto write_opt = co_await client.write(r, MESSAGE);
    if (!write_opt) {
      FAIL(write_opt.error().description());
    }

    std::array<char, MESSAGE.size()> response_buf;
    auto read_opt = co_await client.read(r, response_buf);
    if (!read_opt) {
      FAIL(read_opt.error().description());
    }
  };

  constexpr static auto SERVER_TASK = [](Reactor &r, TcpSocket server) -> BackgroundTask {
    std::array<char, MESSAGE.size()> request_buf;
    auto read_opt = co_await server.read(r, request_buf);
    if (!read_opt) {
      FAIL(read_opt.error().description());
    }

    auto write_opt = co_await server.write(r, MESSAGE);
    if (!write_opt) {
      FAIL(write_opt.error().description());
    }
  };

  constexpr static auto SPAWN_CONNECTIONS_TASK =
      [](Reactor &r, SockaddrStorage const &addr, size_t num_connections) -> BackgroundTask {
    for (size_t i = 0; i < num_connections; ++i) {
      REQUIRE(CLIENT_TASK(r, addr));
      co_await Yield{};
    }
  };

  SockaddrStorage addr;

  auto acceptor_task = [&](Reactor &r, size_t num_connections) -> BackgroundTask {
    auto listener_opt = TcpListener::make({
        .addr = Ipv4Addr::loopback().to_sockaddr(0),
        .reuse_addr = true,
        .reuse_port = true,
    });
    if (!listener_opt) {
      FAIL(listener_opt.error().description());
    }
    auto listener = std::move(listener_opt.value());

    addr = listener.address().value();

    for (size_t i = 0; i < num_connections; ++i) {
      auto server_opt = co_await listener.accept(r);
      if (!server_opt) {
        FAIL(server_opt.error().description());
      }
      REQUIRE(SERVER_TASK(r, std::move(server_opt.value().incoming_connection)));
    }
  };

  Reactor reactor{mem};
  REQUIRE(acceptor_task(reactor, num_connections));
  REQUIRE(SPAWN_CONNECTIONS_TASK(reactor, addr, num_connections));
  REQUIRE(reactor.drain_remaining_tasks());
}

// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
auto mem = std::make_unique<Allocator::Memory<std::numeric_limits<uint32_t>::max()>>();

} // namespace

TEST_CASE("TCP server DoS") {

  size_t const num_connections =
      GENERATE(32, 64, 128, 256, 512, 1024, 1536, 2048, 2560, 3072, 3584, 4096, 8192);

  BENCHMARK(std::format("Server self-DoS with {} connections", num_connections)) {
    benchmark_body(*mem, num_connections);
  };
}
