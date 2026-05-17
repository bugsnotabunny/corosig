#include "corosig/Clock.hpp"
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Random.hpp"
#include "corosig/Result.hpp"
#include "corosig/Sighandler.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/Stdio.hpp"
#include "corosig/io/TcpSocket.hpp"
#include "corosig/io/dns/Cache.hpp"
#include "corosig/io/dns/HostsFileCache.hpp"
#include "corosig/io/dns/Resolver.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <format>
#include <string_view>
#include <utility>

namespace {

using namespace corosig;

template <typename... ARGS>
Fut<void, Error<AllocationError, SyscallError>>
log(Reactor &r, std::format_string<ARGS...> fmt, ARGS &&...args) noexcept {
  std::array<char, 128> fmt_buf;
  auto *fmt_buf_end =
      std::format_to_n(fmt_buf.begin(), fmt_buf.size(), fmt, std::forward<ARGS>(args)...).out;
  COROSIG_CO_TRYV(co_await STDOUT.write(r, std::string_view{fmt_buf.begin(), fmt_buf_end}));
  co_return Ok{};
}

Fut<dns::Resolver<>, Error<AllocationError, SyscallError>> make_dns_resolver(Reactor &r) noexcept {
  std::array<char, 32> rnd_seed;
  COROSIG_CO_TRYV(co_await read_dev_urandom(r, rnd_seed));
  COROSIG_CO_TRY(auto cacheless_resolver,
                 dns::CachelessResolver::make(rnd_seed, Ipv4Addr{}.to_sockaddr()));

  co_return dns::Resolver{
      dns::Cache<>{r, dns::HOSTS_FILE_PATH, r.allocator()},
      std::move(cacheless_resolver),
  };
}

Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>>
sighandler_impl(corosig::Reactor &r, int) noexcept {
  constexpr std::string_view WEBSITE_NAME = "google.com";

  COROSIG_CO_TRY(dns::Resolver resolver, co_await make_dns_resolver(r));

  COROSIG_CO_TRYV(co_await log(r, "Trying to resolve {}\n", WEBSITE_NAME));

  auto resolve_start = SteadyClock::now();

  // NOLINTNEXTLINE (bugprone-unchecked-optional-access)
  SockaddrStorage dns_server_addr = Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT);
  COROSIG_CO_TRY(
      auto addr,
      co_await resolver.resolve_name1<Ipv4Addr>(r, std::span{&dns_server_addr, 1}, WEBSITE_NAME));

  COROSIG_CO_TRYV(co_await log(
      r,
      "Resolved {} to 0x{:x} in {}\nMaking request to it now\n\n",
      WEBSITE_NAME,
      addr.address.value(),
      std::chrono::round<std::chrono::milliseconds>(SteadyClock::now() - resolve_start)));

  constexpr uint16_t HTTP_PORT = 80;
  COROSIG_CO_TRY(auto connection,
                 co_await TcpSocket::connect(r, addr.address.to_sockaddr(HTTP_PORT)));

  COROSIG_CO_TRYV(co_await connection.write(r, "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"));

  COROSIG_CO_TRYV(co_await log(r, "Received an answer from {}:\n", WEBSITE_NAME));

  while (true) {
    std::array<char, 512> response_buf;
    COROSIG_CO_TRY(size_t response_bytes, co_await connection.read(r, response_buf));
    if (response_bytes == 0) {
      break;
    }

    COROSIG_CO_TRYV(
        co_await STDOUT.write(r, std::string_view{response_buf.data(), response_bytes}));
  }
  COROSIG_CO_TRYV(co_await STDOUT.write(r, "\n"));

  co_return Ok{};
}

Fut<void, Error<SyscallError, AllocationError, dns::ResolveError>> sighandler(corosig::Reactor &r,
                                                                              int sig) noexcept {
  if (auto res = co_await sighandler_impl(r, sig)) {
    co_return res;
  } else {
    COROSIG_CO_TRYV(co_await STDOUT.write(r, res.error().description()));
    COROSIG_CO_TRYV(co_await STDOUT.write(r, "\n"));
    co_return Failure{res.error()};
  }
}

} // namespace

int main(int, char **) {
  try {
    corosig::set_sighandler<1024 * 16, sighandler>(SIGFPE);
    ::raise(SIGFPE);
    return 0;
  } catch (std::exception const &) {
    return 1;
  }
}
