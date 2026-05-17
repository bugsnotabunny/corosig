#include "corosig/io/dns/Resolver.hpp"

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <array>
#include <concepts>
#include <functional>

namespace {

using namespace corosig;

constexpr std::array<uint8_t, 32> RAND_SEED{};

struct MockCache {
  MockCache(Reactor &r) noexcept
      : m_expected_pulls{r.allocator()},
        m_reactor{r} {
  }

  MockCache(MockCache const &) = delete;
  MockCache &operator=(MockCache const &) = delete;
  MockCache(MockCache &&) = delete;
  MockCache &operator=(MockCache &&) = delete;

  ~MockCache() = default;

  struct PullCall {
    std::string_view name;
    size_t out_size;
  };

  struct PushCall {
    std::string_view name;
    size_t addrs_size;
  };

  struct PullResult {
    PullResult(Allocator &alloc)
        : ipv4_results{alloc},
          ipv6_results{alloc} {
    }

    size_t count = 0;
    bool should_fail = false;
    Vector<dns::ResolvedAddress<Ipv4Addr>> ipv4_results;
    Vector<dns::ResolvedAddress<Ipv6Addr>> ipv6_results;
  };

  void expect_pull(std::string_view name, PullResult result) noexcept {
    COROSIG_REQUIRE(m_expected_pulls.push_back({name, std::move(result)}));
  }

  void verify() const noexcept {
    COROSIG_REQUIRE(m_pull_count >= m_expected_pulls.size());
  }

  void reset() noexcept {
    m_pull_count = 0;
    m_last_pull_call = std::nullopt;
  }

  [[nodiscard]] std::optional<PullCall> last_pull_call() const noexcept {
    return m_last_pull_call;
  }

  [[nodiscard]] size_t pull_count() const noexcept {
    return m_pull_count;
  }

  [[nodiscard]] size_t push_count() const noexcept {
    return m_push_count;
  }

  [[nodiscard]] size_t prune_count() const noexcept {
    return m_prune_count;
  }

  [[nodiscard]] std::optional<PushCall> last_push_call() const noexcept {
    return m_last_push_call;
  }

  Fut<size_t, Error<AllocationError, SyscallError>>
  pull(std::string_view ascii_name, std::span<dns::ResolvedAddress<Ipv4Addr>> out) noexcept {
    return pull_impl(m_reactor, ascii_name, out);
  }

  Fut<size_t, Error<AllocationError, SyscallError>>
  pull(std::string_view ascii_name, std::span<dns::ResolvedAddress<Ipv6Addr>> out) noexcept {
    return pull_impl(m_reactor, ascii_name, out);
  }

  Result<void, AllocationError>
  push(std::string_view ascii_name,
       std::span<dns::ResolvedAddress<Ipv4Addr> const> addrs) noexcept {
    return push_impl(ascii_name, addrs);
  }

  Result<void, AllocationError>
  push(std::string_view ascii_name,
       std::span<dns::ResolvedAddress<Ipv6Addr> const> addrs) noexcept {
    return push_impl(ascii_name, addrs);
  }

  void prune() noexcept {
    m_prune_count++;
  }

  void pop_expected_pull() noexcept {
    m_expected_pulls.erase(m_expected_pulls.begin());
  }

private:
  template <typename IP>
  Fut<size_t, Error<AllocationError, SyscallError>> pull_impl(
      Reactor &, std::string_view ascii_name, std::span<dns::ResolvedAddress<IP>> out) noexcept {
    m_pull_count++;
    m_last_pull_call.emplace(ascii_name, out.size());

    if (m_expected_pulls.empty()) {
      co_return 0;
    }

    auto const &[name, result] = m_expected_pulls.front();
    if (result.should_fail) {
      co_return Failure{AllocationError{}};
    }

    if constexpr (std::same_as<IP, Ipv6Addr>) {
      auto count = std::min(result.ipv6_results.size(), out.size());
      for (size_t i = 0; i < count; ++i) {
        out[i] = result.ipv6_results[i];
      }
      co_return count;
    } else if constexpr (std::same_as<IP, Ipv4Addr>) {
      auto count = std::min(result.ipv4_results.size(), out.size());
      for (size_t i = 0; i < count; ++i) {
        out[i] = result.ipv4_results[i];
      }
      co_return count;
    } else {
      static_assert(false);
    }
  }

  template <typename IP>
  Result<void, AllocationError>
  push_impl(std::string_view ascii_name, std::span<dns::ResolvedAddress<IP> const> addrs) noexcept {
    m_push_count++;
    m_last_push_call.emplace(ascii_name, addrs.size());
    return Ok{};
  }

  struct ExpectedPull {
    std::string_view name;
    PullResult result;
  };

