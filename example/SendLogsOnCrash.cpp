/// Use case: there is an application, producing logs. Logs are initially stored
/// in some in-memory buffer. Periodically, a flush operation is triggered and all
/// logs are written to various outputs: to remote udp server and to file. But what if
/// an app receives malicious signal (such as SIGABRT when std::terminate is called or
/// SIGFPE if current c++ implementation raises it on zero division) then in-buffer
/// logs are lost by default. To prevent this we have to write custom signal handler.
/// And to speed this sighandler up we will use corosig-powered async io

#include "corosig/reactor/Reactor.hpp"

#include <boost/outcome/try.hpp>
#include <corosig/Coro.hpp>
#include <corosig/ErrorTypes.hpp>
#include <corosig/Parallel.hpp>
#include <corosig/Result.hpp>
#include <corosig/Sighandler.hpp>
#include <corosig/io/File.hpp>
#include <corosig/io/TcpSocket.hpp>
#include <csignal>
#include <fstream>
#include <netinet/in.h>
#include <thread>

namespace {

sockaddr_storage const SERVER_ADDR = [] {
  ::sockaddr_storage addr;
  std::memset(&addr, 0, sizeof(addr));

  auto *addr4 = (::sockaddr_in *)&addr;
  addr4->sin_family = AF_INET;
  addr4->sin_port = htons(8080);
  addr4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return addr;
}();

constexpr std::string_view FILE1 = "file1.log";
constexpr std::string_view FILE2 = "file2.log";

std::vector<std::string> logs_buffer;

namespace sighandling {

using namespace corosig;

Fut<void, Error<AllocationError, SyscallError>> write_to_file(Reactor &r,
                                                              char const *path) noexcept {
  using enum File::OpenFlags;
  BOOST_OUTCOME_CO_TRY(auto file, co_await File::open(r, path, CREATE | TRUNCATE | WRONLY));
  for (auto &log : logs_buffer) {
    BOOST_OUTCOME_CO_TRY(co_await file.write(r, log));
  }
  co_return success();
};

Fut<void, Error<AllocationError, SyscallError>> send_via_tcp(Reactor &r) {
  BOOST_OUTCOME_CO_TRY(auto socket, co_await TcpSocket::connect(r, SERVER_ADDR));
  for (auto &log : logs_buffer) {
    BOOST_OUTCOME_CO_TRY(co_await socket.write(r, log));
  }
  co_return success();
};

Fut<void, Error<AllocationError, SyscallError>> sighandler(Reactor &r, int) noexcept {

  BOOST_OUTCOME_CO_TRY(co_await when_all_succeed(r, write_to_file(r, FILE1.data()),
                                                 write_to_file(r, FILE2.data()), send_via_tcp(r)));
  co_return success();
}

} // namespace sighandling

void run_tcp_server(std::string &out) {
  int srv_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  assert(srv_fd >= 0);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = ::htons(8080);
  addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);

  int opt = 1;
  setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  ::bind(srv_fd, (sockaddr *)&addr, sizeof(addr)); // NOLINT
  ::listen(srv_fd, 1);

  int client = ::accept(srv_fd, nullptr, nullptr);
  std::array<char, 1024> buf;
  while (true) {
    ssize_t n = ::read(client, buf.begin(), buf.size());
    if (n <= 0) {
      break;
    }
    out += std::string_view{buf.begin(), size_t(n)};
  }
  ::close(client);
  ::close(srv_fd);
}

} // namespace

int main() {
  try {
    constexpr auto REACTOR_MEMORY = 8 * 1024;
    for (auto signal : {SIGILL, SIGFPE, SIGTERM, SIGABRT}) {
      corosig::set_sighandler<REACTOR_MEMORY, sighandling::sighandler>(signal);
    }

    std::string remote_server_data;
    auto remote_server_thread = std::jthread(run_tcp_server, std::ref(remote_server_data));

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

    remote_server_thread.join();
    std::cout << "Remote server received\n" << remote_server_data << '\n';

    return 0;
  } catch (const std::exception &e) {
    std::cout << e.what() << '\n';
    return 1;
  }
}
