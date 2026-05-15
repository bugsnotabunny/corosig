#ifndef COROSIG_IO_DNS_A_CACHE_HPP
#define COROSIG_IO_DNS_A_CACHE_HPP

#include "corosig/Clock.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/meta/AResult.hpp"
#include "corosig/meta/Futurize.hpp"

#include <algorithm>

namespace corosig::dns {

namespace detail {

constexpr char to_lower(char c) noexcept {
  if (c >= 'A' && c <= 'Z') {
    c += 32;
  }
  return c;
}

constexpr bool debug_is_ascii(std::string_view str) noexcept {
  return std::ranges::all_of(str, [](char c) { return (c & (1 << 8)) == 0; });
}

} // namespace detail

template <typename IP>
struct ResolvedAddress {
  IP address;
  SteadyClock::time_point expires_at;
};

struct CachePushTransaction {
  std::string_view name;
  SteadyClock::time_point expire_at;
  std::span<Ipv4Addr const> ipv4s = {};
  std::span<Ipv6Addr const> ipv6s = {};
};

template <typename T>
concept ACache = requires(T t) {
  // both methods should return result or future which resolve to size_t in case of success
  { t.pull(std::string_view{}, std::span<ResolvedAddress<IpvNAddr>>{}) } noexcept;
  { t.pull(std::string_view{}, std::span<ResolvedAddress<Ipv6Addr>>{}) } noexcept;
  { t.pull(std::string_view{}, std::span<ResolvedAddress<Ipv4Addr>>{}) } noexcept;
};

template <typename T>
concept AMutableCache = ACache<T> && requires(T t) {
  { t.push(CachePushTransaction{}) } noexcept -> AResult;
  { t.prune() } noexcept -> std::same_as<void>;
};

} // namespace corosig::dns

#endif
