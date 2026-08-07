#include "corosig/io/TcpListener.hpp"

#include "corosig/Background.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <array>
#include <string_view>

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("TcpListener default constructed socket has invalid handle") {
  TcpListener sock;
  COROSIG_REQUIRE(sock.underlying_handle() == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener make creates valid socket with default options") {
  auto result = TcpListener::make({
      .addr = Ipv4Addr::loopback().to_sockaddr(0),
  });

  COROSIG_REQUIRE(result.is_ok());
  TcpListener sock = std::move(result).value();

  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener make with custom backlog size") {
  auto result = TcpListener::make({
      .addr = Ipv4Addr::loopback().to_sockaddr(0),
      .backlog_size = 128,
  });

  COROSIG_REQUIRE(result.is_ok());
  TcpListener sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener make with reuse_addr false") {
  auto result = TcpListener::make({
      .addr = Ipv4Addr::loopback().to_sockaddr(12345),
      .reuse_addr = false,
  });

  COROSIG_REQUIRE(result.is_ok());
  TcpListener sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener make with reuse_port false") {
  auto result = TcpListener::make({
      .addr = Ipv4Addr::loopback().to_sockaddr(0),
      .reuse_port = false,
  });

  COROSIG_REQUIRE(result.is_ok());
  TcpListener sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener make with specific port") {
  auto result = TcpListener::make({
      .addr = Ipv4Addr::loopback().to_sockaddr(12345),
  });

  COROSIG_REQUIRE(result.is_ok());
  TcpListener sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener accept incoming connection") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    TcpListener::Options options{
        .addr = Ipv4Addr::loopback().to_sockaddr(0),
    };

    COROSIG_CO_TRY(auto listener, TcpListener::make(options));

    auto client_task = [](Reactor &r,
                          SockaddrStorage addr) -> Fut<void, Error<AllocationError, SyscallError>> {
      auto client_opt = co_await TcpSocket::connect(r, addr);
      COROSIG_REQUIRE(client_opt);
      co_return Ok{};
    }(r, listener.address().value());

    COROSIG_CO_TRY(auto accept_result, co_await listener.accept(r));

    COROSIG_REQUIRE(accept_result.incoming_connection.underlying_handle() >= 0);
    COROSIG_REQUIRE(accept_result.incoming_connection_addr.native_storage.ss_family == AF_INET);

    COROSIG_CO_TRYV(co_await std::move(client_task));
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener accept and exchange data") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    TcpListener::Options listener_options{
        .addr = Ipv4Addr::loopback().to_sockaddr(0),
    };

    COROSIG_CO_TRY(auto listener, TcpListener::make(listener_options));

    constexpr static std::string_view SERVER_MSG = "hello from server";
    constexpr static std::string_view CLIENT_MSG = "hello from client";

    auto client_task = [](Reactor &r, SockaddrStorage addr) -> Fut<void, AllocationError> {
      auto client_opt = co_await TcpSocket::connect(r, addr);
      COROSIG_REQUIRE(client_opt);
      auto client = std::move(client_opt.value());

      auto written = co_await client.write(r, CLIENT_MSG);

      COROSIG_REQUIRE(written);
      COROSIG_REQUIRE(written.value() == CLIENT_MSG.size());

      std::array<char, 64> client_buf;
      auto client_read = co_await client.read_some(r, client_buf);
      COROSIG_REQUIRE(client_read);
      COROSIG_REQUIRE(std::string_view{client_buf.begin(), client_read.value()} == SERVER_MSG);
      co_return Ok{};
    }(r, listener.address().value());

    COROSIG_CO_TRY(auto ar, co_await listener.accept(r));

    std::array<char, 64> server_buf;
    COROSIG_CO_TRY(auto read, co_await ar.incoming_connection.read_some(r, server_buf));
    COROSIG_REQUIRE(std::string_view{server_buf.begin(), read} == CLIENT_MSG);

    COROSIG_CO_TRYV(co_await ar.incoming_connection.write(r, SERVER_MSG));
    COROSIG_REQUIRE(co_await std::move(client_task));
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener accept multiple connections") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    TcpListener::Options options{
        .addr = Ipv4Addr::loopback().to_sockaddr(0),
    };

    COROSIG_CO_TRY(auto listener, TcpListener::make(options));

    constexpr static size_t NUM_CONNECTIONS = 16;

    auto client_coro = [](Reactor &r,
                          SockaddrStorage addr) -> Fut<void, Error<AllocationError, SyscallError>> {
      auto client_opt = co_await TcpSocket::connect(r, addr);
      COROSIG_REQUIRE(client_opt);
      auto client = std::move(client_opt.value());

      constexpr static std::string_view MSG = "test";
      COROSIG_CO_TRYV(co_await client.write(r, MSG));

      std::array<char, 64> client_buf;
      auto client_read = co_await client.read_some(r, client_buf);
      COROSIG_REQUIRE(client_read);
      COROSIG_REQUIRE(std::string_view{client_buf.begin(), client_read.value()} == "response");

      co_return Ok{};
    };

    SockaddrStorage addr = listener.address().value();

    for (size_t i = 0; i < NUM_CONNECTIONS; ++i) {
      auto client_future = client_coro(r, addr);

      COROSIG_CO_TRY(auto server, co_await listener.accept(r));
      COROSIG_REQUIRE(server.incoming_connection.underlying_handle() >= 0);

      std::array<char, 64> buf{};
      COROSIG_CO_TRY(auto read, co_await server.incoming_connection.read_some(r, buf));
      COROSIG_REQUIRE(std::string_view{buf.begin(), read} == "test");

      COROSIG_CO_TRYV(co_await server.incoming_connection.write(r, "response"));

      COROSIG_CO_TRYV(co_await std::move(client_future));
    }

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener move semantics") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    TcpListener::Options options{
        .addr = Ipv4Addr::loopback().to_sockaddr(0),
    };

    COROSIG_CO_TRY(auto listener1, TcpListener::make(options));
    int fd_before = listener1.underlying_handle();

    TcpListener listener2(std::move(listener1));

    COROSIG_REQUIRE(listener1.underlying_handle() == -1);
    COROSIG_REQUIRE(listener2.underlying_handle() >= 0);
    COROSIG_REQUIRE(listener2.underlying_handle() == fd_before);

    auto client_task = [](Reactor &r,
                          SockaddrStorage addr) -> Fut<void, Error<AllocationError, SyscallError>> {
      auto client_opt = co_await TcpSocket::connect(r, addr);
      COROSIG_REQUIRE(client_opt);
      co_return Ok{};
    }(r, listener2.address().value());

    COROSIG_CO_TRY(auto accept_result, co_await listener2.accept(r));

    COROSIG_REQUIRE(accept_result.incoming_connection.underlying_handle() >= 0);

    COROSIG_CO_TRYV(co_await std::move(client_task));
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener move assignment") {
  auto test_coro = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    TcpListener::Options options1{
        .addr = Ipv4Addr::loopback().to_sockaddr(0),
    };

    COROSIG_CO_TRY(auto listener1, TcpListener::make(options1));

    TcpListener listener2;
    listener2 = std::move(listener1);

    COROSIG_REQUIRE(listener1.underlying_handle() == -1);
    COROSIG_REQUIRE(listener2.underlying_handle() >= 0);

    BackgroundTask client_task = [](Reactor &r, SockaddrStorage addr) -> BackgroundTask {
      auto client_opt = co_await TcpSocket::connect(r, addr);
      COROSIG_REQUIRE(client_opt);
    }(r, listener2.address().value());

    COROSIG_CO_TRY(auto accept_result, co_await listener2.accept(r));

    COROSIG_REQUIRE(accept_result.incoming_connection.underlying_handle() >= 0);

    COROSIG_CO_TRYV(std::move(client_task));
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener close invalidates handle") {
  auto result = TcpListener::make({
      .addr = Ipv4Addr::loopback().to_sockaddr(0),
  });

  COROSIG_REQUIRE(result.is_ok());
  TcpListener sock = std::move(result).value();
  COROSIG_REQUIRE(sock.underlying_handle() >= 0);

  sock.close();
  COROSIG_REQUIRE(sock.underlying_handle() == -1);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener underlying_handle returns correct value") {
  auto result = TcpListener::make({
      .addr = Ipv4Addr::loopback().to_sockaddr(0),
  });

  COROSIG_REQUIRE(result.is_ok());
  TcpListener sock = std::move(result).value();
  int handle = sock.underlying_handle();

  COROSIG_REQUIRE(handle >= 0);
}

COROSIG_SIGHANDLER_TEST_CASE("TcpListener destructor closes handle") {
  int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  COROSIG_REQUIRE(fd >= 0);

  {
    TcpListener sock = TcpListener::make_from_os_specific_handle(fd);
    COROSIG_REQUIRE(sock.underlying_handle() == fd);
  }
}
