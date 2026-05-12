#include "corosig/io/dns/MemoryCache.hpp"

#include "corosig/Clock.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <catch2/catch_all.hpp>

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: push and pull IPv4 addresses") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<Ipv4Addr, 2> addrs = {*Ipv4Addr::parse("192.168.1.1"),
                                   *Ipv4Addr::parse("192.168.1.2")};

  dns::CachePushTransaction transaction{.name = "example.com",
                                        .expire_at =
                                            SteadyClock::now() + std::chrono::seconds{3600},
                                        .ipv4s = addrs};

  auto push_result = cache.push(transaction);
  COROSIG_REQUIRE(push_result.is_ok());

  std::array<Ipv4Addr, 4> out_addrs{};
  auto pull_result = cache.pull("example.com", out_addrs);
  COROSIG_REQUIRE(pull_result.is_ok());
  COROSIG_REQUIRE(pull_result.value() == 2);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("192.168.1.1"));
  COROSIG_REQUIRE(out_addrs[1] == *Ipv4Addr::parse("192.168.1.2"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: push and pull IPv6 addresses") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<Ipv6Addr, 2> addrs = {*Ipv6Addr::parse("2001:db8::1"),
                                   *Ipv6Addr::parse("2001:db8::2")};

  dns::CachePushTransaction transaction{.name = "example.com",
                                        .expire_at =
                                            SteadyClock::now() + std::chrono::seconds{3600},
                                        .ipv6s = addrs};

  auto push_result = cache.push(transaction);
  COROSIG_REQUIRE(push_result.is_ok());

  std::array<Ipv6Addr, 4> out_addrs{};
  auto pull_result = cache.pull("example.com", out_addrs);
  COROSIG_REQUIRE(pull_result.is_ok());
  COROSIG_REQUIRE(pull_result.value() == 2);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv6Addr::parse("2001:db8::1"));
  COROSIG_REQUIRE(out_addrs[1] == *Ipv6Addr::parse("2001:db8::2"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: push and pull mixed IPv4 and IPv6 addresses") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<Ipv4Addr, 2> ipv4s = {*Ipv4Addr::parse("192.168.1.1"),
                                   *Ipv4Addr::parse("192.168.1.2")};
  std::array<Ipv6Addr, 2> ipv6s = {*Ipv6Addr::parse("2001:db8::1"),
                                   *Ipv6Addr::parse("2001:db8::2")};

  dns::CachePushTransaction transaction{.name = "example.com",
                                        .expire_at =
                                            SteadyClock::now() + std::chrono::seconds{3600},
                                        .ipv4s = ipv4s,
                                        .ipv6s = ipv6s};

  auto push_result = cache.push(transaction);
  COROSIG_REQUIRE(push_result.is_ok());

  std::array<Ipv4Addr, 4> out_ipv4s{};
  auto pull_ipv4_result = cache.pull("example.com", out_ipv4s);
  COROSIG_REQUIRE(pull_ipv4_result.is_ok());
  COROSIG_REQUIRE(pull_ipv4_result.value() == 2);
  COROSIG_REQUIRE(out_ipv4s[0] == *Ipv4Addr::parse("192.168.1.1"));
  COROSIG_REQUIRE(out_ipv4s[1] == *Ipv4Addr::parse("192.168.1.2"));

  std::array<Ipv6Addr, 4> out_ipv6s{};
  auto pull_ipv6_result = cache.pull("example.com", out_ipv6s);
  COROSIG_REQUIRE(pull_ipv6_result.is_ok());
  COROSIG_REQUIRE(pull_ipv6_result.value() == 2);
  COROSIG_REQUIRE(out_ipv6s[0] == *Ipv6Addr::parse("2001:db8::1"));
  COROSIG_REQUIRE(out_ipv6s[1] == *Ipv6Addr::parse("2001:db8::2"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: pull returns zero for unknown hostname") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<Ipv4Addr, 4> out_addrs{};
  auto pull_result = cache.pull("unknown.example.com", out_addrs);
  COROSIG_REQUIRE(pull_result.is_ok());
  COROSIG_REQUIRE(pull_result.value() == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: fills available output buffer") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<Ipv4Addr, 5> addrs = {*Ipv4Addr::parse("192.168.1.1"),
                                   *Ipv4Addr::parse("192.168.1.2"),
                                   *Ipv4Addr::parse("192.168.1.3"),
                                   *Ipv4Addr::parse("192.168.1.4"),
                                   *Ipv4Addr::parse("192.168.1.5")};

  dns::CachePushTransaction transaction{.name = "example.com",
                                        .expire_at =
                                            SteadyClock::now() + std::chrono::seconds{3600},
                                        .ipv4s = addrs};

  auto push_result = cache.push(transaction);
  COROSIG_REQUIRE(push_result.is_ok());

  std::array<Ipv4Addr, 3> out_addrs{};
  auto pull_result = cache.pull("example.com", out_addrs);
  COROSIG_REQUIRE(pull_result.is_ok());
  COROSIG_REQUIRE(pull_result.value() == 3);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("192.168.1.1"));
  COROSIG_REQUIRE(out_addrs[1] == *Ipv4Addr::parse("192.168.1.2"));
  COROSIG_REQUIRE(out_addrs[2] == *Ipv4Addr::parse("192.168.1.3"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: pruning removes expired entries") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<Ipv4Addr, 1> addrs = {*Ipv4Addr::parse("192.168.1.1")};

  dns::CachePushTransaction expired_transaction{.name = "expired.example.com",
                                                .expire_at =
                                                    SteadyClock::now() - std::chrono::seconds{3600},
                                                .ipv4s = addrs};

  auto expired_push_result = cache.push(expired_transaction);
  COROSIG_REQUIRE(expired_push_result.is_ok());

  dns::CachePushTransaction valid_transaction{.name = "valid.example.com",
                                              .expire_at =
                                                  SteadyClock::now() + std::chrono::seconds{3600},
                                              .ipv4s = addrs};

  auto valid_push_result = cache.push(valid_transaction);
  COROSIG_REQUIRE(valid_push_result.is_ok());

  cache.prune();

  std::array<Ipv4Addr, 4> out_addrs{};

  auto expired_pull_result = cache.pull("expired.example.com", out_addrs);
  COROSIG_REQUIRE(expired_pull_result.is_ok());
  COROSIG_REQUIRE(expired_pull_result.value() == 0);

  auto valid_pull_result = cache.pull("valid.example.com", out_addrs);
  COROSIG_REQUIRE(valid_pull_result.is_ok());
  COROSIG_REQUIRE(valid_pull_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("192.168.1.1"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: handles multiple entries with different names") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<Ipv4Addr, 1> addrs1 = {*Ipv4Addr::parse("192.168.1.1")};
  std::array<Ipv4Addr, 1> addrs2 = {*Ipv4Addr::parse("10.0.0.1")};
  std::array<Ipv4Addr, 1> addrs3 = {*Ipv4Addr::parse("172.16.0.1")};

  dns::CachePushTransaction transaction1{.name = "host1.example.com",
                                         .expire_at =
                                             SteadyClock::now() + std::chrono::seconds{3600},
                                         .ipv4s = addrs1};

  dns::CachePushTransaction transaction2{.name = "host2.example.com",
                                         .expire_at =
                                             SteadyClock::now() + std::chrono::seconds{3600},
                                         .ipv4s = addrs2};

  dns::CachePushTransaction transaction3{.name = "host3.example.com",
                                         .expire_at =
                                             SteadyClock::now() + std::chrono::seconds{3600},
                                         .ipv4s = addrs3};

  COROSIG_REQUIRE(cache.push(transaction1).is_ok());
  COROSIG_REQUIRE(cache.push(transaction2).is_ok());
  COROSIG_REQUIRE(cache.push(transaction3).is_ok());

  std::array<Ipv4Addr, 4> out_addrs{};

  auto pull1_result = cache.pull("host1.example.com", out_addrs);
  COROSIG_REQUIRE(pull1_result.is_ok());
  COROSIG_REQUIRE(pull1_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("192.168.1.1"));

  auto pull2_result = cache.pull("host2.example.com", out_addrs);
  COROSIG_REQUIRE(pull2_result.is_ok());
  COROSIG_REQUIRE(pull2_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("10.0.0.1"));

  auto pull3_result = cache.pull("host3.example.com", out_addrs);
  COROSIG_REQUIRE(pull3_result.is_ok());
  COROSIG_REQUIRE(pull3_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("172.16.0.1"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: handles multiple address records for same name") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::array<Ipv4Addr, 1> addrs1 = {*Ipv4Addr::parse("192.168.1.1")};
  std::array<Ipv4Addr, 1> addrs2 = {*Ipv4Addr::parse("192.168.1.2")};

  dns::CachePushTransaction transaction1{.name = "multi.example.com",
                                         .expire_at =
                                             SteadyClock::now() + std::chrono::seconds{3600},
                                         .ipv4s = addrs1};

  dns::CachePushTransaction transaction2{.name = "multi.example.com",
                                         .expire_at =
                                             SteadyClock::now() + std::chrono::seconds{3600},
                                         .ipv4s = addrs2};

  COROSIG_REQUIRE(cache.push(transaction1).is_ok());
  COROSIG_REQUIRE(cache.push(transaction2).is_ok());

  std::array<Ipv4Addr, 4> out_addrs{};
  auto pull_result = cache.pull("multi.example.com", out_addrs);
  COROSIG_REQUIRE(pull_result.is_ok());
  COROSIG_REQUIRE(pull_result.value() == 2);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("192.168.1.2"));
  COROSIG_REQUIRE(out_addrs[1] == *Ipv4Addr::parse("192.168.1.1"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: handles empty address lists") {
  dns::MemoryCache<> cache{reactor.allocator()};

  dns::CachePushTransaction transaction{.name = ".example.com",
                                        .expire_at =
                                            SteadyClock::now() + std::chrono::seconds{3600},
                                        .ipv4s = {},
                                        .ipv6s = {}};

  auto push_result = cache.push(transaction);
  COROSIG_REQUIRE(push_result.is_ok());

  std::array<Ipv4Addr, 4> out_ipv4s{};
  auto pull_ipv4_result = cache.pull(".example.com", out_ipv4s);
  COROSIG_REQUIRE(pull_ipv4_result.is_ok());
  COROSIG_REQUIRE(pull_ipv4_result.value() == 0);

  std::array<Ipv6Addr, 4> out_ipv6s{};
  auto pull_ipv6_result = cache.pull(".example.com", out_ipv6s);
  COROSIG_REQUIRE(pull_ipv6_result.is_ok());
  COROSIG_REQUIRE(pull_ipv6_result.value() == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: returns allocation failure on OOM") {
  Allocator::Memory<64> mem;
  Allocator alloc{mem};
  dns::MemoryCache<> cache{alloc};

  std::array<Ipv4Addr, 10> addrs{};
  for (size_t i = 0; i < addrs.size(); ++i) {
    addrs[i] = *Ipv4Addr::parse(std::format("192.168.1.{}", i + 1));
  }

  dns::CachePushTransaction transaction{.name = "example.com",
                                        .expire_at =
                                            SteadyClock::now() + std::chrono::seconds{3600},
                                        .ipv4s = addrs};

  auto push_result = cache.push(transaction);
  COROSIG_REQUIRE(!push_result.is_ok());
  COROSIG_REQUIRE(push_result.error() == AllocationError{});
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: handles long hostnames") {
  dns::MemoryCache<> cache{reactor.allocator()};

  std::string long_name = "a.very.long.hostname.that.goes.on.and.on.subdomain.example.com";
  std::array<Ipv4Addr, 1> addrs = {*Ipv4Addr::parse("192.168.1.1")};

  dns::CachePushTransaction transaction{.name = long_name,
                                        .expire_at =
                                            SteadyClock::now() + std::chrono::seconds{3600},
                                        .ipv4s = addrs};

  auto push_result = cache.push(transaction);
  COROSIG_REQUIRE(push_result.is_ok());

  std::array<Ipv4Addr, 4> out_addrs{};
  auto pull_result = cache.pull(long_name, out_addrs);
  COROSIG_REQUIRE(pull_result.is_ok());
  COROSIG_REQUIRE(pull_result.value() == 1);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("192.168.1.1"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: prune with multiple expired entries") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto now = SteadyClock::now();
  std::array<Ipv4Addr, 1> addrs = {*Ipv4Addr::parse("192.168.1.1")};

  dns::CachePushTransaction expired1{
      .name = "expired1.example.com", .expire_at = now - std::chrono::hours{2}, .ipv4s = addrs};

  dns::CachePushTransaction expired2{
      .name = "expired2.example.com", .expire_at = now - std::chrono::hours{1}, .ipv4s = addrs};

  dns::CachePushTransaction valid{
      .name = "valid.example.com", .expire_at = now + std::chrono::hours{1}, .ipv4s = addrs};

  COROSIG_REQUIRE(cache.push(expired1).is_ok());
  COROSIG_REQUIRE(cache.push(expired2).is_ok());
  COROSIG_REQUIRE(cache.push(valid).is_ok());

  cache.prune();

  std::array<Ipv4Addr, 4> out_addrs{};

  auto pull_expired1 = cache.pull("expired1.example.com", out_addrs);
  COROSIG_REQUIRE(pull_expired1.is_ok());
  COROSIG_REQUIRE(pull_expired1.value() == 0);

  auto pull_expired2 = cache.pull("expired2.example.com", out_addrs);
  COROSIG_REQUIRE(pull_expired2.is_ok());
  COROSIG_REQUIRE(pull_expired2.value() == 0);

  auto pull_valid = cache.pull("valid.example.com", out_addrs);
  COROSIG_REQUIRE(pull_valid.is_ok());
  COROSIG_REQUIRE(pull_valid.value() == 1);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("192.168.1.1"));
}

COROSIG_SIGHANDLER_TEST_CASE("MemoryCache: multiple pulls after prune") {
  dns::MemoryCache<> cache{reactor.allocator()};

  auto now = SteadyClock::now();

  std::array<Ipv4Addr, 1> expired_addr = {*Ipv4Addr::parse("192.168.1.1")};
  std::array<Ipv4Addr, 1> valid_addr = {*Ipv4Addr::parse("192.168.1.2")};

  dns::CachePushTransaction expired{.name = "test.example.com",
                                    .expire_at = now - std::chrono::seconds{3600},
                                    .ipv4s = expired_addr};

  dns::CachePushTransaction valid{.name = "test.example.com",
                                  .expire_at = now + std::chrono::seconds{3600},
                                  .ipv4s = valid_addr};

  COROSIG_REQUIRE(cache.push(expired).is_ok());
  COROSIG_REQUIRE(cache.push(valid).is_ok());

  std::array<Ipv4Addr, 4> out_addrs{};
  auto pull_before = cache.pull("test.example.com", out_addrs);
  COROSIG_REQUIRE(pull_before.is_ok());
  // expired addr is ignored even though not pruned yet
  COROSIG_REQUIRE(pull_before.value() == 1);

  cache.prune();

  auto pull_after = cache.pull("test.example.com", out_addrs);
  COROSIG_REQUIRE(pull_after.is_ok());
  COROSIG_REQUIRE(pull_after.value() == 1);
  COROSIG_REQUIRE(out_addrs[0] == *Ipv4Addr::parse("192.168.1.2"));
}
