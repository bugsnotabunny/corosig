#include "corosig/io/dns/MemoryCache.hpp"

#include "corosig/io/Sockaddr.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <array>

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: push and pull IPv4 addresses") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
  std::array<dns::ResolvedAddress<Ipv4Addr>, 2> addrs{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = expires_at},
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.2").value(),
                                     .expires_at = expires_at},
  };

  auto push_result = cache.push("example.com", addrs);
  COROSIG_REQUIRE(push_result.is_ok());

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
  auto pull_result = cache.pull("EXAMPLE.COM", out_addrs);
  COROSIG_REQUIRE(pull_result.value() == 2);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.1").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);
  COROSIG_REQUIRE(out_addrs[1].address == Ipv4Addr::parse("192.168.1.2").value());
  COROSIG_REQUIRE(out_addrs[1].expires_at == expires_at);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: push and pull IPv6 addresses") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
  std::array<dns::ResolvedAddress<Ipv6Addr>, 2> addrs{
      dns::ResolvedAddress<Ipv6Addr>{.address = Ipv6Addr::parse("2001:db8::1").value(),
                                     .expires_at = expires_at},
      dns::ResolvedAddress<Ipv6Addr>{.address = Ipv6Addr::parse("2001:db8::2").value(),
                                     .expires_at = expires_at},
  };

  auto push_result = cache.push("example.com", addrs);
  COROSIG_REQUIRE(push_result.is_ok());

  std::array<dns::ResolvedAddress<Ipv6Addr>, 4> out_addrs{};
  auto pull_result = cache.pull("EXAMPLE.COM", out_addrs);
  COROSIG_REQUIRE(pull_result.value() == 2);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv6Addr::parse("2001:db8::1").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);
  COROSIG_REQUIRE(out_addrs[1].address == Ipv6Addr::parse("2001:db8::2").value());
  COROSIG_REQUIRE(out_addrs[1].expires_at == expires_at);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: push and pull mixed IPv4 and IPv6 addresses") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
  std::array<dns::ResolvedAddress<Ipv4Addr>, 2> ipv4s{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = expires_at},
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.2").value(),
                                     .expires_at = expires_at},
  };
  std::array<dns::ResolvedAddress<Ipv6Addr>, 2> ipv6s{
      dns::ResolvedAddress<Ipv6Addr>{.address = Ipv6Addr::parse("2001:db8::1").value(),
                                     .expires_at = expires_at},
      dns::ResolvedAddress<Ipv6Addr>{.address = Ipv6Addr::parse("2001:db8::2").value(),
                                     .expires_at = expires_at},
  };

  COROSIG_REQUIRE(cache.push("example.com", ipv4s).is_ok());
  COROSIG_REQUIRE(cache.push("example.com", ipv6s).is_ok());

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_ipv4s{};
  auto pull_ipv4_result = cache.pull("EXAMPLE.COM", out_ipv4s);
  COROSIG_REQUIRE(pull_ipv4_result.value() == 2);
  COROSIG_REQUIRE(out_ipv4s[0].address == Ipv4Addr::parse("192.168.1.1").value());
  COROSIG_REQUIRE(out_ipv4s[0].expires_at == expires_at);
  COROSIG_REQUIRE(out_ipv4s[1].address == Ipv4Addr::parse("192.168.1.2").value());
  COROSIG_REQUIRE(out_ipv4s[1].expires_at == expires_at);

  std::array<dns::ResolvedAddress<Ipv6Addr>, 4> out_ipv6s{};
  auto pull_ipv6_result = cache.pull("EXAMPLE.COM", out_ipv6s);
  COROSIG_REQUIRE(pull_ipv6_result.value() == 2);
  COROSIG_REQUIRE(out_ipv6s[0].address == Ipv6Addr::parse("2001:db8::1").value());
  COROSIG_REQUIRE(out_ipv6s[0].expires_at == expires_at);
  COROSIG_REQUIRE(out_ipv6s[1].address == Ipv6Addr::parse("2001:db8::2").value());
  COROSIG_REQUIRE(out_ipv6s[1].expires_at == expires_at);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: pull returns zero for unknown hostname") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
  auto pull_result = cache.pull("unknown.example.com", out_addrs);
  COROSIG_REQUIRE(pull_result.value() == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: fills available output buffer") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
  std::array<dns::ResolvedAddress<Ipv4Addr>, 5> addrs{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = expires_at},
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.2").value(),
                                     .expires_at = expires_at},
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.3").value(),
                                     .expires_at = expires_at},
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.4").value(),
                                     .expires_at = expires_at},
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.5").value(),
                                     .expires_at = expires_at},
  };

  COROSIG_REQUIRE(cache.push("example.com", addrs).is_ok());

  std::array<dns::ResolvedAddress<Ipv4Addr>, 3> out_addrs{};
  auto pull_result = cache.pull("EXAMPLE.COM", out_addrs);
  COROSIG_REQUIRE(pull_result.value() == 3);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.1").value());
  COROSIG_REQUIRE(out_addrs[1].address == Ipv4Addr::parse("192.168.1.2").value());
  COROSIG_REQUIRE(out_addrs[2].address == Ipv4Addr::parse("192.168.1.3").value());
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: pruning removes expired entries") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto expired_at = SteadyClock::now() - std::chrono::seconds{3600};
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> expired_addrs{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = expired_at},
  };

  auto valid_at = SteadyClock::now() + std::chrono::seconds{3600};
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> valid_addrs{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.2").value(),
                                     .expires_at = valid_at},
  };

  COROSIG_REQUIRE(cache.push("expired.example.com", expired_addrs).is_ok());
  COROSIG_REQUIRE(cache.push("valid.example.com", valid_addrs).is_ok());

  cache.prune();

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};

  auto expired_pull_result = cache.pull("expired.example.com", out_addrs);
  COROSIG_REQUIRE(expired_pull_result.value() == 0);

  auto valid_pull_result = cache.pull("VALID.EXAMPLE.COM", out_addrs);
  COROSIG_REQUIRE(valid_pull_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.2").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == valid_at);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: handles multiple entries with different names") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};

  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> addrs1{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = expires_at},
  };
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> addrs2{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("10.0.0.1").value(),
                                     .expires_at = expires_at},
  };
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> addrs3{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("172.16.0.1").value(),
                                     .expires_at = expires_at},
  };

  COROSIG_REQUIRE(cache.push("host1.example.com", addrs1).is_ok());
  COROSIG_REQUIRE(cache.push("host2.example.com", addrs2).is_ok());
  COROSIG_REQUIRE(cache.push("host3.example.com", addrs3).is_ok());

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};

  auto pull1_result = cache.pull("HOST1.EXAMPLE.COM", out_addrs);
  COROSIG_REQUIRE(pull1_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.1").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);

  auto pull2_result = cache.pull("host2.example.com", out_addrs);
  COROSIG_REQUIRE(pull2_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("10.0.0.1").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);

  auto pull3_result = cache.pull("Host3.Example.com", out_addrs);
  COROSIG_REQUIRE(pull3_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("172.16.0.1").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: handles multiple address records for same name") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};

  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> addrs1{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = expires_at},
  };
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> addrs2{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.2").value(),
                                     .expires_at = expires_at},
  };

  COROSIG_REQUIRE(cache.push("multi.example.com", addrs1).is_ok());
  COROSIG_REQUIRE(cache.push("multi.example.com", addrs2).is_ok());

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
  auto pull_result = cache.pull("MULTI.EXAMPLE.COM", out_addrs);
  COROSIG_REQUIRE(pull_result.value() == 2);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.2").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == expires_at);
  COROSIG_REQUIRE(out_addrs[1].address == Ipv4Addr::parse("192.168.1.1").value());
  COROSIG_REQUIRE(out_addrs[1].expires_at == expires_at);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: handles empty address lists") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<dns::ResolvedAddress<Ipv4Addr>, 0> ipv4s{};
  std::array<dns::ResolvedAddress<Ipv6Addr>, 0> ipv6s{};

  COROSIG_REQUIRE(cache.push(".example.com", ipv4s).is_ok());
  COROSIG_REQUIRE(cache.push(".example.com", ipv6s).is_ok());

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_ipv4s{};
  auto pull_ipv4_result = cache.pull(".EXAMPLE.COM", out_ipv4s);
  COROSIG_REQUIRE(pull_ipv4_result.value() == 0);

  std::array<dns::ResolvedAddress<Ipv6Addr>, 4> out_ipv6s{};
  auto pull_ipv6_result = cache.pull(".example.com", out_ipv6s);
  COROSIG_REQUIRE(pull_ipv6_result.value() == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: returns allocation failure on OOM") {
  Allocator::Memory<64> mem;
  Allocator alloc{mem};
  dns::MemoryCache<> cache{alloc};

  std::array<dns::ResolvedAddress<Ipv4Addr>, 10> addrs{};
  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};

  for (size_t i = 0; i < addrs.size(); ++i) {
    addrs[i] = dns::ResolvedAddress<Ipv4Addr>{
        .address = Ipv4Addr::parse(std::format("192.168.1.{}", i + 1)).value(),
        .expires_at = expires_at};
  }

  auto push_result = cache.push("example.com", addrs);
  COROSIG_REQUIRE(!push_result.is_ok());
  COROSIG_REQUIRE(push_result.error() == AllocationError{});
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: handles long hostnames") {
  dns::MemoryCache<> cache{reactor.allocator()};

  constexpr std::string_view long_name =
      "a.very.long.hostname.that.goes.on.and.on.subdomain.example.com";

  COROSIG_REQUIRE(
      cache
          .push(long_name,
                std::array<dns::ResolvedAddress<Ipv4Addr>, 1>{dns::ResolvedAddress<Ipv4Addr>{
                    .address = Ipv4Addr::parse("192.168.1.1").value(),
                    .expires_at = SteadyClock::now() + std::chrono::seconds{3600}}})
          .is_ok());

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
  auto pull_result = cache.pull(long_name, out_addrs);
  COROSIG_REQUIRE(pull_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.1").value());
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: prune with multiple expired entries") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto now = SteadyClock::now();

  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> expired1{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = now - std::chrono::hours{2}},
  };
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> expired2{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = now - std::chrono::hours{1}},
  };

  auto valid_at = now + std::chrono::hours{1};
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> valid{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = valid_at},
  };

  COROSIG_REQUIRE(cache.push("expired1.example.com", expired1).is_ok());
  COROSIG_REQUIRE(cache.push("expired2.example.com", expired2).is_ok());
  COROSIG_REQUIRE(cache.push("valid.example.com", valid).is_ok());

  cache.prune();

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};

  auto pull_expired1 = cache.pull("expired1.example.com", out_addrs);
  COROSIG_REQUIRE(pull_expired1.value() == 0);

  auto pull_expired2 = cache.pull("EXPIRED2.EXAMPLE.COM", out_addrs);
  COROSIG_REQUIRE(pull_expired2.value() == 0);

  auto pull_valid = cache.pull("valid.example.com", out_addrs);
  COROSIG_REQUIRE(pull_valid.value() == 1);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.1").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == valid_at);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: multiple pulls after prune") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto now = SteadyClock::now();

  auto expired_at = now - std::chrono::seconds{3600};
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> expired_addrs{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.1").value(),
                                     .expires_at = expired_at},
  };

  auto valid_at = now + std::chrono::seconds{3600};
  std::array<dns::ResolvedAddress<Ipv4Addr>, 1> valid_addrs{
      dns::ResolvedAddress<Ipv4Addr>{.address = Ipv4Addr::parse("192.168.1.2").value(),
                                     .expires_at = valid_at},
  };

  COROSIG_REQUIRE(cache.push("test.example.com", expired_addrs).is_ok());
  COROSIG_REQUIRE(cache.push("test.example.com", valid_addrs).is_ok());

  std::array<dns::ResolvedAddress<Ipv4Addr>, 4> out_addrs{};
  auto pull_before = cache.pull("TEST.EXAMPLE.COM", out_addrs);
  COROSIG_REQUIRE(pull_before.value() == 1);

  cache.prune();

  auto pull_after = cache.pull("test.example.com", out_addrs);
  COROSIG_REQUIRE(pull_after.value() == 1);
  COROSIG_REQUIRE(out_addrs[0].address == Ipv4Addr::parse("192.168.1.2").value());
  COROSIG_REQUIRE(out_addrs[0].expires_at == valid_at);
}
