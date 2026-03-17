#include "corosig/io/dns/Protocol.hpp"

#include "corosig/testing/Signals.hpp"

#include <array>
#include <cstring>

namespace {

using namespace corosig;

}

COROSIG_SIGHANDLER_TEST_CASE("Flags: query/response bit", "[flags]") {
  dns::detail::Header::Flags f{};

  COROSIG_REQUIRE(!f.is_response());

  f.set_response();
  COROSIG_REQUIRE(f.is_response());

  f.set_query();
  COROSIG_REQUIRE(!f.is_response());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: opcode set/get", "[flags]") {
  dns::detail::Header::Flags f{};

  f.set_opcode(dns::QueryOpcode::STANDARD);
  COROSIG_REQUIRE(f.opcode() == 0);

  f.set_opcode(dns::QueryOpcode::INVERSE);
  COROSIG_REQUIRE(f.opcode() == 1);

  f.set_opcode(dns::QueryOpcode::STATUS);
  COROSIG_REQUIRE(f.opcode() == 2);
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: authoritative answer flag", "[flags]") {
  dns::detail::Header::Flags f{};

  COROSIG_REQUIRE(!f.authoritative_answer());

  f.set_authoritative_answer(true);
  COROSIG_REQUIRE(f.authoritative_answer());

  f.set_authoritative_answer(false);
  COROSIG_REQUIRE(!f.authoritative_answer());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: truncated flag", "[flags]") {
  dns::detail::Header::Flags f{};

  COROSIG_REQUIRE(!f.truncated());

  f.set_truncated(true);
  COROSIG_REQUIRE(f.truncated());

  f.set_truncated(false);
  COROSIG_REQUIRE(!f.truncated());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: recursion desired flag", "[flags]") {
  dns::detail::Header::Flags f{};

  COROSIG_REQUIRE(!f.recursion_desired());

  f.set_recursion_desired(true);
  COROSIG_REQUIRE(f.recursion_desired());

  f.set_recursion_desired(false);
  COROSIG_REQUIRE(!f.recursion_desired());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: recursion available flag", "[flags]") {
  dns::detail::Header::Flags f{};

  COROSIG_REQUIRE(!f.recursion_available());

  f.set_recursion_available(true);
  COROSIG_REQUIRE(f.recursion_available());

  f.set_recursion_available(false);
  COROSIG_REQUIRE(!f.recursion_available());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: rcode set/get", "[flags]") {
  dns::detail::Header::Flags f{};

  f.set_rcode(0);
  COROSIG_REQUIRE(f.get_rcode() == 0);

  f.set_rcode(5);
  COROSIG_REQUIRE(f.get_rcode() == 5);

  f.set_rcode(15);
  COROSIG_REQUIRE(f.get_rcode() == 15);

  // Ensure masking to 4 bits
  f.set_rcode(0xFF);
  COROSIG_REQUIRE(f.get_rcode() == 0x0F);
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: standard query helper", "[flags]") {
  dns::detail::Header::Flags f{};

  f.set_standard_query();

  COROSIG_REQUIRE(!f.is_response());
  COROSIG_REQUIRE(f.opcode() == 0);
  COROSIG_REQUIRE(f.recursion_desired());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: standard response helper", "[flags]") {
  dns::detail::Header::Flags f{};

  f.set_standard_response();

  COROSIG_REQUIRE(f.is_response());
  COROSIG_REQUIRE(!f.recursion_available());
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: chaining works correctly", "[flags]") {
  dns::detail::Header::Flags f{};

  f.set_response()
      .set_authoritative_answer(true)
      .set_truncated(true)
      .set_recursion_desired(true)
      .set_recursion_available(true)
      .set_rcode(3);

  COROSIG_REQUIRE(f.is_response());
  COROSIG_REQUIRE(f.authoritative_answer());
  COROSIG_REQUIRE(f.truncated());
  COROSIG_REQUIRE(f.recursion_desired());
  COROSIG_REQUIRE(f.recursion_available());
  COROSIG_REQUIRE(f.get_rcode() == 3);
}

COROSIG_SIGHANDLER_TEST_CASE("Flags: comparison operator", "[flags]") {
  dns::detail::Header::Flags a{};
  dns::detail::Header::Flags b{};

  COROSIG_REQUIRE(a == b);

  a.set_response();
  COROSIG_REQUIRE(a != b);

  b.set_response();
  COROSIG_REQUIRE(a == b);
}

