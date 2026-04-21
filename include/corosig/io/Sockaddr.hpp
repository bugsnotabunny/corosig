#ifndef COROSIG_IO_SOCKADDR_HPP
#define COROSIG_IO_SOCKADDR_HPP

#include "corosig/util/Variant.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <sys/socket.h>

namespace corosig {

/// @brief Crossplatform storage type for any socket address
struct SockaddrStorage {
  sockaddr_storage native_storage = {};
};

/// @brief Ipv4 address type
struct Ipv4Addr {
  /// @brief Set Ipv4Addr from Ipv4 groups, where each group is in host byte order
  static Ipv4Addr from_groups(std::array<uint8_t, 4> groups) noexcept;

  /// @brief Set Ipv4Addr from bytes in network byte order
  static Ipv4Addr from_bytes(std::array<uint8_t, 4> value) noexcept;

  /// @brief Parse Ipv4Addr from string
  [[nodiscard]] static std::optional<Ipv4Addr> parse(std::string_view addr) noexcept;

  /// @brief Convert this to SockaddrStorage
  [[nodiscard]] SockaddrStorage to_sockaddr(uint16_t port = 0) const noexcept;

  /// @brief Get value for this Ipv4 address
  /// @note: Always network byte order. Call to hton is not required
  [[nodiscard]] uint32_t value() const noexcept;

  constexpr auto operator<=>(const Ipv4Addr &) const noexcept = default;

private:
  uint32_t m_value = 0;
};

/// @brief Ipv6 address type
struct Ipv6Addr {
  /// @brief Set Ipv6Addr from Ipv6 groups, where each group is in host byte order
  static Ipv6Addr from_groups(std::array<uint16_t, 8> groups) noexcept;

  /// @brief Set Ipv6Addr from bytes in network byte order
  static Ipv6Addr from_bytes(std::array<uint8_t, 16> value) noexcept;

  /// @brief Parse any Ipv6Addr from string
  [[nodiscard]] static std::optional<Ipv6Addr> parse(std::string_view addr) noexcept;

  /// @brief Parse Ipv6Addr from string. Do not consider Ipv6-mapped Ipv4 addresses
  [[nodiscard]] static std::optional<Ipv6Addr> parse_regular(std::string_view addr) noexcept;

  /// @brief Parse Ipv6Addr from string. Consider only Ipv6-mapped Ipv4 addresses
  [[nodiscard]] static std::optional<Ipv6Addr> parse_mapped_ipv4(std::string_view addr) noexcept;

  /// @brief Convert this to SockaddrStorage
  [[nodiscard]] SockaddrStorage to_sockaddr(uint16_t port = 0) const noexcept;

  /// @brief Get value for this Ipv6 address
  /// @note: Always network byte order. Call to hton is not required
  [[nodiscard]] std::array<uint8_t, 16> value() const noexcept;

  constexpr auto operator<=>(const Ipv6Addr &) const noexcept = default;

private:
  std::array<uint8_t, 16> m_value = {};
};

/// @brief Ipv4/Ipv6 address type
struct IpvNAddr : Variant<Ipv4Addr, Ipv6Addr> {
  /// @brief Parse any Ipv6 or Ipv4 addr from string
  [[nodiscard]] static std::optional<IpvNAddr> parse(std::string_view addr) noexcept;

  /// @brief Convert this to SockaddrStorage
  [[nodiscard]] SockaddrStorage to_sockaddr(uint16_t port = 0) const noexcept;

  constexpr auto operator<=>(const IpvNAddr &) const noexcept = default;
};

} // namespace corosig

#endif
