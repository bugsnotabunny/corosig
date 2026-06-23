#include "corosig/io/dns/Cache.hpp"

#include "corosig/io/Sockaddr.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"
#include "corosig/testing/TemporaryFileTestListener.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace corosig;
using namespace corosig::testing;

CATCH_REGISTER_LISTENER(TemporaryFileTestListener);

} // namespace

TEST_CASE("Cache: pull returns from HostsFileCache when hostname exists there") {
  write_temp_file("192.168.1.100 hosts.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r,
                  char const *hosts_file) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::Cache<> cache{r, hosts_file, r.allocator()};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
      auto result = co_await cache.pull("hosts.example.com", addrs);

      COROSIG_REQUIRE(result.value() == 1);
      COROSIG_REQUIRE(addrs[0].address == Ipv4Addr::parse("192.168.1.100").value());

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor, g_temp_test_file).block_on().is_ok());
  });
}

TEST_CASE("Cache: pull falls back to MemoryCache when not in hosts file") {
  write_temp_file("127.0.0.1 localhost\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r,
                  char const *hosts_file) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::Cache<> cache{r, hosts_file, r.allocator()};

      auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
      std::array<dns::ResolvedAddress<Ipv4Addr>, 1> addrs{dns::ResolvedAddress<Ipv4Addr>{
          .address = Ipv4Addr::parse("10.0.0.1").value(),
          .expires_at = expires_at,
      }};

      auto push_result = cache.push("mem.example.com", addrs);
      COROSIG_REQUIRE(push_result.is_ok());

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
      auto result = co_await cache.pull("mem.example.com", out_addrs);

      COROSIG_REQUIRE(result.value() == 1);
      COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("10.0.0.1").value());
      COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor, g_temp_test_file).block_on().is_ok());
  });
}

TEST_CASE("Cache: pull returns zero when hostname in neither cache") {
  write_temp_file("127.0.0.1 localhost\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r,
                  char const *hosts_file) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::Cache<> cache{r, hosts_file, r.allocator()};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
      auto result = co_await cache.pull("unknown.example.com", addrs);

      COROSIG_REQUIRE(result.value() == 0);

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor, g_temp_test_file).block_on().is_ok());
  });
}

TEST_CASE("Cache: HostsFileCache takes priority over MemoryCache") {
  write_temp_file("192.168.1.200 priority.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r,
                  char const *hosts_file) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::Cache<> cache{r, hosts_file, r.allocator()};

      auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
      std::array<dns::ResolvedAddress<Ipv4Addr>, 1> mem_addrs{dns::ResolvedAddress<Ipv4Addr>{
          .address = Ipv4Addr::parse("10.0.0.50").value(),
          .expires_at = expires_at,
      }};

      auto push_result = cache.push("priority.example.com", mem_addrs);
      COROSIG_REQUIRE(push_result.is_ok());

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
      auto result = co_await cache.pull("priority.example.com", out_addrs);

      COROSIG_REQUIRE(result.value() == 1);
      COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.200").value());

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor, g_temp_test_file).block_on().is_ok());
  });
}

TEST_CASE("Cache: IPv6 pull works correctly") {
  write_temp_file("2001:db8::1 ipv6hosts.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r,
                  char const *hosts_file) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::Cache<> cache{r, hosts_file, r.allocator()};

      std::array<dns::ResolvedAddress<Ipv6Addr>, 4> addrs{};
      auto result = co_await cache.pull("ipv6hosts.example.com", addrs);

      COROSIG_REQUIRE(result.value() == 1);
      COROSIG_REQUIRE(addrs[0].address == Ipv6Addr::parse("2001:db8::1").value());

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor, g_temp_test_file).block_on().is_ok());
  });
}

TEST_CASE("Cache: push delegates to MemoryCache") {
  write_temp_file("# Empty hosts file\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r,
                  char const *hosts_file) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::Cache<> cache{r, hosts_file, r.allocator()};

      auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
      std::array<dns::ResolvedAddress<Ipv4Addr>, 2> addrs{
          dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.2.1").value(),
                                         .expires_at = expires_at},
          dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.2.2").value(),
                                         .expires_at = expires_at},
      };

      auto push_result = cache.push("pushed.example.com", addrs);
      COROSIG_REQUIRE(push_result.is_ok());

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
      auto result = co_await cache.pull("pushed.example.com", out_addrs);

      COROSIG_REQUIRE(result.value() == 2);
      COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.2.1").value());
      COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);
      COROSIG_REQUIRE(out_addrs[1].address == Ipv4Addr::parse("192.168.2.2").value());
      COROSIG_REQUIRE(out_addrs[1].expires_at == expires_at);

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor, g_temp_test_file).block_on().is_ok());
  });
}

TEST_CASE("Cache: prune delegates to MemoryCache") {
  write_temp_file("# Empty hosts file\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r,
                  char const *hosts_file) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::Cache<> cache{r, hosts_file, r.allocator()};

      auto now = SteadyClock::now();
      auto expired_at = now - std::chrono::seconds{3600};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 1> expired_addrs{dns::ResolvedAddress<Ipv4Addr>{
          .address = Ipv4Addr::parse("192.168.3.1").value(),
          .expires_at = expired_at,
      }};

      auto push_result = cache.push("expired.example.com", expired_addrs);
      COROSIG_REQUIRE(push_result.is_ok());

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
      auto result1 = co_await cache.pull("expired.example.com", out_addrs);
      COROSIG_REQUIRE(result1.value() == 0);

      cache.prune();

      auto result2 = co_await cache.pull("expired.example.com", out_addrs);
      COROSIG_REQUIRE(result2.value() == 0);

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor, g_temp_test_file).block_on().is_ok());
  });
}
