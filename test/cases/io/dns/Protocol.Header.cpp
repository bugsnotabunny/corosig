#include "corosig/io/dns/Protocol.hpp"

#include "corosig/testing/Signals.hpp"

#include <array>
#include <cstdint>

namespace {

using namespace corosig;

}

COROSIG_SIGHANDLER_TEST_CASE("Flags: query/response bit", "[flags]") {
  dns::Header::Flags f{};

  COROSIG_REQUIRE(!f.is_response());

  f.set_response(true);
  COROSIG_REQUIRE(f.is_response());

  f.set_response(false);
  COROSIG_REQUIRE(!f.is_response());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: opcode set/get", "[flags]") {
  dns::Header::Flags f{};

  f.set_opcode(dns::QueryOpcode::STANDARD);
  COROSIG_REQUIRE(f.opcode() == dns::QueryOpcode::STANDARD);

  f.set_opcode(dns::QueryOpcode::INVERSE);
  COROSIG_REQUIRE(f.opcode() == dns::QueryOpcode::INVERSE);

  f.set_opcode(dns::QueryOpcode::STATUS);
  COROSIG_REQUIRE(f.opcode() == dns::QueryOpcode::STATUS);
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: authoritative answer flag", "[flags]") {
  dns::Header::Flags f{};

  COROSIG_REQUIRE(!f.authoritative_answer());

  f.set_authoritative_answer(true);
  COROSIG_REQUIRE(f.authoritative_answer());

  f.set_authoritative_answer(false);
  COROSIG_REQUIRE(!f.authoritative_answer());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: truncated flag", "[flags]") {
  dns::Header::Flags f{};

  COROSIG_REQUIRE(!f.truncated());

  f.set_truncated(true);
  COROSIG_REQUIRE(f.truncated());

  f.set_truncated(false);
  COROSIG_REQUIRE(!f.truncated());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: recursion desired flag", "[flags]") {
  dns::Header::Flags f{};

  COROSIG_REQUIRE(!f.recursion_desired());

  f.set_recursion_desired(true);
  COROSIG_REQUIRE(f.recursion_desired());

  f.set_recursion_desired(false);
  COROSIG_REQUIRE(!f.recursion_desired());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: recursion available flag", "[flags]") {
  dns::Header::Flags f{};

  COROSIG_REQUIRE(!f.recursion_available());

  f.set_recursion_available(true);
  COROSIG_REQUIRE(f.recursion_available());

  f.set_recursion_available(false);
  COROSIG_REQUIRE(!f.recursion_available());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: rcode set/get", "[flags]") {
  dns::Header::Flags f{};

  f.set_rcode(dns::ServerResponseCode::NOERROR);
  COROSIG_REQUIRE(f.get_rcode() == dns::ServerResponseCode::NOERROR);

  f.set_rcode(dns::ServerResponseCode::REFUSED);
  COROSIG_REQUIRE(f.get_rcode() == dns::ServerResponseCode::REFUSED);

  f.set_rcode(dns::ServerResponseCode::Value{15});                      // NOLINT
  COROSIG_REQUIRE(f.get_rcode() == dns::ServerResponseCode::Value{15}); // NOLINT
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: chaining works correctly", "[flags]") {
  dns::Header::Flags f{};

  f.set_response(true)
      .set_authoritative_answer(true)
      .set_truncated(true)
      .set_recursion_desired(true)
      .set_recursion_available(true)
      .set_rcode(dns::ServerResponseCode::NAME_ERROR);

  COROSIG_REQUIRE(f.is_response());
  COROSIG_REQUIRE(f.authoritative_answer());
  COROSIG_REQUIRE(f.truncated());
  COROSIG_REQUIRE(f.recursion_desired());
  COROSIG_REQUIRE(f.recursion_available());
  COROSIG_REQUIRE(f.get_rcode() == dns::ServerResponseCode::NAME_ERROR);
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: comparison operator", "[flags]") {
  dns::Header::Flags a{};
  dns::Header::Flags b{};

  COROSIG_REQUIRE(a == b);

  a.set_response(true);
  COROSIG_REQUIRE(a != b);

  b.set_response(true);
  COROSIG_REQUIRE(a == b);
}

