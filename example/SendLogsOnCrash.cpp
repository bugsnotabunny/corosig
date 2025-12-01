/// Use case: there is an application, producing logs. Logs are initially stored
/// in some in-memory buffer. Periodically, a flush operation is triggered and all
/// logs are written to various outputs: to remote udp server and to file. But what if
/// an app receives malicious signal (such as SIGABRT when std::terminate is called or
/// SIGFPE if current c++ implementation raises it on zero division) then in-buffer
/// logs are lost by default. To prevent this we have to write custom signal handler.
/// And to speed this sighandler up we will use corosig-powered async io

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Parallel.hpp"
#include "corosig/Result.hpp"
#include "corosig/Sighandler.hpp"
#include "corosig/io/File.hpp"
#include "corosig/io/Stdio.hpp"
#include "corosig/io/TcpSocket.hpp"
#include "corosig/io/UdpSocket.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace {

sockaddr_storage localhost(uint16_t port) noexcept {
  ::sockaddr_storage addr;
  std::memset(&addr, 0, sizeof(addr));

  auto *addr4 = (::sockaddr_in *)&addr;
  addr4->sin_family = AF_INET;
  addr4->sin_port = htons(port);
  addr4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return addr;
}

sockaddr_storage const TCP_SERVER_ADDR = localhost(8080);
sockaddr_storage const UDP_SERVER_ADDR = localhost(9090);

constexpr std::string_view FILE1 = "file1.log";
constexpr std::string_view FILE2 = "file2.log";

std::vector<std::string> logs_buffer;

void run_background_tcp_server(std::string &out) {
  int srv_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (srv_fd == -1) {
    throw std::system_error{errno, std::system_category(), "socket"};
  }

  int opt = 1;
  if (::setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    throw std::system_error{errno, std::system_category(), "setsockopt"};
  }

  auto addr = TCP_SERVER_ADDR;
  if (::bind(srv_fd, (sockaddr *)&addr, sizeof(addr)) == -1) {
    throw std::system_error{errno, std::system_category(), "bind"};
  }

  if (::listen(srv_fd, 1) == -1) {
    throw std::system_error{errno, std::system_category(), "listen"};
  }

  int client = ::accept(srv_fd, nullptr, nullptr);
  while (true) {
    std::array<char, 1024> buf;
    ssize_t n = ::read(client, buf.begin(), buf.size());
    if (n <= 0) {
      break;
    }
    out += std::string_view{buf.begin(), size_t(n)};
  }
  ::close(client);
  ::close(srv_fd);
}

void run_background_udp_server(std::string &out) {
  int srv_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (srv_fd == -1) {
    throw std::system_error{errno, std::system_category(), "socket"};
  }

  auto addr = UDP_SERVER_ADDR;
  if (bind(srv_fd, (sockaddr *)&addr, sizeof(addr)) == -1) {
    throw std::system_error{errno, std::system_category(), "bind"};
  }

  while (true) {
    std::array<char, 1024> buf;
    ssize_t n = recvfrom(srv_fd, buf.data(), buf.size(), 0, nullptr, nullptr);
    if (n <= 0) {
      break;
    }
    out += std::string_view{buf.data(), size_t(n)};
  }
  ::close(srv_fd);
}

namespace sighandling {

using namespace corosig;

Fut<void, Error<AllocationError, SyscallError>> write_to_file(Reactor &r,
                                                              char const *path) noexcept {
  using enum File::OpenFlags;
  COROSIG_CO_TRY(auto file, co_await File::open(r, path, CREATE | TRUNCATE | WRONLY));
  for (auto &log : logs_buffer) {
    COROSIG_CO_TRYV(co_await file.write(r, log));
  }
  co_return Ok{};
};

Fut<void, Error<AllocationError, SyscallError>> write_to_stdout(Reactor &r) {

  COROSIG_CO_TRYV(co_await STDOUT.write(r, "Printing logs to stdout\n"));
  for (auto &log : logs_buffer) {
    COROSIG_CO_TRYV(co_await STDOUT.write(r, log));
  }
  COROSIG_CO_TRYV(co_await STDOUT.write(r, "\n"));
  co_return Ok{};
};

Fut<void, Error<AllocationError, SyscallError>> send_via_tcp(Reactor &r) {
  COROSIG_CO_TRY(auto socket, co_await TcpSocket::connect(r, TCP_SERVER_ADDR));
  for (auto &log : logs_buffer) {
    COROSIG_CO_TRYV(co_await socket.write(r, log));
  }
  co_return Ok{};
};

Fut<void, Error<AllocationError, SyscallError>> send_via_udp(Reactor &r) {
  COROSIG_CO_TRY(auto socket, UdpSocket::writer());
  for (auto &log : logs_buffer) {
    COROSIG_CO_TRYV(co_await socket.send_to(r, log, UDP_SERVER_ADDR));
  }
  co_return Ok{};
};

Fut<void, Error<AllocationError, SyscallError>> sighandler(Reactor &r, int) noexcept {

  Result res = co_await when_all_succeed(r,
                                         write_to_file(r, FILE1.data()),
                                         write_to_file(r, FILE2.data()),
                                         send_via_tcp(r),
                                         send_via_udp(r),
                                         write_to_stdout(r));

  if (!res.is_ok()) {
    COROSIG_CO_TRYV(co_await STDERR.write(r, "One of transmissions has produced an error:"));
    COROSIG_CO_TRYV(co_await STDERR.write(r, res.error().description()));
    COROSIG_CO_TRYV(co_await STDERR.write(r, "\n"));
  }

  std::array<char, 512> message;
  size_t message_size = std::format_to_n(message.data(),
                                         message.size() - 1,
                                         "At peak Reactor has used {} bytes of memory\n\n",
                                         r.peak_memory())
                            .size;

  COROSIG_CO_TRYV(co_await STDOUT.write(r, std::string_view{message.data(), message_size}));
  co_return Ok{};
}

} // namespace sighandling

} // namespace

int main() {
  try {
    constexpr auto REACTOR_MEMORY = 8 * 1024;
    for (auto signal : {SIGILL, SIGFPE, SIGTERM, SIGABRT}) {
      corosig::set_sighandler<REACTOR_MEMORY, sighandling::sighandler>(signal);
    }

    std::string remote_tcp_server_data;
    auto remote_tcp_server_thread =
        std::jthread(run_background_tcp_server, std::ref(remote_tcp_server_data));

    std::string remote_udp_server_data;
    auto remote_udp_server_thread =
        std::jthread(run_background_udp_server, std::ref(remote_udp_server_data));

    logs_buffer.emplace_back("Log message 1\n");
    logs_buffer.emplace_back("Log message 2\n");
    logs_buffer.emplace_back("Log message 3\n");
    logs_buffer.emplace_back("Log message 4\n");

    ::raise(SIGFPE);

    {
      std::ifstream file{FILE1.data()};
      if (!file.is_open()) {
        std::cerr << "Failed to open file 1\n";
      } else {
        std::cout << "File 1 contains\n" << file.rdbuf() << '\n';
      }
    }
    {
      std::ifstream file{FILE2.data()};
      if (!file.is_open()) {
        std::cerr << "Failed to open file 2\n";
      } else {
        std::cout << "File 2 contains\n" << file.rdbuf() << '\n';
      }
    }

    remote_tcp_server_thread.join();
    std::cout << "Remote TCP server received\n" << remote_tcp_server_data << '\n';

    remote_udp_server_thread.join();
    std::cout << "Remote UDP server received\n" << remote_tcp_server_data << '\n';

    return 0;
  } catch (const std::exception &e) {
    std::cout << e.what() << '\n';
    return 1;
  }
}
