#include "corosig/io/dns/Cache.hpp"

#include "corosig/io/Sockaddr.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <random>

namespace {

using namespace corosig;

char const *g_test_hosts_file{nullptr};

std::filesystem::path tmp_hosts_file() {
  auto tmp_dir = std::filesystem::temp_directory_path();
  std::array<char, 16> filename{};
  do {
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<char> distr{'a', 'z'};
    auto rand_letter = [&] { return distr(gen); };
    std::ranges::generate_n(filename.begin(), std::size(filename) - 1, rand_letter);
  } while (std::filesystem::exists(tmp_dir / std::string_view{filename.data(), filename.size()}));
  return tmp_dir / std::string_view{filename.data(), filename.size()};
}

struct TestListener : Catch::EventListenerBase {
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const &) override {
    file = tmp_hosts_file();
    g_test_hosts_file = file.c_str();
  }
  void testCaseEnded(Catch::TestCaseStats const &) override {
    std::filesystem::remove(file);
    g_test_hosts_file = nullptr;
  }

  std::filesystem::path file;
};

CATCH_REGISTER_LISTENER(TestListener);

void write_hosts_file(std::string_view content) {
  std::ofstream ofs(g_test_hosts_file);
  ofs << content;
}

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Cache: pull returns from HostsFileCache when hostname exists there") {
  write_hosts_file("192.168.1.100 hosts.example.com\n");

  dns::Cache<> cache{reactor, g_test_hosts_file, reactor.allocator()};

  auto foo = [](dns::Cache<> &c, Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto result = co_await c.pull("hosts.example.com", addrs);

    COROSIG_REQUIRE(result.value() == 1);
    COROSIG_REQUIRE(addrs[0].address == Ipv4Addr::parse("192.168.1.100").value());

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(cache, reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Cache: pull falls back to MemoryCache when not in hosts file") {
  write_hosts_file("127.0.0.1 localhost\n");

  dns::Cache<> cache{reactor, g_test_hosts_file, reactor.allocator()};

  auto foo = [](dns::Cache<> &c, Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
    std::array<dns::ResolvedAddress<Ipv4Addr>, 1> addrs{dns::ResolvedAddress<Ipv4Addr>{
        .address = Ipv4Addr::parse("10.0.0.1").value(),
        .expires_at = expires_at,
    }};

    auto push_result = c.push("mem.example.com", addrs);
    COROSIG_REQUIRE(push_result.is_ok());

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
    auto result = co_await c.pull("mem.example.com", out_addrs);

    COROSIG_REQUIRE(result.value() == 1);
    COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("10.0.0.1").value());
    COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(cache, reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Cache: pull returns zero when hostname in neither cache") {
  write_hosts_file("127.0.0.1 localhost\n");

  dns::Cache<> cache{reactor, g_test_hosts_file, reactor.allocator()};

  auto foo = [](dns::Cache<> &c, Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto result = co_await c.pull("unknown.example.com", addrs);

    COROSIG_REQUIRE(result.value() == 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(cache, reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Cache: HostsFileCache takes priority over MemoryCache") {
  write_hosts_file("192.168.1.200 priority.example.com\n");

  dns::Cache<> cache{reactor, g_test_hosts_file, reactor.allocator()};

  auto foo = [](dns::Cache<> &c, Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
    std::array<dns::ResolvedAddress<Ipv4Addr>, 1> mem_addrs{dns::ResolvedAddress<Ipv4Addr>{
        .address = Ipv4Addr::parse("10.0.0.50").value(),
        .expires_at = expires_at,
    }};

    auto push_result = c.push("priority.example.com", mem_addrs);
    COROSIG_REQUIRE(push_result.is_ok());

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
    auto result = co_await c.pull("priority.example.com", out_addrs);

    COROSIG_REQUIRE(result.value() == 1);
    COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.200").value());

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(cache, reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Cache: IPv6 pull works correctly") {
  write_hosts_file("2001:db8::1 ipv6hosts.example.com\n");

  dns::Cache<> cache{reactor, g_test_hosts_file, reactor.allocator()};

  auto foo = [](dns::Cache<> &c, Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    std::array<dns::ResolvedAddress<Ipv6Addr>, 4> addrs{};
    auto result = co_await c.pull("ipv6hosts.example.com", addrs);

    COROSIG_REQUIRE(result.value() == 1);
    COROSIG_REQUIRE(addrs[0].address == Ipv6Addr::parse("2001:db8::1").value());

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(cache, reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Cache: push delegates to MemoryCache") {
  write_hosts_file("# Empty hosts file\n");

  dns::Cache<> cache{reactor, g_test_hosts_file, reactor.allocator()};

  auto foo = [](dns::Cache<> &c, Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
    std::array<dns::ResolvedAddress<Ipv4Addr>, 2> addrs{
        dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.2.1").value(),
                                       .expires_at = expires_at},
        dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.2.2").value(),
                                       .expires_at = expires_at},
    };

    auto push_result = c.push("pushed.example.com", addrs);
    COROSIG_REQUIRE(push_result.is_ok());

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
    auto result = co_await c.pull("pushed.example.com", out_addrs);

    COROSIG_REQUIRE(result.value() == 2);
    COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.2.1").value());
    COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);
    COROSIG_REQUIRE(out_addrs[1].address == Ipv4Addr::parse("192.168.2.2").value());
    COROSIG_REQUIRE(out_addrs[1].expires_at == expires_at);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(cache, reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Cache: prune delegates to MemoryCache") {
  write_hosts_file("# Empty hosts file\n");

  dns::Cache<> cache{reactor, g_test_hosts_file, reactor.allocator()};

  auto foo = [](dns::Cache<> &c, Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    auto now = SteadyClock::now();
    auto expired_at = now - std::chrono::seconds{3600};

    std::array<dns::ResolvedAddress<Ipv4Addr>, 1> expired_addrs{dns::ResolvedAddress<Ipv4Addr>{
        .address = Ipv4Addr::parse("192.168.3.1").value(),
        .expires_at = expired_at,
    }};

    auto push_result = c.push("expired.example.com", expired_addrs);
    COROSIG_REQUIRE(push_result.is_ok());

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
    auto result1 = co_await c.pull("expired.example.com", out_addrs);
    COROSIG_REQUIRE(result1.value() == 0);

    c.prune();

    auto result2 = co_await c.pull("expired.example.com", out_addrs);
    COROSIG_REQUIRE(result2.value() == 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(foo(cache, reactor).block_on().is_ok());
}