COROSIG_SIGHANDLER_TEST_CASE("dns::detail::Header: default values", "[dns::detail::Header]") {
  dns::detail::Header h{};

  COROSIG_REQUIRE(h.id == 0);
  COROSIG_REQUIRE(h.qdcount == 0);
  COROSIG_REQUIRE(h.ancount == 0);
  COROSIG_REQUIRE(h.nscount == 0);
  COROSIG_REQUIRE(h.arcount == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("dns::detail::Header: comparison operator", "[dns::detail::Header]") {
  dns::detail::Header a{};
  dns::detail::Header b{};

  COROSIG_REQUIRE(a == b);

  a.id = 42;
  COROSIG_REQUIRE(a != b);

  b.id = 42;
  COROSIG_REQUIRE(a == b);

  a.flags.set_response();
  COROSIG_REQUIRE(a != b);

  b.flags.set_response();
  COROSIG_REQUIRE(a == b);
}

COROSIG_SIGHANDLER_TEST_CASE("encode_header: produces 12 bytes", "[encode]") {
  dns::detail::Header h{};
  std::array<char, 1024> buffer;
  char *it = encode_header(buffer.begin(), h);
  COROSIG_REQUIRE(it - buffer.begin() == 12);
}

COROSIG_SIGHANDLER_TEST_CASE("encode_header: encodes fields in network byte order", "[encode]") {
  dns::detail::Header h{};
  h.id = 0x1234;
  h.qdcount = 0x0102;
  h.ancount = 0x0304;
  h.nscount = 0x0506;
  h.arcount = 0x0708;

  std::array<char, 1024> buffer;
  encode_header(buffer.begin(), h);

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
  dns::detail::Header h{};
  h.flags.set_response()
      .set_opcode(dns::QueryOpcode::STATUS)
      .set_authoritative_answer(true)
      .set_truncated(true)
      .set_recursion_desired(true)
      .set_recursion_available(true)
      .set_rcode(9);

  std::array<char, 1024> buffer;
  encode_header(buffer.begin(), h);

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

COROSIG_SIGHANDLER_TEST_CASE("decode_header: fails on too small input", "[decode]") {
  std::array<uint8_t, 5> small{};
  COROSIG_REQUIRE(!dns::detail::decode_header(small));
}

COROSIG_SIGHANDLER_TEST_CASE("decode_header: decodes basicdns::detail::Header correctly",
                             "[decode]") {
  std::array<uint8_t, 12> data{
      0x12,
      0x34, // id
      0x81,
      0x80, // flags (response + RD + RA)
      0x00,
      0x01, // qdcount
      0x00,
      0x02, // ancount
      0x00,
      0x03, // nscount
      0x00,
      0x04 // arcount
  };

  auto h = dns::detail::decode_header(data).value();

  COROSIG_REQUIRE(h.id == 0x1234);
  COROSIG_REQUIRE(h.qdcount == 1);
  COROSIG_REQUIRE(h.ancount == 2);
  COROSIG_REQUIRE(h.nscount == 3);
  COROSIG_REQUIRE(h.arcount == 4);

  COROSIG_REQUIRE(h.flags.is_response());
  COROSIG_REQUIRE(h.flags.recursion_desired());
  COROSIG_REQUIRE(h.flags.recursion_available());
}

COROSIG_SIGHANDLER_TEST_CASE("decode_header: decodes flags correctly", "[decode]") {
  std::array<uint8_t, 12> data{0x00,
                               0x01,
                               0b11101011, // flags1
                               0b10001010, // flags2
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0};

  auto f = dns::detail::decode_header(data).value().flags;

  COROSIG_REQUIRE(f.is_response());
  COROSIG_REQUIRE(f.opcode() == 0b1101);
  COROSIG_REQUIRE(!f.authoritative_answer());
  COROSIG_REQUIRE(f.truncated());
  COROSIG_REQUIRE(f.recursion_desired());
  COROSIG_REQUIRE(f.recursion_available());
  COROSIG_REQUIRE(f.get_rcode() == 0b1010);
}

COROSIG_SIGHANDLER_TEST_CASE("encode/decode: roundtrip identity", "[roundtrip]") {
  dns::detail::Header original{
      .id = 0xBEEF,
      .flags = dns::detail::Header::Flags{}
                   .set_response()
                   .set_opcode(dns::QueryOpcode::INVERSE)
                   .set_authoritative_answer(true)
                   .set_truncated(true)
                   .set_recursion_desired(true)
                   .set_recursion_available(true)
                   .set_rcode(7),
      .qdcount = 10,
      .ancount = 20,
      .nscount = 30,
      .arcount = 40,

  };

  std::array<char, 12> buffer;
  encode_header(buffer.begin(), original);

  std::array<uint8_t, 12> bytes;
  std::memcpy(bytes.begin(), buffer.begin(), buffer.size());

  auto decoded = dns::detail::decode_header(bytes);

  COROSIG_REQUIRE(decoded.value() == original);
}
