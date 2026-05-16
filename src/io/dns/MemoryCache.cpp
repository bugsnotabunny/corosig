#include "corosig/io/dns/MemoryCache.hpp"

#include "corosig/Clock.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/io/dns/Protocol.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <ranges>
#include <span>
#include <string_view>

namespace {

using namespace corosig;

static_assert(dns::AMutableCache<dns::MemoryCache<>>);

} // namespace

namespace corosig::dns {

namespace detail {

std::string_view NameStorageHeader::name() const noexcept {
  return {reinterpret_cast<char const *>(this) + sizeof(NameStorageHeader), m_name_size};
}

NameStorageHeader::NameStorageHeader(std::string_view name_) noexcept
    : m_name_size{name_.size()} {
  std::ranges::copy(name_ | std::views::transform(to_lower),
                    reinterpret_cast<char *>(this) + sizeof(NameStorageHeader));
}

Ipv4Addr const *AddrStorageHeader::ipv4s_begin() const noexcept {
  return reinterpret_cast<Ipv4Addr const *>(reinterpret_cast<char const *>(this) +
                                            sizeof(AddrStorageHeader));
}

Ipv4Addr *AddrStorageHeader::ipv4s_begin() noexcept {
  return reinterpret_cast<Ipv4Addr *>(reinterpret_cast<char *>(this) + sizeof(AddrStorageHeader));
}

std::span<Ipv6Addr> AddrStorageHeader::ipv6s() noexcept {
  return {reinterpret_cast<Ipv6Addr *>(ipv4s_begin() + m_ipv4s_count), m_ipv6s_count};
}

std::span<Ipv6Addr const> AddrStorageHeader::ipv6s() const noexcept {
  return {reinterpret_cast<Ipv6Addr const *>(ipv4s_begin() + m_ipv4s_count), m_ipv6s_count};
}

std::span<Ipv4Addr> AddrStorageHeader::ipv4s() noexcept {
  return {ipv4s_begin(), m_ipv4s_count};
}

std::span<Ipv4Addr const> AddrStorageHeader::ipv4s() const noexcept {
  return {ipv4s_begin(), m_ipv4s_count};
}

SteadyClock::time_point AddrStorageHeader::expires_at() const noexcept {
  return m_expires_at;
}

template <typename IP, typename IP2>
AlwaysOkResult<size_t> memory_cache_pull_impl(
    std::add_pointer_t<std::span<IP2 const>(AddrStorageHeader const &)> ip_source,
    memory_cache_names const &names,
    std::string_view ascii_name,
    std::span<ResolvedAddress<IP>> out) noexcept {
  assert(dns::detail::debug_is_ascii(ascii_name));
  if (ascii_name.size() > dns::detail::FQDN_MAX_OCTET_LEN) {
    return size_t(0);
  }

  std::array<char, FQDN_MAX_OCTET_LEN> ascii_name_lowercase_buf;
  char const *end =
      std::ranges::transform(ascii_name, ascii_name_lowercase_buf.begin(), dns::detail::to_lower)
          .out;

  std::string_view lowercase_ascii_name = {ascii_name_lowercase_buf.begin(), end};

  auto name_it = names.find(lowercase_ascii_name, std::less<>{});
  if (name_it == names.end()) {
    return size_t(0);
  }

  size_t pulled = 0;
  auto now = SteadyClock::now();
  for (detail::AddrStorageHeader const &addr_storage_header : name_it->ips) {
    if (out.empty()) {
      break;
    }

    if (addr_storage_header.expires_at() <= now) {
      continue;
    }

    std::span ips = ip_source(addr_storage_header);
    ips = ips.subspan(0, std::min(out.size(), ips.size()));
    for (IP2 const &ip : ips) {
      out.front() = ResolvedAddress<IP>{
          .address = IP{ip},
          .expires_at = addr_storage_header.expires_at(),
      };
      out = out.subspan(1);
    }

    pulled += ips.size();
  }
  return pulled;
}

NameStorageHeader &AddrStorageHeader::parent() const noexcept {
  return m_parent;
}

template AlwaysOkResult<size_t> memory_cache_pull_impl<Ipv6Addr, Ipv6Addr>(
    std::add_pointer_t<std::span<Ipv6Addr const>(AddrStorageHeader const &)>,
    memory_cache_names const &,
    std::string_view,
    std::span<ResolvedAddress<Ipv6Addr>>) noexcept;

template AlwaysOkResult<size_t> memory_cache_pull_impl<Ipv4Addr, Ipv4Addr>(
    std::add_pointer_t<std::span<Ipv4Addr const>(AddrStorageHeader const &)>,
    memory_cache_names const &,
    std::string_view,
    std::span<ResolvedAddress<Ipv4Addr>>) noexcept;

template AlwaysOkResult<size_t> memory_cache_pull_impl<IpvNAddr, Ipv4Addr>(
    std::add_pointer_t<std::span<Ipv4Addr const>(AddrStorageHeader const &)>,
    memory_cache_names const &,
    std::string_view,
    std::span<ResolvedAddress<IpvNAddr>>) noexcept;

template AlwaysOkResult<size_t> memory_cache_pull_impl<IpvNAddr, Ipv6Addr>(
    std::add_pointer_t<std::span<Ipv6Addr const>(AddrStorageHeader const &)>,
    memory_cache_names const &,
    std::string_view,
    std::span<ResolvedAddress<IpvNAddr>>) noexcept;

} // namespace detail

template struct MemoryCache<AllocatorRef<Allocator>>;

} // namespace corosig::dns
