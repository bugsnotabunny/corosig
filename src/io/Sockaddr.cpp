#include "corosig/io/Sockaddr.hpp"

#include "corosig/util/Endianness.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <ranges>
#include <string_view>
#include <sys/socket.h>

namespace {

using namespace corosig;

std::optional<uint8_t> hex_char_to_num(char c) noexcept {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return std::nullopt;
};

struct ParseIpv6GroupOk {
  uint16_t group;
  std::string_view addr_without_group;
};

std::optional<ParseIpv6GroupOk> parse_ipv6_group(std::string_view group_str) noexcept {
  uint16_t group = 0;
  for (size_t i = 0; i < 4 && !group_str.empty(); ++i) {
    std::optional add_value = hex_char_to_num(group_str.front());
    if (!add_value) {
      if (i == 0) {
        return std::nullopt;
      }
      break;
    }
    group *= 16;
    group += *add_value;
    group_str.remove_prefix(1);
  }
  return ParseIpv6GroupOk{.group = group, .addr_without_group = group_str};
}

constexpr std::string_view IPV6_COMPRESSOR = "::";

} // namespace

namespace corosig {

Ipv4Addr from_groups(std::array<uint8_t, 4> groups) noexcept {
  return Ipv4Addr::from_bytes(groups);
}

Ipv4Addr Ipv4Addr::from_bytes(std::array<uint8_t, 4> value) noexcept {
  Ipv4Addr result;
  result.m_value = std::bit_cast<uint32_t>(value);
  return result;
}

uint32_t Ipv4Addr::value() const noexcept {
  return m_value;
}

[[nodiscard]] SockaddrStorage Ipv4Addr::to_sockaddr(uint16_t port) const noexcept {
  SockaddrStorage sockaddr;
  auto *result = reinterpret_cast<sockaddr_in *>(&sockaddr.native_storage);
  result->sin_family = AF_INET;
  result->sin_port = port;
  result->sin_addr.s_addr = m_value;
  return sockaddr;
}

std::optional<Ipv4Addr> Ipv4Addr::parse(std::string_view addr) noexcept {
  std::array<uint8_t, 4> result;
  size_t i = 0;
  for (auto group : addr | std::views::split('.')) {
    if (i == 4) {
      return std::nullopt;
    }
    if (group.size() < 1 || group.size() > 3) {
      return std::nullopt;
    }

    uint16_t group_result = 0;
    for (char c : group) {
      if (c < '0' || c > '9') {
        return std::nullopt;
      }

      group_result *= 10;
      group_result += c - '0';
    }

    if (group_result > 255) {
      return std::nullopt;
    }

    result[i] = group_result;
    ++i;
  }
  if (i != 4) {
    return std::nullopt;
  }
  return Ipv4Addr::from_bytes(result);
}

Ipv6Addr Ipv6Addr::from_groups(std::array<uint16_t, 8> groups) noexcept {
  std::array<uint8_t, 16> network_bytes;
  for (size_t i = 0; i < groups.size(); ++i) {
    network_bytes[i * 2] = (groups[i] >> 8) & 0xFF;
    network_bytes[i * 2 + 1] = groups[i] & 0xFF;
  }
  return Ipv6Addr::from_bytes(network_bytes);
}

Ipv6Addr Ipv6Addr::from_bytes(std::array<uint8_t, 16> value) noexcept {
  Ipv6Addr result;
  result.m_value = value;
  return result;
}

std::array<uint8_t, 16> Ipv6Addr::value() const noexcept {
  return m_value;
}

[[nodiscard]] SockaddrStorage Ipv6Addr::to_sockaddr(uint16_t port) const noexcept {
  SockaddrStorage sockaddr;
  auto *result = reinterpret_cast<sockaddr_in6 *>(&sockaddr.native_storage);
  result->sin6_family = AF_INET6;
  result->sin6_port = port;
  std::memcpy(&result->sin6_addr, &m_value, sizeof(m_value));
  return sockaddr;
}

std::optional<Ipv6Addr> Ipv6Addr::parse_mapped_ipv4(std::string_view addr) noexcept {
  if (addr.size() < IPV6_COMPRESSOR.size() + 5 || !addr.starts_with(IPV6_COMPRESSOR)) {
    return std::nullopt;
  }
  addr.remove_prefix(IPV6_COMPRESSOR.size());

  auto substr = addr;
  substr.remove_suffix(addr.size() - 4);
  for (char c : substr) {
    if (c != 'f' && c != 'F') {
      return std::nullopt;
    }
  }
  addr.remove_prefix(4);
  if (!addr.starts_with(':')) {
    return std::nullopt;
  }
  addr.remove_prefix(1);

  auto ipv4 = Ipv4Addr::parse(addr);
  if (!ipv4) {
    return std::nullopt;
  }

  uint32_t ipv4_value = ipv4->value();
  Ipv6Addr result;

  result.m_value[10] = uint8_t(0xFF);
  result.m_value[11] = uint8_t(0xFF);

  static_assert(sizeof(ipv4_value) % sizeof(uint8_t) == 0);
  std::ranges::copy(std::span{reinterpret_cast<uint8_t const *>(&ipv4_value),
                              sizeof(ipv4_value) / sizeof(uint8_t)},
                    result.m_value.begin() + 12);

  return result;
}

std::optional<Ipv6Addr> Ipv6Addr::parse_regular(std::string_view addr) noexcept {
  std::array<uint16_t, 8> expansion_buf;
  bool has_met_compressor = false;
  size_t front_groups = 0;

  if (addr.starts_with(IPV6_COMPRESSOR)) {
    has_met_compressor = true;
    addr.remove_prefix(1);
  } else {
    while (!addr.empty() && front_groups < expansion_buf.size()) {
      auto res = parse_ipv6_group(addr);
      if (!res) {
        return std::nullopt;
      }

      expansion_buf[front_groups] = res->group;
      addr = res->addr_without_group;
      ++front_groups;

      if (addr.starts_with(IPV6_COMPRESSOR)) {
        has_met_compressor = true;
        addr.remove_prefix(1);
        break;
      }

      addr.remove_prefix(std::min(size_t(1), addr.size()));
    }
  }

  size_t back_groups = 0;
  if (has_met_compressor) {
    constexpr size_t MINIMAL_ZERO_GROUPS_IN_COMPRESSOR = 1;

    while (!addr.empty()) {
      if (back_groups + front_groups + MINIMAL_ZERO_GROUPS_IN_COMPRESSOR > expansion_buf.size() ||
          !addr.starts_with(':') || addr.starts_with(IPV6_COMPRESSOR)) {
        return std::nullopt;
      }
      addr.remove_prefix(1);
      if (addr.empty()) {
        break;
      }
      auto res = parse_ipv6_group(addr);
      if (!res) {
        return std::nullopt;
      }
      expansion_buf[front_groups + back_groups] = res->group;
      addr = res->addr_without_group;
      ++back_groups;
    }

    size_t expanded_zero_groups = expansion_buf.size() - front_groups - back_groups;
    std::shift_right(
        expansion_buf.begin() + front_groups, expansion_buf.end(), ptrdiff_t(expanded_zero_groups));
    std::fill_n(expansion_buf.begin() + front_groups, expanded_zero_groups, 0);

    if (front_groups + back_groups >= expansion_buf.size()) {

      return std::nullopt;
    }
  }
  if ((!has_met_compressor && front_groups != expansion_buf.size()) || !addr.empty()) {
    return std::nullopt;
  }

  return Ipv6Addr::from_groups(expansion_buf);
}

std::optional<Ipv6Addr> Ipv6Addr::parse(std::string_view addr) noexcept {
  if (std::optional result = Ipv6Addr::parse_regular(addr)) {
    return result;
  }
  return Ipv6Addr::parse_mapped_ipv4(addr);
}

std::optional<IpvNAddr> IpvNAddr::parse(std::string_view addr) noexcept {
  if (std::optional result = Ipv4Addr::parse(addr)) {
    return IpvNAddr{*result};
  }
  if (std::optional result = Ipv6Addr::parse(addr)) {
    return IpvNAddr{*result};
  }
  return std::nullopt;
}

SockaddrStorage IpvNAddr::to_sockaddr(uint16_t port) const noexcept {
  return visit([&](auto const &addr) { return addr.to_sockaddr(port); });
}

} // namespace corosig
