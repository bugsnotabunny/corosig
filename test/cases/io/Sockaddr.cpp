#include "corosig/io/Sockaddr.hpp"

#include "corosig/testing/Signals.hpp"
#include "corosig/util/Endianness.hpp"

#include <arpa/inet.h>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <cstdint>
#include <cstring>

namespace {

using namespace corosig;

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Ipv4Addr from_bytes", "[ipv4]") {
  std::array<uint8_t, 4> bytes = {192, 168, 1, 1};
  auto addr = Ipv4Addr::from_bytes(bytes);

  auto expected = hton<uint32_t>((192 << 24) | (168 << 16) | (1 << 8) | 1);
  COROSIG_REQUIRE(addr.value() == expected);
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv4Addr parse valid addresses", "[ipv4]") {
  struct TestCase {
    std::string_view input;
    std::array<uint8_t, 4> expected;
  };

  auto tests = std::to_array<TestCase>({
      {.input = "192.168.1.1", .expected = {192, 168, 1, 1}},
      {.input = "0.0.0.0", .expected = {0, 0, 0, 0}},
      {.input = "255.255.255.255", .expected = {255, 255, 255, 255}},
      {.input = "127.0.0.1", .expected = {127, 0, 0, 1}},
      {.input = "10.0.0.1", .expected = {10, 0, 0, 1}},
      {.input = "1.2.3.4", .expected = {1, 2, 3, 4}},
      {.input = "001.002.003.004", .expected = {1, 2, 3, 4}},
  });

