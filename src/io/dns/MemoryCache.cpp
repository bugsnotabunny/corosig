#include "corosig/io/dns/MemoryCache.hpp"

#include "corosig/Clock.hpp"
#include "corosig/io/Sockaddr.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <string_view>

namespace corosig::dns {

namespace detail {

std::string_view NameStorageHeader::name() const noexcept {
  return {reinterpret_cast<char const *>(this) + sizeof(NameStorageHeader), m_name_size};
}

NameStorageHeader::NameStorageHeader(std::string_view name_) noexcept
    : m_name_size{name_.size()} {
  std::memcpy(
      reinterpret_cast<char *>(this) + sizeof(NameStorageHeader), name_.data(), name_.size());
}

Ipv4Addr const *AddrStorageHeader::ipv4s_begin() const noexcept {
  return reinterpret_cast<Ipv4Addr const *>(reinterpret_cast<char const *>(this) +
                                            sizeof(AddrStorageHeader));
}

Ipv4Addr *AddrStorageHeader::ipv4s_begin() noexcept {
  return reinterpret_cast<Ipv4Addr *>(reinterpret_cast<char *>(this) + sizeof(AddrStorageHeader));
}

AddrStorageHeader::AddrStorageHeader(NameStorageHeader &parent,
                                     SteadyClock::time_point expires_at,
                                     std::span<Ipv4Addr const> ipv4s_list,
                                     std::span<Ipv6Addr const> ipv6s_list) noexcept
    : m_parent{parent},
      m_expires_at{expires_at},
      m_ipv4s_count{uint32_t(ipv4s_list.size())},
      m_ipv6s_count{uint32_t(ipv6s_list.size())} {
  assert(ipv4s_list.size() <= std::numeric_limits<uint32_t>::max());
  assert(ipv6s_list.size() <= std::numeric_limits<uint32_t>::max());
  std::ranges::copy(ipv4s_list, ipv4s().begin());
  std::ranges::copy(ipv6s_list, ipv6s().begin());
  parent.ips.push_front(*this);
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

template <typename IP>
AlwaysOkResult<size_t>
memory_cache_pull_impl(std::add_pointer_t<std::span<IP const>(AddrStorageHeader const &)> ip_source,
                       memory_cache_names const &names,
                       std::string_view ascii_name,
                       std::span<IP> out) noexcept {

  auto name_it = names.find(ascii_name, std::less<>{});
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
    std::ranges::copy(ips, out.begin());
    out = out.subspan(ips.size());
    pulled += ips.size();
  }
  return pulled;
}

NameStorageHeader &AddrStorageHeader::parent() const noexcept {
  return m_parent;
}

template AlwaysOkResult<size_t> memory_cache_pull_impl<Ipv6Addr>(
    std::add_pointer_t<std::span<Ipv6Addr const>(AddrStorageHeader const &)>,
    memory_cache_names const &,
    std::string_view,
    std::span<Ipv6Addr>) noexcept;

template AlwaysOkResult<size_t> memory_cache_pull_impl<Ipv4Addr>(
    std::add_pointer_t<std::span<Ipv4Addr const>(AddrStorageHeader const &)>,
    memory_cache_names const &,
    std::string_view,
    std::span<Ipv4Addr>) noexcept;

} // namespace detail

template struct MemoryCache<AllocatorRef<Allocator>>;

} // namespace corosig::dns