  size_t m_pull_count = 0;
  size_t m_push_count = 0;
  size_t m_prune_count = 0;
  std::optional<PullCall> m_last_pull_call;
  std::optional<PushCall> m_last_push_call;
  Vector<ExpectedPull> m_expected_pulls;
  std::reference_wrapper<Reactor> m_reactor;
};

struct MockCacheRef {
  MockCacheRef(MockCache &r) noexcept
      : ref{r} {
  }

  Fut<size_t, Error<AllocationError, SyscallError>>
  pull(std::string_view ascii_name, std::span<dns::ResolvedAddress<Ipv4Addr>> out) noexcept {
    return ref.get().pull(ascii_name, out);
  }

  Fut<size_t, Error<AllocationError, SyscallError>>
  pull(std::string_view ascii_name, std::span<dns::ResolvedAddress<Ipv6Addr>> out) noexcept {
    return ref.get().pull(ascii_name, out);
  }

  Result<void, AllocationError>
  push(std::string_view ascii_name,
       std::span<dns::ResolvedAddress<Ipv4Addr> const> addrs) noexcept {
    return ref.get().push(ascii_name, addrs);
  }

  Result<void, AllocationError>
  push(std::string_view ascii_name,
       std::span<dns::ResolvedAddress<Ipv6Addr> const> addrs) noexcept {
    return ref.get().push(ascii_name, addrs);
  }

  void prune() noexcept {
    return ref.get().prune();
  }

  std::reference_wrapper<MockCache> ref;
};

static_assert(dns::ACache<MockCache>);
static_assert(dns::AMutableCache<MockCache>);

static_assert(dns::ACache<MockCacheRef>);
static_assert(dns::AMutableCache<MockCacheRef>);

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Resolver<MockCache>: cache pull is called on resolve_name") {
  MockCache cache{reactor};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
  MockCache::PullResult result{reactor.allocator()};
  result.count = 1;
  COROSIG_REQUIRE(result.ipv4_results.push_back(dns::ResolvedAddress<Ipv4Addr>{
      .address = Ipv4Addr::parse("192.168.1.100").value(), .expires_at = expires_at}));
  cache.expect_pull("example.com", std::move(result));