COROSIG_SIGHANDLER_TEST_CASE("dns::Header: default values", "[dns::Header]") {
  dns::Header h{};

  COROSIG_REQUIRE(h.id == 0);
  COROSIG_REQUIRE(h.qdcount == 0);
  COROSIG_REQUIRE(h.ancount == 0);
  COROSIG_REQUIRE(h.nscount == 0);
  COROSIG_REQUIRE(h.arcount == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("dns::Header: comparison operator", "[dns::Header]") {
  dns::Header a{};
  dns::Header b{};

  COROSIG_REQUIRE(a == b);

  a.id = 42;
  COROSIG_REQUIRE(a != b);

  b.id = 42;
  COROSIG_REQUIRE(a == b);

  a.flags.set_response(true);
  COROSIG_REQUIRE(a != b);

  b.flags.set_response(true);
  COROSIG_REQUIRE(a == b);
}

COROSIG_SIGHANDLER_TEST_CASE("encode_header: produces 12 bytes", "[encode]") {
  dns::Header h{};
  std::array<char, 1024> buffer;
  char *it = dns::detail::encode_header(buffer.begin(), h);
  COROSIG_REQUIRE(it - buffer.begin() == 12);
}

COROSIG_SIGHANDLER_TEST_CASE("encode_header: encodes fields in network byte order", "[encode]") {
  dns::Header h{};
  h.id = 0x1234;
  h.qdcount = 0x0102;
  h.ancount = 0x0304;
  h.nscount = 0x0506;
  h.arcount = 0x0708;

  std::array<char, 1024> buffer;
  dns::detail::encode_header(buffer.begin(), h);

  COROSIG_REQUIRE(uint8_t(buffer[0]) == 0x12);
  COROSIG_REQUIRE(uint8_t(buffer[1]) == 0x34);

  COROSIG_REQUIRE(uint8_t(buffer[4]) == 0x01);
  COROSIG_REQUIRE(uint8_t(buffer[5]) == 0x02);

  COROSIG_REQUIRE(uint8_t(buffer[6]) == 0x03);
  COROSIG_REQUIRE(uint8_t(buffer[7]) == 0x04);

  COROSIG_REQUIRE(uint8_t(buffer[8]) == 0x05);
  COROSIG_REQUIRE(uint8_t(buffer[9]) == 0x06);

  COROSIG_REQUIRE(uint8_t(buffer[10]) == 0x07);
  COROSIG_REQUIRE(uint8_t(buffer[11]) == 0x08);
}

COROSIG_SIGHANDLER_TEST_CASE("encode_header: encodes flags correctly", "[encode]") {
  dns::Header h{};
  h.flags.set_response(true)
      .set_opcode(dns::QueryOpcode::STATUS)
      .set_authoritative_answer(true)
      .set_truncated(true)
      .set_recursion_desired(true)
      .set_recursion_available(true)
      .set_rcode(dns::ServerResponseCode::Value(9)); // NOLINT

  std::array<char, 1024> buffer;
  dns::detail::encode_header(buffer.begin(), h);

  auto flags1 = uint8_t(buffer[2]);
  auto flags2 = uint8_t(buffer[3]);

  COROSIG_REQUIRE((flags1 & 0x80) != 0);        // QR
  COROSIG_REQUIRE(((flags1 >> 3) & 0x0F) == 2); // opcode
  COROSIG_REQUIRE((flags1 & 0x04) != 0);        // AA
  COROSIG_REQUIRE((flags1 & 0x02) != 0);        // TC
  COROSIG_REQUIRE((flags1 & 0x01) != 0);        // RD

  COROSIG_REQUIRE((flags2 & 0x80) != 0); // RA
  COROSIG_REQUIRE((flags2 & 0x0F) == 9); // RCODE
}
