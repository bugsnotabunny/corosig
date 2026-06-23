#include "corosig/util/Endianness.hpp"

#include "corosig/testing/Signals.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <type_traits>

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr betoh with uint16_t") {
  constexpr uint16_t big_endian_value = 0x1234;
  constexpr uint16_t result = betoh(big_endian_value);

  // On little-endian systems, bytes are swapped
  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(result == 0x3412);
  } else {
    COROSIG_REQUIRE(result == big_endian_value);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr letoh with uint16_t") {
  constexpr uint16_t little_endian_value = 0x3412;
  constexpr uint16_t result = letoh(little_endian_value);

  // On little-endian systems, value stays the same
  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(result == little_endian_value);
  } else {
    COROSIG_REQUIRE(result == 0x1234);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr htole with uint16_t") {
  constexpr uint16_t host_value = 0x1234;
  constexpr uint16_t little_endian_result = htole(host_value);

  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(little_endian_result == host_value);
  } else {
    COROSIG_REQUIRE(little_endian_result == 0x3412);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr htobe with uint16_t") {
  constexpr uint16_t host_value = 0x1234;
  constexpr uint16_t big_endian_result = htobe(host_value);

  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(big_endian_result == 0x3412);
  } else {
    COROSIG_REQUIRE(big_endian_result == host_value);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr hton with uint16_t") {
  constexpr uint16_t host_value = 0x1234;
  constexpr uint16_t network_order = hton(host_value);

  // Network order is big-endian
  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(network_order == 0x3412);
  } else {
    COROSIG_REQUIRE(network_order == host_value);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr ntoh with uint16_t") {
  constexpr uint16_t network_order = 0x3412;
  constexpr uint16_t host_value = ntoh(network_order);

  // Network order is big-endian
  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(host_value == 0x1234);
  } else {
    COROSIG_REQUIRE(host_value == network_order);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: round-trip htobe and betoh") {
  constexpr uint16_t original = 0xABCD;
  constexpr uint16_t big_endian = htobe(original);
  constexpr uint16_t back_to_host = betoh(big_endian);

  COROSIG_REQUIRE(back_to_host == original);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: round-trip htole and letoh") {
  constexpr uint16_t original = 0xABCD;
  constexpr uint16_t little_endian = htole(original);
  constexpr uint16_t back_to_host = letoh(little_endian);

  COROSIG_REQUIRE(back_to_host == original);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: enum type conversion") {
  enum class TestEnum : uint32_t {
    Value1 = 0x12345678,
    Value2 = 0x9ABCDEF0,
  };

  constexpr TestEnum value = TestEnum::Value1;
  constexpr TestEnum result = betoh(value);

  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(std::underlying_type_t<TestEnum>(result) == 0x78563412);
  } else {
    COROSIG_REQUIRE(result == value);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: 8-bit values are unchanged") {
  constexpr uint8_t value = 0xAB;
  constexpr uint8_t result_be = betoh(value);
  constexpr uint8_t result_le = letoh(value);

  COROSIG_REQUIRE(result_be == value);
  COROSIG_REQUIRE(result_le == value);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: host to network and back is identity") {
  constexpr uint16_t original = 0xBEEF;
  constexpr uint16_t network = hton(original);
  constexpr uint16_t back = ntoh(network);

  COROSIG_REQUIRE(back == original);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: network to host and back is identity") {
  constexpr uint32_t network = 0xDEADBEEF;
  constexpr uint32_t host = ntoh(network);
  constexpr uint32_t back = hton(host);

  COROSIG_REQUIRE(back == network);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: enum round-trip conversion") {
  enum class Protocol : uint16_t {
    TCP = 0x1234,
    UDP = 0x5678,
  };

  constexpr Protocol original = Protocol::UDP;
  constexpr Protocol network_order = hton(original);
  constexpr Protocol back_to_host = ntoh(network_order);

  COROSIG_REQUIRE(back_to_host == original);
}