  auto test_coro =
      [](MockCache &c,
         Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));

    dns::Resolver<MockCacheRef> resolver{c, std::move(resolver_base)};

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto resolve_result = co_await resolver.resolve_name(r, dns_servers, "example.com", addrs);

    COROSIG_REQUIRE(resolve_result.is_ok());
    COROSIG_REQUIRE(resolve_result.value() == 1);

    COROSIG_REQUIRE(c.pull_count() == 1);
    COROSIG_REQUIRE(c.prune_count() == 1);
    COROSIG_REQUIRE(c.push_count() == 0);

    auto const &calls = c.last_pull_call();
    COROSIG_REQUIRE(calls.has_value());
    COROSIG_REQUIRE(calls->name == "example.com");
    COROSIG_REQUIRE(calls->out_size == 4);

    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(cache, reactor).block_on().is_ok());
  cache.verify();
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver<MockCache>: cache hit returns cached value") {
  MockCache cache{reactor};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
  MockCache::PullResult result{reactor.allocator()};
  result.count = 2;
  COROSIG_REQUIRE(result.ipv4_results.push_back(dns::ResolvedAddress<Ipv4Addr>{
      .address = Ipv4Addr::parse("1.2.3.4").value(), .expires_at = expires_at}));
  COROSIG_REQUIRE(result.ipv4_results.push_back(dns::ResolvedAddress<Ipv4Addr>{
      .address = Ipv4Addr::parse("5.6.7.8").value(), .expires_at = expires_at}));
  cache.expect_pull("cached.com", std::move(result));

  auto test_coro =
      [](MockCache &c,
         Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));

    dns::Resolver<MockCacheRef> resolver{c, std::move(resolver_base)};

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto resolve_result = co_await resolver.resolve_name(r, dns_servers, "cached.com", addrs);

    COROSIG_REQUIRE(resolve_result.is_ok());
    COROSIG_REQUIRE(resolve_result.value() == 2);
    COROSIG_REQUIRE(addrs[0].address == Ipv4Addr::parse("1.2.3.4").value());
    COROSIG_REQUIRE(addrs[1].address == Ipv4Addr::parse("5.6.7.8").value());
    COROSIG_REQUIRE(c.prune_count() == 1);
    COROSIG_REQUIRE(c.push_count() == 0);
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(cache, reactor).block_on().is_ok());
  cache.verify();
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver<MockCache>: cache miss falls through") {
  MockCache cache{reactor};

  auto test_coro =
      [](MockCache &c,
         Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));

    dns::Resolver<MockCacheRef> resolver{c, std::move(resolver_base)};

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto resolve_result = co_await resolver.resolve_name(r, dns_servers, "uncached.com", addrs);

    COROSIG_REQUIRE(resolve_result.is_ok());
    COROSIG_REQUIRE(resolve_result.value() > 0);

    COROSIG_REQUIRE(c.pull_count() == 1);
    COROSIG_REQUIRE(c.prune_count() == 1);
    COROSIG_REQUIRE(c.push_count() == 1);

    auto const &calls = c.last_pull_call();
    COROSIG_REQUIRE(calls.has_value());
    COROSIG_REQUIRE(calls->name == "uncached.com");

    auto const &push_call = c.last_push_call();
    COROSIG_REQUIRE(push_call.has_value());
    COROSIG_REQUIRE(push_call->name == "uncached.com");
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(cache, reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver<MockCache>: cache error falls through") {
  MockCache cache{reactor};

  MockCache::PullResult fail_result{reactor.allocator()};
  fail_result.should_fail = true;
  cache.expect_pull("error.com", std::move(fail_result));
  cache.pop_expected_pull();

  auto test_coro =
      [](MockCache &c,
         Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));

    dns::Resolver<MockCacheRef> resolver{c, std::move(resolver_base)};

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv4Addr>, 4> addrs{};
    auto resolve_result = co_await resolver.resolve_name(r, dns_servers, "error.com", addrs);

    COROSIG_REQUIRE(resolve_result.is_ok());
    COROSIG_REQUIRE(resolve_result.value() > 0);

    COROSIG_REQUIRE(c.pull_count() == 1);
    COROSIG_REQUIRE(c.prune_count() == 1);
    COROSIG_REQUIRE(c.push_count() == 1);

    auto const &calls = c.last_pull_call();
    COROSIG_REQUIRE(calls.has_value());
    COROSIG_REQUIRE(calls->name == "error.com");

    auto const &push_call = c.last_push_call();
    COROSIG_REQUIRE(push_call.has_value());
    COROSIG_REQUIRE(push_call->name == "error.com");
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(cache, reactor).block_on().is_ok());
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver<MockCache>: multiple sequential calls") {
  MockCache cache{reactor};

  auto test_coro =
      [](MockCache &c,
         Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(RAND_SEED, Ipv4Addr{}.to_sockaddr()));

    dns::Resolver<MockCacheRef> resolver{c, std::move(resolver_base)};

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<std::string_view, 3> domain_names{
        "google.com", "github.com", "motherfuckingwebsite.com"};

    for (std::string_view name : domain_names) {
      std::array<dns::ResolvedAddress<Ipv4Addr>, 2> addrs{};
      COROSIG_REQUIRE(co_await resolver.resolve_name(r, dns_servers, name, addrs));
    }
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(cache, reactor).block_on().is_ok());
  COROSIG_REQUIRE(cache.pull_count() >= 3);
  COROSIG_REQUIRE(cache.prune_count() == 3);
  COROSIG_REQUIRE(cache.push_count() >= 3);
}

COROSIG_SIGHANDLER_TEST_CASE("Resolver<MockCache>: IPv6 resolve works") {
  MockCache cache{reactor};

  auto expires_at = SteadyClock::now() + std::chrono::seconds{3600};
  MockCache::PullResult result{reactor.allocator()};
  result.count = 1;
  COROSIG_REQUIRE(result.ipv6_results.push_back(dns::ResolvedAddress<Ipv6Addr>{
      .address = Ipv6Addr::parse("2001:db8::1").value(), .expires_at = expires_at}));
  cache.expect_pull("ipv6.com", std::move(result));

  auto test_coro =
      [](MockCache &c,
         Reactor &r) -> Fut<void, Error<AllocationError, SyscallError, dns::ResolveError>> {
    COROSIG_CO_TRY(auto resolver_base,
                   dns::CachelessResolver::make(
                       RAND_SEED, Ipv6Addr::from_groups({0, 0, 0, 0, 0, 0, 0, 0}).to_sockaddr()));

    dns::Resolver<MockCacheRef> resolver{c, std::move(resolver_base)};

    std::array<SockaddrStorage, 1> dns_servers{
        Ipv4Addr::parse("8.8.8.8")->to_sockaddr(dns::STANDARD_PORT),
    };

    std::array<dns::ResolvedAddress<Ipv6Addr>, 4> addrs{};
    auto resolve_result = co_await resolver.resolve_name(r, dns_servers, "ipv6.com", addrs);

    COROSIG_REQUIRE(resolve_result.is_ok());
    COROSIG_REQUIRE(resolve_result.value() == 1);

    COROSIG_REQUIRE(c.pull_count() == 1);
    COROSIG_REQUIRE(c.prune_count() == 1);
    COROSIG_REQUIRE(c.push_count() == 0);
    co_return Ok{};
  };
  COROSIG_REQUIRE(test_coro(cache, reactor).block_on().is_ok());
  cache.verify();
}
