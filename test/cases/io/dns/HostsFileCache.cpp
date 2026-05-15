#include "corosig/io/dns/HostsFileCache.hpp"

#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

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

TEST_CASE("HostsFileCache: finds IPv4 address") {
  write_hosts_file("# Test hosts file\n"
                   "127.0.0.1 localhost\n"
                   "192.168.1.1 test.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};

      COROSIG_CO_TRY(auto result, co_await cache.pull("test.example.com", addrs));

      COROSIG_REQUIRE(result == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.1.1"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: finds IPv6 address") {
  write_hosts_file("# IPv6 test\n"
                   "::1 localhost\n"
                   "2001:db8::1 ipv6.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv6Addr>, 4> addrs{};
      COROSIG_CO_TRY(auto result, co_await cache.pull("ipv6.example.com", addrs));

      COROSIG_REQUIRE(result == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv6Addr::parse("2001:db8::1"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: returns NameNotCached for unknown hostname") {
  write_hosts_file("127.0.0.1 localhost\n");

  run_in_sighandler([](Reactor &reactor) {
    dns::HostsFileCache cache{reactor, g_test_hosts_file};

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto result = cache.pull("unknown.example.com", addrs).block_on();

    COROSIG_REQUIRE(result.is_ok());
    COROSIG_REQUIRE(result.value() == 0);
  });
}

TEST_CASE("HostsFileCache: handles multiple addresses for hostname") {
  write_hosts_file("192.168.1.10 multi1.example.com multi2.example.com\n"
                   "192.168.1.11 multi3.example.com\n"
                   "192.168.1.10 multi4.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
      COROSIG_CO_TRY(auto result, co_await cache.pull("multi1.example.com", addrs));

      COROSIG_REQUIRE(result == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.1.10"));

      COROSIG_CO_TRY(size_t result2, co_await cache.pull("multi2.example.com", addrs));
      COROSIG_REQUIRE(result2 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.1.10"));

      COROSIG_CO_TRY(size_t result3, co_await cache.pull("multi3.example.com", addrs));
      COROSIG_REQUIRE(result3 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.1.11"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: handles comments and empty lines") {
  write_hosts_file("# This is a comment\n"
                   "\n"
                   "    # Another comment\n"
                   "10.0.0.1 host1.example.com # inline comment\n"
                   "\n"
                   "10.0.0.2 host2.example.com\n"
                   "# End comment\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};

      COROSIG_CO_TRY(size_t result1, co_await cache.pull("host1.example.com", addrs));
      COROSIG_REQUIRE(result1 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("10.0.0.1"));

      COROSIG_CO_TRY(size_t result2, co_await cache.pull("host2.example.com", addrs));
      COROSIG_REQUIRE(result2 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("10.0.0.2"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: handles whitespace variations") {
  write_hosts_file("127.0.0.1\tlocalhost\n"
                   "192.168.1.5\t  \t spaced.example.com\n"
                   "10.0.0.100 another.example.com\t  \n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};

      COROSIG_CO_TRY(size_t result1, co_await cache.pull("spaced.example.com", addrs));
      COROSIG_REQUIRE(result1 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.1.5"));

      COROSIG_CO_TRY(size_t result2, co_await cache.pull("another.example.com", addrs));
      COROSIG_REQUIRE(result2 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("10.0.0.100"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: fills available output buffer") {
  write_hosts_file("192.168.1.100 host1.example.com\n"
                   "192.168.1.101 host1.example.com\n"
                   "192.168.1.102 host1.example.com\n"
                   "192.168.1.103 host1.example.com\n"
                   "192.168.1.104 host1.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 3> addrs{};
      COROSIG_CO_TRY(auto result, co_await cache.pull("host1.example.com", addrs));

      COROSIG_REQUIRE(result == 3);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.1.100"));
      COROSIG_REQUIRE(addrs[1].address == *Ipv4Addr::parse("192.168.1.101"));
      COROSIG_REQUIRE(addrs[2].address == *Ipv4Addr::parse("192.168.1.102"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: handles leading whitespace on lines") {
  write_hosts_file("  127.0.0.1 localhost\n"
                   "\t192.168.2.1\tled.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
      COROSIG_CO_TRY(size_t result, co_await cache.pull("led.example.com", addrs));

      COROSIG_REQUIRE(result == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.2.1"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: ignores malformed lines gracefully") {
  write_hosts_file("127.0.0.1 valid.example.com\n"
                   "malformed_line_without_addr\n"
                   "10.0.0.5 another.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
      COROSIG_CO_TRY(size_t result, co_await cache.pull("another.example.com", addrs));

      COROSIG_REQUIRE(result == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("10.0.0.5"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: case insensitive hostname matching") {
  write_hosts_file("127.0.0.1 LocalHost\n"
                   "192.168.3.1 MixedCase.Example.Com\n"
                   "192.168.3.2 lowercase.example.com\n");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};

      COROSIG_CO_TRY(size_t result1, co_await cache.pull("localhost", addrs));
      COROSIG_REQUIRE(result1 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("127.0.0.1"));

      COROSIG_CO_TRY(size_t result2, co_await cache.pull("mixedcase.example.com", addrs));
      COROSIG_REQUIRE(result2 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.3.1"));

      COROSIG_CO_TRY(size_t result3, co_await cache.pull("LoWeRcAsE.ExaMple.cOm", addrs));
      COROSIG_REQUIRE(result3 == 1);
      COROSIG_REQUIRE(addrs[0].address == *Ipv4Addr::parse("192.168.3.2"));

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("HostsFileCache: underlying_reactor returns reactor") {
  write_hosts_file("# empty file\n");

  run_in_sighandler([](Reactor &reactor) {
    dns::HostsFileCache cache{reactor, g_test_hosts_file};
    COROSIG_REQUIRE(&cache.underlying_reactor() == &reactor);
  });
}

TEST_CASE("HostsFileCache: handles empty file") {
  write_hosts_file("");

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      dns::HostsFileCache cache{r, g_test_hosts_file};

      std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
      auto result = co_await cache.pull("any.example.com", addrs);

      COROSIG_REQUIRE(result.is_ok());
      COROSIG_REQUIRE(result.value() == 0);

      co_return Ok{};
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}
