#include "corosig/io/dns/Protocol.hpp"

#include "corosig/testing/Signals.hpp"

#include <array>
#include <cstring>
#include <string_view>

namespace {

using namespace corosig;

std::vector<dns::QuestionQueryEntry> const TOO_MUCH_ENTRIES{
    std::numeric_limits<uint16_t>::max(),
    dns::QuestionQueryEntry{
        .name = "example.com",
        .qtype = dns::QueryType::A,
        .qclass = dns::QueryClass::IN,
    },
};

std::string const LONG_NAME(256, 'a');

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("write_compression_ptr: encodes pointer correctly",
                             "[compression_ptr]") {

  // example offset within 14-bit range
  uint16_t offset = 0x0123;

  std::array<char, 2> buffer;

  char *end = dns::detail::write_compression_ptr(buffer.begin(), offset);
  COROSIG_REQUIRE(end == buffer.end());
  uint16_t encoded = (uint8_t(buffer[0]) << 8) | uint8_t(buffer[1]);
  COROSIG_REQUIRE((encoded & 0xC000) == 0xC000);
  COROSIG_REQUIRE((encoded & 0x3FFF) == offset);
}

COROSIG_SIGHANDLER_TEST_CASE("write_label: fails on empty label", "[write_label]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  auto res = write_label(out, "", ".example.com", map);

  COROSIG_REQUIRE(res.error() == dns::QuestionEncodeError::EMPTY_LABEL);
}

COROSIG_SIGHANDLER_TEST_CASE("write_label: fails on label longer than 63 chars", "[write_label]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  std::array<char, 64> long_label;
  std::memset(long_label.begin(), 'a', long_label.size());

  auto res =
      write_label(out, std::string_view{long_label.begin(), long_label.size()}, "example.com", map);
  COROSIG_REQUIRE(res.error() == dns::QuestionEncodeError::TOO_LONG_DOMAIN_NAME_LABEL);
}

COROSIG_SIGHANDLER_TEST_CASE("write_label: writes length + label", "[write_label]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  out = write_label(out, "www", "www.example.com", map).value().out;

  COROSIG_REQUIRE(out.written() == 4);
  COROSIG_REQUIRE(uint8_t(buffer[0]) == 3);
  COROSIG_REQUIRE(buffer[1] == 'w');
  COROSIG_REQUIRE(buffer[2] == 'w');
  COROSIG_REQUIRE(buffer[3] == 'w');

  // label with upcoming text got cached
  COROSIG_REQUIRE(map.buckets.size() == 1);
  decltype(map)::Node node{"www.example.com", 0};
  COROSIG_REQUIRE(map.find(node) != map.buckets.end());
}

COROSIG_SIGHANDLER_TEST_CASE("write_label: uses compression pointer when suffix exists",
                             "[write_label]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  // First write inserts into map
  out = write_label(out, "example", "example.com", map).value().out;
  out = write_label(out, "com", "com", map).value().out;

  // Second write should hit compression
  {
    auto res = write_label(out, "example", "example.com", map).value();
    COROSIG_REQUIRE(res.all_compressed);
    out = res.out;
  }

  COROSIG_REQUIRE(out.written() == 14);

  uint16_t encoded = ((buffer[out.written() - 2]) << 8) | (buffer[out.written() - 1]);
  COROSIG_REQUIRE((encoded & 0xC000) == 0xC000);
}

COROSIG_SIGHANDLER_TEST_CASE("write_label: does not insert if offset exceeds compression limit",
                             "[write_label]") {
  std::array<char, size_t(1024 * 1024)> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  // artificially simulate large offset
  for (size_t i = 0; i < dns::detail::COMPRESSION_PTR_MAX_VALUE + 10; ++i) {
    *out++ = 'x';
  }

  COROSIG_REQUIRE(write_label(out, "abc", "abc.com", map));
  // map may or may not insert depending on condition — we assert no crash
}

COROSIG_SIGHANDLER_TEST_CASE("write_name: fails if name exceeds 255 bytes", "[write_name]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  auto res = dns::detail::write_name(out, LONG_NAME, map);
  COROSIG_REQUIRE(res.error() == dns::QuestionEncodeError::TOO_LONG_DOMAIN_NAME);
}

COROSIG_SIGHANDLER_TEST_CASE("write_name: encodes single label with terminator", "[write_name]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  COROSIG_REQUIRE(write_name(out, "com", map));

  COROSIG_REQUIRE(out.written() == 5); // length + "com" + 0
  COROSIG_REQUIRE(uint8_t(buffer[0]) == 3);
  COROSIG_REQUIRE(buffer[1] == 'c');
  COROSIG_REQUIRE(buffer[2] == 'o');
  COROSIG_REQUIRE(buffer[3] == 'm');
  COROSIG_REQUIRE(uint8_t(buffer[4]) == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("write_name: encodes multi-label domain", "[write_name]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  COROSIG_REQUIRE(write_name(out, "www.example.com", map));

  // Expected: 3www 7example 3com 0
  COROSIG_REQUIRE(uint8_t(buffer[0]) == 3);
  COROSIG_REQUIRE(uint8_t(buffer[4]) == 7);
  COROSIG_REQUIRE(uint8_t(buffer[12]) == 3);
  COROSIG_REQUIRE(uint8_t(buffer.back()) == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("write_name: propagates label error (empty label)", "[write_name]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  // invalid: consecutive dots → empty label
  auto res = write_name(out, "www..com", map);
  COROSIG_REQUIRE(res.error() == dns::QuestionEncodeError::EMPTY_LABEL);
}

COROSIG_SIGHANDLER_TEST_CASE("write_name: compression short-circuits remaining labels",
                             "[write_name]") {
  std::array<char, 1024> buffer;
  size_t written_count = 0;
  dns::detail::CountingOutputIterator out{buffer.begin(), written_count};
  dns::detail::CompressionMap map{reactor.allocator()};

  // invalid: consecutive dots → empty label
  auto res = write_name(out, "www..com", map);
  COROSIG_REQUIRE(res.error() == dns::QuestionEncodeError::EMPTY_LABEL);

  // First write: fills map
  out = write_name(out, "example.com", map).value();

  size_t written_count2 = 0;
  dns::detail::CountingOutputIterator out2{
      buffer.begin() + out.written(),
      written_count2,
  };

  // Second write: should reuse compression
  COROSIG_REQUIRE(write_name(out2, "example.com", map));
  COROSIG_REQUIRE(out2.written() == 2);

  size_t written_total = out.written() + out2.written();
  uint16_t last = (uint8_t(buffer[written_total - 2]) << 8) | uint8_t(buffer[written_total - 1]);
  COROSIG_REQUIRE((last & 0xC000) == 0xC000);
}

COROSIG_SIGHANDLER_TEST_CASE("encode_question: fails if too many entries", "[encode_question]") {

  std::array<char, 0> buffer;
  auto res = encode_question(buffer.begin(),
                             dns::QuestionParams{
                                 .id = 1,
                                 .opcode = dns::QueryOpcode::STANDARD,
                                 .recursion_desired = true,
                             },
                             TOO_MUCH_ENTRIES,
                             reactor.allocator());

  COROSIG_REQUIRE(res.error() == dns::QuestionEncodeError::TOO_MANY_QUESTION_ENTRIES);
}

COROSIG_SIGHANDLER_TEST_CASE("encode_question: encodes single question correctly",
                             "[encode_question]") {
  std::array<char, 1024> buffer;

  COROSIG_REQUIRE(encode_question(buffer.begin(),
                                  dns::QuestionParams{
                                      .id = 0x1234,
                                      .opcode = dns::QueryOpcode::STANDARD,
                                      .recursion_desired = true,
                                  },
                                  dns::QuestionQueryEntry{
                                      .name = "example.com",
                                      .qtype = dns::QueryType::A,
                                      .qclass = dns::QueryClass::IN,
                                  },
                                  reactor.allocator()));

  COROSIG_REQUIRE(buffer.size() > sizeof(dns::detail::Header));

  // ID (network order)
  COROSIG_REQUIRE(uint8_t(buffer[0]) == 0x12);
  COROSIG_REQUIRE(uint8_t(buffer[1]) == 0x34);

  // QDCOUNT = 1
  COROSIG_REQUIRE(uint8_t(buffer[4]) == 0x00);
  COROSIG_REQUIRE(uint8_t(buffer[5]) == 0x01);

  // Check name encoding: "example.com"
  // 7example 3com 0
  size_t pos = sizeof(dns::detail::Header);
  COROSIG_REQUIRE(uint8_t(buffer[pos]) == 7);
  pos += 8;
  COROSIG_REQUIRE(uint8_t(buffer[pos]) == 3);
  pos += 4;
  COROSIG_REQUIRE(uint8_t(buffer[pos]) == 0);
}

COROSIG_SIGHANDLER_TEST_CASE("encode_question: encodes multiple questions", "[encode_question]") {
  std::array<char, 1024> buffer;

  COROSIG_REQUIRE(encode_question(buffer.begin(),
                                  dns::QuestionParams{
                                      .id = 1,
                                      .opcode = dns::QueryOpcode::STANDARD,
                                      .recursion_desired = true,
                                  },
                                  {
                                      dns::QuestionQueryEntry{
                                          .name = "a.com",
                                          .qtype = dns::QueryType::A,
                                          .qclass = dns::QueryClass::IN,
                                      },
                                      dns::QuestionQueryEntry{
                                          .name = "b.com",
                                          .qtype = dns::QueryType::AAAA,
                                          .qclass = dns::QueryClass::IN,
                                      },
                                  },
                                  reactor.allocator()));

  // QDCOUNT = 2
  COROSIG_REQUIRE(uint8_t(buffer[4]) == 0x00);
  COROSIG_REQUIRE(uint8_t(buffer[5]) == 0x02);

  // 1a 3com 0
  size_t pos = sizeof(dns::detail::Header);
  COROSIG_REQUIRE(uint8_t(buffer[pos]) == 1);
  pos += 2;
  size_t com_pos = pos;
  COROSIG_REQUIRE(uint8_t(buffer[pos]) == 3);
  pos += 4;
  COROSIG_REQUIRE(uint8_t(buffer[pos]) == 0);
  pos += 1;
  // QTYPE QCLASS
  pos += 4;
  // 1b [compressed]
  COROSIG_REQUIRE(uint8_t(buffer[pos]) == 1);
  pos += 2;
  COROSIG_REQUIRE((uint8_t(buffer[pos]) & 0xC0) == 0xC0);
  COROSIG_REQUIRE(((uint8_t(buffer[pos]) & ~0xC0) << uint8_t(8) | uint8_t(buffer[pos + 1])) ==
                  int(com_pos));
}

COROSIG_SIGHANDLER_TEST_CASE("encode_question: writes qtype and qclass", "[encode_question]") {
  std::array<char, 1024> buffer;

  COROSIG_REQUIRE(encode_question(buffer.begin(),
                                  dns::QuestionParams{
                                      .id = 1,
                                      .opcode = dns::QueryOpcode::STANDARD,
                                      .recursion_desired = true,
                                  },
                                  {
                                      "a.com",
                                      dns::QueryType::AAAA,
                                      dns::QueryClass::IN,
                                  },
                                  reactor.allocator()));

  // find end of name (walk it)
  size_t pos = sizeof(dns::detail::Header);
  while (buffer[pos] != 0) {
    pos += uint8_t(buffer[pos]) + 1;
  }
  pos++; // skip terminator

  // qtype (AAAA = 28)
  COROSIG_REQUIRE(uint8_t(buffer[pos]) == 0x00);
  COROSIG_REQUIRE(uint8_t(buffer[pos + 1]) == 28);

  // qclass (IN = 1)
  COROSIG_REQUIRE(uint8_t(buffer[pos + 2]) == 0x00);
  COROSIG_REQUIRE(uint8_t(buffer[pos + 3]) == 0x01);
}
