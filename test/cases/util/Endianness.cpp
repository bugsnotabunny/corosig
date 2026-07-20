#include "corosig/util/Endianness.hpp"

#include "corosig/testing/Signals.hpp"

#include <cstdint>
#include <type_traits>

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr betoh with uint16_t") {
  constexpr uint16_t BIG_ENDIAN_VALUE = 0x1234;
  constexpr uint16_t RESULT = betoh(BIG_ENDIAN_VALUE);

  // On little-endian systems, bytes are swapped
  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(RESULT == 0x3412);
  } else {
    COROSIG_REQUIRE(RESULT == BIG_ENDIAN_VALUE);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr letoh with uint16_t") {
  constexpr uint16_t LITTLE_ENDIAN_VALUE = 0x3412;
  constexpr uint16_t RESULT = letoh(LITTLE_ENDIAN_VALUE);

  // On little-endian systems, value stays the same
  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(RESULT == LITTLE_ENDIAN_VALUE);
  } else {
    COROSIG_REQUIRE(RESULT == 0x1234);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr htole with uint16_t") {
  constexpr uint16_t HOST_VALUE = 0x1234;
  constexpr uint16_t LITTLE_ENDIAN_RESULT = htole(HOST_VALUE);

  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(LITTLE_ENDIAN_RESULT == HOST_VALUE);
  } else {
    COROSIG_REQUIRE(LITTLE_ENDIAN_RESULT == 0x3412);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr htobe with uint16_t") {
  constexpr uint16_t HOST_VALUE = 0x1234;
  constexpr uint16_t BIG_ENDIAN_RESULT = htobe(HOST_VALUE);

  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(BIG_ENDIAN_RESULT == 0x3412);
  } else {
    COROSIG_REQUIRE(BIG_ENDIAN_RESULT == HOST_VALUE);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr hton with uint16_t") {
  constexpr uint16_t HOST_VALUE = 0x1234;
  constexpr uint16_t NETWORK_ORDER = hton(HOST_VALUE);

  // Network order is big-endian
  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(NETWORK_ORDER == 0x3412);
  } else {
    COROSIG_REQUIRE(NETWORK_ORDER == HOST_VALUE);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: constexpr ntoh with uint16_t") {
  constexpr uint16_t NETWORK_ORDER = 0x3412;
  constexpr uint16_t HOST_VALUE = ntoh(NETWORK_ORDER);

  // Network order is big-endian
  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(HOST_VALUE == 0x1234);
  } else {
    COROSIG_REQUIRE(HOST_VALUE == NETWORK_ORDER);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: round-trip htobe and betoh") {
  constexpr uint16_t ORIGINAL = 0xABCD;
  constexpr uint16_t BIG_ENDIAN1 = htobe(ORIGINAL);
  constexpr uint16_t BACK_TO_HOST = betoh(BIG_ENDIAN1);

  COROSIG_REQUIRE(BACK_TO_HOST == ORIGINAL);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: round-trip htole and letoh") {
  constexpr uint16_t ORIGINAL = 0xABCD;
  constexpr uint16_t LITTLE_ENDIAN1 = htole(ORIGINAL);
  constexpr uint16_t BACK_TO_HOST = letoh(LITTLE_ENDIAN1);

  COROSIG_REQUIRE(BACK_TO_HOST == ORIGINAL);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: enum type conversion") {
  enum class TestEnum : uint32_t {
    VALUE1 = 0x12345678,
    VALUE2 = 0x9ABCDEF0,
  };

  constexpr TestEnum VALUE = TestEnum::VALUE1;
  constexpr TestEnum RESULT = betoh(VALUE);

  if constexpr (std::endian::native == std::endian::little) {
    COROSIG_REQUIRE(std::underlying_type_t<TestEnum>(RESULT) == 0x78563412);
  } else {
    COROSIG_REQUIRE(RESULT == VALUE);
  }
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: 8-bit values are unchanged") {
  constexpr uint8_t VALUE = 0xAB;
  constexpr uint8_t RESULT_BE = betoh(VALUE);
  constexpr uint8_t RESULT_LE = letoh(VALUE);

  COROSIG_REQUIRE(RESULT_BE == VALUE);
  COROSIG_REQUIRE(RESULT_LE == VALUE);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: host to network and back is identity") {
  constexpr uint16_t ORIGINAL = 0xBEEF;
  constexpr uint16_t NETWORK = hton(ORIGINAL);
  constexpr uint16_t BACK = ntoh(NETWORK);

  COROSIG_REQUIRE(BACK == ORIGINAL);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: network to host and back is identity") {
  constexpr uint32_t NETWORK = 0xDEADBEEF;
  constexpr uint32_t HOST = ntoh(NETWORK);
  constexpr uint32_t BACK = hton(HOST);

  COROSIG_REQUIRE(BACK == NETWORK);
}

COROSIG_SIGHANDLER_TEST_CASE("Endianness: enum round-trip conversion") {
  enum class Protocol : uint16_t {
    TCP = 0x1234,
    UDP = 0x5678,
  };

  constexpr Protocol ORIGINAL = Protocol::UDP;
  constexpr Protocol NETWORK_ORDER = hton(ORIGINAL);
  constexpr Protocol BACK_TO_HOST = ntoh(NETWORK_ORDER);

  COROSIG_REQUIRE(BACK_TO_HOST == ORIGINAL);
}