  for (const auto &test : tests) {
    auto result = Ipv4Addr::parse(test.input);
    COROSIG_REQUIRE(result.has_value());
    COROSIG_REQUIRE(result->value() == Ipv4Addr::from_bytes(test.expected).value());

    auto result2 = IpvNAddr::parse(test.input);
    COROSIG_REQUIRE(result2.has_value());
    COROSIG_REQUIRE(result2->as<Ipv4Addr>().value() == Ipv4Addr::from_bytes(test.expected).value());
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv4Addr parse invalid addresses", "[ipv4]") {
  auto invalid = std::to_array<std::string_view>({
      "",                // Empty
      "256.1.1.1",       // Value > 255
      "192.168.1",       // Missing octet
      "192.168.1.1.1",   // Too many octets
      "192.168.1.256",   // Last octet too large
      "192.168..1",      // Empty octet
      "192.168.1.1.1",   // Extra octet
      "abc.def.ghi.jkl", // Non-numeric
      "192.168.1.1a",    // Trailing characters
      "192.168.1. 1",    // Space
      ".192.168.1.1",    // Leading dot
      "192.168.1.1.",    // Trailing dot
      "999.999.999.999", // All too large
      "-1.0.0.0",        // Negative
  });

  for (auto input : invalid) {
    auto result = Ipv4Addr::parse(input);
    COROSIG_REQUIRE(!result.has_value());

    auto result2 = IpvNAddr::parse(input);
    COROSIG_REQUIRE(!result2.has_value());
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv4Addr to_sockaddr", "[ipv4]") {
  auto addr = Ipv4Addr::parse("192.168.1.1").value();
  uint16_t port = 8080;

  auto storage = addr.to_sockaddr(port);
  const auto *sockaddr = reinterpret_cast<const sockaddr_in *>(&storage.native_storage);

  COROSIG_REQUIRE(sockaddr->sin_family == AF_INET);
  COROSIG_REQUIRE(sockaddr->sin_port == port);

  // Compare address
  auto expected = hton<uint32_t>((192 << 24) | (168 << 16) | (1 << 8) | 1);
  COROSIG_REQUIRE(sockaddr->sin_addr.s_addr == expected);
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv6Addr from_bytes", "[ipv6]") {
  std::array<uint8_t, 16> bytes = {0};
  bytes[15] = 1; // ::1
  auto addr = Ipv6Addr::from_bytes(bytes);

  auto result = addr.value();
  COROSIG_REQUIRE(result == bytes);
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv6Addr parse_mapped_ipv4", "[ipv6]") {
  struct TestCase {
    std::string_view input;
    std::string_view expected_ipv4;
  };

  auto tests = std::to_array<TestCase>({
      {.input = "::ffff:192.168.1.1", .expected_ipv4 = "192.168.1.1"},
      {.input = "::ffff:10.0.0.1", .expected_ipv4 = "10.0.0.1"},
      {.input = "::ffff:127.0.0.1", .expected_ipv4 = "127.0.0.1"},
      {.input = "::FFFF:192.168.1.1", .expected_ipv4 = "192.168.1.1"}, // Uppercase
  });

  constexpr auto CHECK_MAPPED_IPV4 = [](Ipv6Addr ipv6, Ipv4Addr ipv4) {
    auto ipv6_bytes = ipv6.value();

    // Check the IPv4 part is embedded correctly
    uint32_t embedded_ipv4;
    std::memcpy(&embedded_ipv4, &ipv6_bytes[12], 4);
    COROSIG_REQUIRE(embedded_ipv4 == ipv4.value());

    // Check the prefix bytes
    COROSIG_REQUIRE(ipv6_bytes[10] == 0xFF);
    COROSIG_REQUIRE(ipv6_bytes[11] == 0xFF);
    COROSIG_REQUIRE(ipv6_bytes[0] == 0);
    COROSIG_REQUIRE(ipv6_bytes[1] == 0);
  };

  for (const auto &test : tests) {
    auto expected_ipv4 = Ipv4Addr::parse(test.expected_ipv4).value();

    auto result1 = Ipv6Addr::parse_mapped_ipv4(test.input);
    COROSIG_REQUIRE(result1.has_value());
    CHECK_MAPPED_IPV4(result1.value(), expected_ipv4);

    auto result2 = Ipv6Addr::parse(test.input);
    COROSIG_REQUIRE(result2.has_value());
    CHECK_MAPPED_IPV4(result2.value(), expected_ipv4);

    auto result3 = IpvNAddr::parse(test.input);
    COROSIG_REQUIRE(result3.has_value());
    CHECK_MAPPED_IPV4(result3.value().as<Ipv6Addr>(), expected_ipv4);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv6Addr parse_mapped_ipv4 invalid", "[ipv6]") {
  auto invalid = std::to_array<std::string_view>({
      "::ffff:256.1.1.1",     // Invalid IPv4
      "::ffff:192.168.1",     // Incomplete IPv4
      "::ffff:192.168.1.1.1", // Extra octet
      "::ffff:",              // No IPv4
      "::fffe:192.168.1.1",   // Wrong prefix
      "::ffff:192.168.1.1a",  // Trailing chars
  });

  for (auto input : invalid) {
    auto result = Ipv6Addr::parse_mapped_ipv4(input);
    COROSIG_REQUIRE(!result.has_value());
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv6Addr parse_regular valid", "[ipv6]") {
  struct TestCase {
    std::string_view input;
    Ipv6Addr expected;
  };

  auto tests = std::to_array<TestCase>({
      {
          .input = "::1",
          .expected = Ipv6Addr::from_groups({0, 0, 0, 0, 0, 0, 0, 0x00000001}),
      },
      {
          .input = "::",
          .expected = Ipv6Addr::from_groups({}),
      },
      {
          .input = "2001:db8::1",
          .expected = Ipv6Addr::from_groups({0x2001, 0xdb8, 0, 0, 0, 0, 0, 0x1}),
      },
      {
          .input = "2001:0db8:0000:0000:0000:0000:0000:0001",
          .expected = Ipv6Addr::from_groups({0x2001, 0xdb8, 0, 0, 0, 0, 0, 0x1}),
      },
      {
          .input = "fe80::1",
          .expected = Ipv6Addr::from_groups({0xfe80, 0, 0, 0, 0, 0, 0, 0x00000001}),
      },
      {
          .input = "::ffff:0:0",
          .expected = Ipv6Addr::from_groups({0, 0, 0, 0, 0, 0xffff, 0, 0}),
      },
      {
          .input = "2001:db8:85a3::8a2e:370:7334",
          .expected = Ipv6Addr::from_groups({0x2001, 0xdb8, 0x85a3, 0, 0, 0x8a2e, 0x370, 0x7334}),
      },
      {
          .input = "::1:2:3:4:5:6:7",
          .expected = Ipv6Addr::from_groups({0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7}),
      },
      {
          .input = "0:1:2:3:4:5:6::",
          .expected = Ipv6Addr::from_groups({0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x0}),
      },
      {
          .input = "1::",
          .expected = Ipv6Addr::from_groups({0x1, 0, 0, 0, 0, 0, 0, 0}),
      },
      {
          .input = "1:2:3:4:5:6:7:8",
          .expected = Ipv6Addr::from_groups({0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8}),
      },
  });

  for (const auto &test : tests) {
    auto result = Ipv6Addr::parse_regular(test.input);
    COROSIG_REQUIRE(result.has_value());
    COROSIG_REQUIRE(*result == test.expected);

    result = Ipv6Addr::parse(test.input);
    COROSIG_REQUIRE(result.has_value());
    COROSIG_REQUIRE(*result == test.expected);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv6Addr parse_regular invalid", "[ipv6]") {
  auto invalid = std::to_array<std::string_view>({
      "",                       // Empty
      ":::",                    // Triple colon
      "1::2::3",                // Double compression
      "g::1",                   // Invalid hex
      "2001:db8::1::1",         // Multiple compressors
      "2001:db8:1:2:3:4:5:6:7", // Too many groups
      "2001:db8",               // Too few groups without compression
      "::1::",                  // Invalid compression
      "1:2:3:4:5:6:7:",         // Trailing colon
      ":1:2:3:4:5:6:7",         // Leading colon
      "2001:db8::1:2:3:4:5:6",  // Too many groups with compression
      "12345::1",               // Group too large
  });

  for (auto input : invalid) {
    auto result = Ipv6Addr::parse_regular(input);
    COROSIG_REQUIRE(!result.has_value());

    result = Ipv6Addr::parse(input);
    COROSIG_REQUIRE(!result.has_value());
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Ipv6Addr to_sockaddr", "[ipv6]") {
  auto addr = Ipv6Addr::parse("2001:db8::1").value();
  uint16_t port = 8080;

  auto storage = addr.to_sockaddr(port);
  const auto *sockaddr = reinterpret_cast<const sockaddr_in6 *>(&storage.native_storage);

  COROSIG_REQUIRE(sockaddr->sin6_family == AF_INET6);
  COROSIG_REQUIRE(sockaddr->sin6_port == port);

  // Verify address by converting back
  std::array<char, INET6_ADDRSTRLEN> buffer;
  const char *str = inet_ntop(AF_INET6, &sockaddr->sin6_addr, buffer.data(), sizeof(buffer));
  COROSIG_REQUIRE(str != nullptr);
  COROSIG_REQUIRE(std::string(str) == "2001:db8::1");
}
