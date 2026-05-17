#include "corosig/io/dns/Resolver.hpp"

#include "corosig/io/Sockaddr.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <array>
#include <concepts>
#include <string_view>

namespace {

using namespace corosig;

constexpr std::array<uint8_t, 32> RAND_SEED{};

template <typename RESOLVER>
auto make_resolver(Reactor &r, dns::CachelessResolver base_resolver) noexcept {
  if constexpr (std::same_as<RESOLVER, dns::CachelessResolver>) {
    return base_resolver;
  } else {
    dns::Cache<> cache{r, "/etc/hosts", r.allocator()};
    return dns::Resolver<>{std::move(cache), std::move(base_resolver)};
  }
}

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: make creates valid resolver") {
  auto result = dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr());
  COROSIG_REQUIRE(result.is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: make fails on invalid socket") {
  SockaddrStorage invalid_addr;
  invalid_addr.native_storage.ss_family = AF_UNSPEC;

  auto result = dns::CachelessResolver::make(RAND_SEED, invalid_addr);

  COROSIG_REQUIRE(!result.is_ok());
}

template <typename RESOLVER>
static void test_resolve_ipv4(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto result = co_await resolver.resolve_name(r, dns_servers, "google.com", addrs);

    COROSIG_REQUIRE(result.is_ok());
    COROSIG_REQUIRE(result.value() > 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: resolve IPv4 address for google.com") {
  return test_resolve_ipv4<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: resolve IPv4 address for google.com") {
  return test_resolve_ipv4<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_resolve_ipv6(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(
                       RAND_SEED, Ipv6Addr::from_groups({0, 0, 0, 0, 0, 0, 0, 0}).to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv6Addr>, 4> addrs{};
    auto result = co_await resolver.resolve_name(r, dns_servers, "google.com", addrs);

    COROSIG_REQUIRE(result.is_ok());
    COROSIG_REQUIRE(result.value() > 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: resolve IPv6 address for google.com") {
  return test_resolve_ipv6<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: resolve IPv6 address for google.com") {
  return test_resolve_ipv6<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_resolve_name1_ipv4(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    auto result = co_await resolver.template resolve_name1<Ipv4Addr>(r, dns_servers, "google.com");

    COROSIG_REQUIRE(result.is_ok());

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: resolve_name1 for IPv4 address") {
  return test_resolve_name1_ipv4<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: resolve_name1 for IPv4 address") {
  return test_resolve_name1_ipv4<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_resolve_name1_ipv6(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(
                       RAND_SEED, Ipv6Addr::from_groups({0, 0, 0, 0, 0, 0, 0, 0}).to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    auto result = co_await resolver.template resolve_name1<Ipv6Addr>(r, dns_servers, "google.com");

    COROSIG_REQUIRE(result.is_ok());

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: resolve_name1 for IPv6 address") {
  return test_resolve_name1_ipv6<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: resolve_name1 for IPv6 address") {
  return test_resolve_name1_ipv6<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_resolve_multiple_servers(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 3> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
        Ipv4Addr::parse("1.1.1.1")->to_sockaddr(dns::STANDARD_PORT),
        Ipv4Addr::parse("8.8.4.4")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto result = co_await resolver.resolve_name(r, dns_servers, "google.com", addrs);

    COROSIG_REQUIRE(result.is_ok());
    COROSIG_REQUIRE(result.value() > 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: resolve multiple DNS servers") {
  return test_resolve_multiple_servers<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: resolve multiple DNS servers") {
  return test_resolve_multiple_servers<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_empty_dns_servers(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 0> dns_servers{};

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};

    auto result = co_await resolver.resolve_name(r, dns_servers, "google.com", addrs);

    COROSIG_REQUIRE(!result.is_ok());
    COROSIG_REQUIRE(result.error() ==
                    dns::ResolveError{dns::ResolveErrorCode::NO_SERVERS_PROVIDED});

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: resolve with empty DNS server list") {
  return test_empty_dns_servers<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: resolve with empty DNS server list") {
  return test_empty_dns_servers<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_empty_output_buffer(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 0> addrs{};

    auto result = co_await resolver.resolve_name(r, dns_servers, "google.com", addrs);

    COROSIG_REQUIRE(result.is_ok());
    COROSIG_REQUIRE(result.value() == 0);

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: resolve with empty output buffer") {
  return test_empty_output_buffer<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: resolve with empty output buffer") {
  return test_empty_output_buffer<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_multiple_sequential_resolves(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    auto domain_names = std::to_array<std::string_view>({
        "google.com",
        "github.com",
        "motherfuckingwebsite.com",
    });

    for (std::string_view name : domain_names) {
      COROSIG_REQUIRE(co_await resolver.template resolve_name1<Ipv4Addr>(r, dns_servers, name));
    }

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: multiple sequential resolves") {
  return test_multiple_sequential_resolves<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: multiple sequential resolves") {
  return test_multiple_sequential_resolves<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_non_existent_domain(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto result = co_await resolver.resolve_name(
        r, dns_servers, "this-domain-definitely-does-notexist-12345.invalid", addrs);

    COROSIG_REQUIRE(!result.is_ok());

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: handle non-existent domain") {
  return test_non_existent_domain<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: handle non-existent domain") {
  return test_non_existent_domain<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_move_constructor(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver1_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver1 = make_resolver<RESOLVER>(r, std::move(resolver1_base));

    auto resolver2 = std::move(resolver1);

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    auto result =
        co_await resolver2.template resolve_name1<Ipv4Addr>(r, dns_servers, "datatracker.ietf.org");

    COROSIG_REQUIRE(result.is_ok());

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: move constructor preserves functionality") {
  return test_move_constructor<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: move constructor preserves functionality") {
  return test_move_constructor<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_fills_output_buffer(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 10> addrs{};
    auto result = co_await resolver.resolve_name(r, dns_servers, "google.com", addrs);

    if (result.is_ok()) {
      auto count = result.value();
      COROSIG_REQUIRE(count <= addrs.size());
      COROSIG_REQUIRE(count > 0);
    }

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: fills output buffer up to capacity") {
  return test_fills_output_buffer<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: fills output buffer up to capacity") {
  return test_fills_output_buffer<dns::Resolver<>>(reactor);
}

template <typename RESOLVER>
static void test_ttl_correctly_set(Reactor &reactor) noexcept {
  auto test_coro =
      [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));
    auto resolver = make_resolver<RESOLVER>(r, std::move(resolver_base));

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    auto before_resolve = SteadyClock::now();

    auto result = co_await resolver.template resolve_name1<Ipv4Addr>(r, dns_servers, "github.com");

    auto after_resolve = SteadyClock::now();

    COROSIG_REQUIRE(result.is_ok());
    auto resolved = result.value();

    COROSIG_REQUIRE(resolved.expires_at >= before_resolve);
    COROSIG_REQUIRE(resolved.expires_at <= after_resolve + std::chrono::seconds{31536000});

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("CachelessResolver: TTL is correctly set") {
  return test_ttl_correctly_set<dns::CachelessResolver>(reactor);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver: TTL is correctly set") {
  return test_ttl_correctly_set<dns::Resolver<>>(reactor);
}
