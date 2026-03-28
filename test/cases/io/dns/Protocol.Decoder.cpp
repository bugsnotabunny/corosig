#include "corosig/io/dns/Protocol.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/generators/catch_generators.hpp"
#include "corosig/container/Allocator.hpp"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <string_view>

namespace {

#define REQUIRE_ERROR(res, expected)                                                               \
  do {                                                                                             \
    REQUIRE_FALSE(res);                                                                            \
    REQUIRE((res).error() == (expected));                                                          \
  } while (false)

using namespace corosig::dns;

std::vector<uint8_t>
make_basic_header(uint16_t id, uint16_t flags, uint16_t qd, uint16_t an, uint16_t ns, uint16_t ar) {
  std::vector<uint8_t> buf;

  auto push16 = [&](uint16_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
  };

  push16(id);
  push16(flags);
  push16(qd);
  push16(an);
  push16(ns);
  push16(ar);

  return buf;
}

std::vector<uint8_t> encode_name(std::string_view name) {
  std::vector<uint8_t> out;

  size_t start = 0;
  while (start < name.size()) {
    auto dot = name.find('.', start);
    if (dot == std::string_view::npos) {
      dot = name.size();
    }

    auto len = dot - start;
    out.push_back(static_cast<uint8_t>(len));
    for (size_t i = 0; i < len; ++i) {
      out.push_back(static_cast<uint8_t>(name[start + i]));
    }

    start = dot + 1;
  }

  out.push_back(0); // terminator
  return out;
}

} // namespace

TEST_CASE("Decode valid DNS header") {
  auto raw = make_basic_header(0x1234,
                               0x8180, // standard response, recursion available
                               1,
                               2,
                               0,
                               0);

  ResponseDecoder decoder(raw);

  auto result = decoder.consume_header();
  REQUIRE(result);

  auto const &header = result.value();
  REQUIRE(header.id == 0x1234);
  REQUIRE(header.qdcount == 1);
  REQUIRE(header.ancount == 2);
  REQUIRE(header.nscount == 0);
  REQUIRE(header.arcount == 0);

  REQUIRE(header.flags.is_response() == true);
}

TEST_CASE("Fail on truncated header", "[dns][header][error]") {
  std::vector<uint8_t> raw = {0x12, 0x34}; // too short

  ResponseDecoder decoder(raw);

  auto result = decoder.consume_header();
  REQUIRE_FALSE(result);
}

TEST_CASE("Decode single question entry", "[dns][question]") {
  auto header = make_basic_header(1, 0x0100, 1, 0, 0, 0);
  auto name = encode_name("example.com");

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), header.begin(), header.end());
  raw.insert(raw.end(), name.begin(), name.end());

  // QTYPE = A, QCLASS = IN
  raw.push_back(0x00);
  raw.push_back(0x01);
  raw.push_back(0x00);
  raw.push_back(0x01);

  ResponseDecoder decoder{raw};

  REQUIRE(decoder.consume_header());

  auto q = decoder.consume_question_entry();
  REQUIRE(q);

  REQUIRE(q.value().qtype == QueryType::A);
  REQUIRE(q.value().qclass == QueryClass::IN);
  REQUIRE(std::ranges::equal(q.value().name.labels_svs(), split_into_labels("example.com")));
}

TEST_CASE("Fail on truncated question", "[dns][question][error]") {
  auto header = make_basic_header(1, 0x0100, 1, 0, 0, 0);

  std::vector<uint8_t> raw = header; // missing question body

  ResponseDecoder decoder(raw);
  REQUIRE(decoder.consume_header());

  auto q = decoder.consume_question_entry();
  REQUIRE_FALSE(q);
}

TEST_CASE("Decode A record", "[dns][rr][A]") {
  auto header = make_basic_header(1, 0x8180, 0, 1, 0, 0);
  auto name = encode_name("example.com");

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), header.begin(), header.end());
  raw.insert(raw.end(), name.begin(), name.end());

  // TYPE = A
  raw.push_back(0x00);
  raw.push_back(0x01);

  // CLASS = IN
  raw.push_back(0x00);
  raw.push_back(0x01);

  // TTL = 300
  raw.push_back(0x00);
  raw.push_back(0x00);
  raw.push_back(0x01);
  raw.push_back(0x2C);

  // RDLENGTH = 4
  raw.push_back(0x00);
  raw.push_back(0x04);

  // 127.0.0.1
  raw.push_back(127);
  raw.push_back(0);
  raw.push_back(0);
  raw.push_back(1);

  ResponseDecoder decoder(raw);

  REQUIRE(decoder.consume_header());

  auto rr = decoder.consume_resource_record();
  REQUIRE(rr);

  auto const &record = rr.value();

  REQUIRE(record.ttl.count() == 300);

  REQUIRE(std::holds_alternative<RDataIpv4>(record.rdata));
  auto const &ipv4 = std::get<RDataIpv4>(record.rdata);

  REQUIRE(ipv4.addr == 0x7F000001);
}

TEST_CASE("Fail on truncated resource record", "[dns][rr][error]") {
  auto header = make_basic_header(1, 0x8180, 0, 1, 0, 0);

  std::vector<uint8_t> raw = header; // missing RR

  ResponseDecoder decoder(raw);

  REQUIRE(decoder.consume_header());

  auto rr = decoder.consume_resource_record();
  REQUIRE_FALSE(rr);
}

TEST_CASE("Decoder becomes invalid after error", "[dns][state]") {
  std::vector<uint8_t> raw = {0x00}; // invalid

  ResponseDecoder decoder(raw);

  auto h = decoder.consume_header();
  REQUIRE_FALSE(h);

  // further calls should also fail
  auto q = decoder.consume_question_entry();
  REQUIRE_FALSE(q);
}

TEST_CASE("Label length too big (>63)", "[dns][label][error]") {
  std::vector<uint8_t> raw = {
      0x00,
      0x01,
      0x00,
      0x00,
      0x00,
      0x01,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      64 // invalid label length
  };

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_ERROR(res, ResponseDecodeError::LABEL_LEN_TOO_BIG);
}

TEST_CASE("Label length exceeds buffer", "[dns][label][error]") {
  std::vector<uint8_t> raw = {
      0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 5, 'a', 'b' // claims 5, only 2 bytes
  };

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_ERROR(res, ResponseDecodeError::LABEL_LEN_OOB);
}

TEST_CASE("Missing root label terminator", "[dns][label][error]") {
  std::vector<uint8_t> raw = {
      0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 3, 'w', 'w', 'w' // no terminating 0
  };

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_ERROR(res, ResponseDecodeError::NO_ROOT_LABEL);
}

TEST_CASE("Compression pointer OOB", "[dns][compression][error]") {
  std::vector<uint8_t> raw = {
      0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0xC0, 0xFF // pointer way outside message
  };

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_ERROR(res, ResponseDecodeError::COMPRESSION_PTR_OOB);
}

TEST_CASE("Compression pointer infinite loop", "[dns][compression][error]") {
  std::vector<uint8_t> raw = {
      0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0xC0, 0x0C // points to itself (offset 12)
  };

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_ERROR(res, ResponseDecodeError::COMPRESSION_PTR_INFINITE_LOOP);
}

TEST_CASE("Domain name exceeds 255 bytes", "[dns][name][error]") {
  std::vector<uint8_t> raw = {0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};

  // construct many labels to exceed 255
  for (int i = 0; i < 50; ++i) {
    raw.push_back(5);
    raw.insert(raw.end(), {'a', 'a', 'a', 'a', 'a'});
  }
  raw.push_back(0);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_ERROR(res, ResponseDecodeError::NAME_LEN_TOO_BIG);
}

TEST_CASE("Missing question type", "[dns][question][error]") {
  auto header = make_basic_header(1, 0, 1, 0, 0, 0);
  auto name = encode_name("a.com");

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), header.begin(), header.end());
  raw.insert(raw.end(), name.begin(), name.end());
  // missing qtype + qclass

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_ERROR(res, ResponseDecodeError::NO_QUESTION_TYPE);
}

TEST_CASE("Missing question class", "[dns][question][error]") {
  auto header = make_basic_header(1, 0, 1, 0, 0, 0);
  auto name = encode_name("a.com");

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), header.begin(), header.end());
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0x00);
  raw.push_back(0x01); // qtype only

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_ERROR(res, ResponseDecodeError::NO_QUESTION_CLASS);
}

TEST_CASE("RR missing type", "[dns][rr][error]") {
  auto header = make_basic_header(1, 0, 0, 1, 0, 0);
  auto name = encode_name("a.com");

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), header.begin(), header.end());
  raw.insert(raw.end(), name.begin(), name.end());

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_resource_record();
  REQUIRE_ERROR(res, ResponseDecodeError::NO_RESOURCE_RECORD_TYPE);
}

TEST_CASE("RR data length exceeds buffer", "[dns][rr][error]") {
  auto header = make_basic_header(1, 0, 0, 1, 0, 0);
  auto name = encode_name("a.com");

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), header.begin(), header.end());
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(1); // TYPE A
  raw.push_back(0);
  raw.push_back(1);                    // CLASS
  raw.insert(raw.end(), {0, 0, 0, 1}); // TTL

  raw.push_back(0);
  raw.push_back(10); // rdlen too big

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_resource_record();
  REQUIRE_ERROR(res, ResponseDecodeError::RESOURCE_RECORD_DATA_LEN_OOB);
}

TEST_CASE("Bad IPv4 RDATA size", "[dns][rr][A][error]") {
  auto header = make_basic_header(1, 0, 0, 1, 0, 0);
  auto name = encode_name("a.com");

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), header.begin(), header.end());
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(1); // A
  raw.push_back(0);
  raw.push_back(1);
  raw.insert(raw.end(), {0, 0, 0, 1});

  raw.push_back(0);
  raw.push_back(3); // invalid len
  raw.insert(raw.end(), {1, 2, 3});

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_resource_record();
  REQUIRE_ERROR(res, ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_A);
}

TEST_CASE("Bad IPv6 RDATA size", "[dns][rr][AAAA][error]") {
  auto header = make_basic_header(1, 0, 0, 1, 0, 0);
  auto name = encode_name("a.com");

  std::vector<uint8_t> raw;
  raw.insert(raw.end(), header.begin(), header.end());
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(28); // AAAA
  raw.push_back(0);
  raw.push_back(1);
  raw.insert(raw.end(), {0, 0, 0, 1});

  raw.push_back(0);
  raw.push_back(15); // should be 16
  raw.resize(raw.size() + 15);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_resource_record();
  REQUIRE_ERROR(res, ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_AAAA);
}

TEST_CASE("Decoder invalidates after failure", "[dns][state]") {
  std::vector<uint8_t> raw = {0}; // broken

  ResponseDecoder d(raw);

  auto h = d.consume_header();
  REQUIRE_ERROR(h, ResponseDecodeError::WRONG_HEADER_BYTES_COUNT);

  // must stay invalid
  auto q = d.consume_question_entry();
  REQUIRE_FALSE(q);

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
}

TEST_CASE("Compression pointer indirect infinite loop", "[dns][compression][loop]") {
  // Layout:
  // offset 12 -> ptr to 14
  // offset 14 -> ptr to 16
  // offset 16 -> ptr to 12

  std::vector<uint8_t> raw = {
      // header
      0,
      1,
      0,
      0,
      0,
      1,
      0,
      0,
      0,
      0,
      0,
      0,

      // offset 12:
      0xC0,
      14,

      // offset 14:
      0xC0,
      16,

      // offset 16:
      0xC0,
      12,
  };

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();

  REQUIRE_FALSE(res);
  REQUIRE(res.error().value == ResponseDecodeError::COMPRESSION_PTR_INFINITE_LOOP);
}

TEST_CASE("Compression pointer valid chain", "[dns][compression][valid]") {
  // Build:
  // 12: "example.com"
  // later: pointer → example.com

  std::vector<uint8_t> raw;

  auto header = make_basic_header(1, 0, 1, 0, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  // offset 12: example.com
  auto base = encode_name("example.com");
  raw.insert(raw.end(), base.begin(), base.end());

  // second name = pointer to offset 12
  raw.push_back(0xC0);
  raw.push_back(12);

  // qtype/qclass
  raw.push_back(0);
  raw.push_back(1);
  raw.push_back(0);
  raw.push_back(1);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE(res);

  REQUIRE(std::ranges::equal(res.value().name.labels_svs(), split_into_labels("example.com")));
}

TEST_CASE("Labels mixed with compression pointer", "[dns][compression][mixed]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(1, 0, 1, 0, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  // offset 12: example.com
  auto base = encode_name("example.com");
  raw.insert(raw.end(), base.begin(), base.end());

  // now mixed name:
  // "www" + pointer to example.com
  raw.push_back(3);
  raw.insert(raw.end(), {'w', 'w', 'w'});

  raw.push_back(0xC0);
  raw.push_back(12);

  // qtype/qclass
  raw.push_back(0);
  raw.push_back(1);
  raw.push_back(0);
  raw.push_back(1);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE(res);

  REQUIRE(std::ranges::equal(res.value().name.labels_svs(), split_into_labels("example.com")));
}

TEST_CASE("Encode -> Decode roundtrip", "[dns][roundtrip]") {
  using corosig::dns::encode_question;

  std::vector<char> buffer(512);

  QuestionParams params{
      .id = 0xBEEF,
      .opcode = QueryOpcode::STANDARD,
      .recursion_desired = true,
  };

  std::vector<QuestionQueryEntry> entries = {
      {"example.com", QueryType::A, QueryClass::IN},
      {"www.example.com", QueryType::AAAA, QueryClass::IN},
  };

  std::array<char, 1024> alloc_buf;
  corosig::Allocator alloc{alloc_buf};

  auto res = encode_question(buffer.begin(), params, entries, alloc);
  REQUIRE(res);

  size_t size = std::distance(buffer.begin(), res.value());

  std::span<uint8_t const> wire{reinterpret_cast<uint8_t const *>(buffer.data()), size};

  ResponseDecoder d(wire);

  auto header = d.consume_header();
  REQUIRE(header);

  REQUIRE(header.value().id == 0xBEEF);
  REQUIRE(header.value().qdcount == entries.size());

  for (auto &entry : entries) {
    auto q = d.consume_question_entry();
    REQUIRE(q);

    REQUIRE(std::ranges::equal(q.value().name.labels_svs(), split_into_labels(entry.name)));
    REQUIRE(q.value().qtype == entry.qtype);
    REQUIRE(q.value().qclass == entry.qclass);
  }
}

TEST_CASE("Compression pointer to pointer chain (valid)", "[dns][compression][chain]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(1, 0, 1, 0, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  // ---- offset 12 ----
  // "example.com"
  auto base = encode_name("example.com");
  raw.insert(raw.end(), base.begin(), base.end());

  // ---- offset 12 + base.size() ----
  size_t offset_ptr1 = raw.size();

  // pointer -> 12
  raw.push_back(0xC0);
  raw.push_back(12);

  // pointer -> offset_ptr1 (pointer to pointer)
  raw.push_back(0xC0);
  raw.push_back(static_cast<uint8_t>(offset_ptr1));

  // question type/class
  raw.push_back(0);
  raw.push_back(1);
  raw.push_back(0);
  raw.push_back(1);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE(res);

  REQUIRE(std::ranges::equal(res.value().name.labels_svs(), split_into_labels("example.com")));
}

TEST_CASE("Truncated DNS message", "[dns][bounds]") {
  std::vector<uint8_t> raw = {
      0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 3, 'w', 'w', 'w' // missing rest
  };

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto res = d.consume_question_entry();
  REQUIRE_FALSE(res);
}

TEST_CASE("NO_RESOURCE_RECORD_CLASS", "[dns][error]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  // type present
  raw.push_back(0);
  raw.push_back(1);

  // CLASS missing

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  CHECK(rr.error().value == ResponseDecodeError::NO_RESOURCE_RECORD_CLASS);
}

TEST_CASE("NO_RESOURCE_RECORD_TTL", "[dns][error]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(1); // type
  raw.push_back(0);
  raw.push_back(1); // class

  // TTL missing

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  CHECK(rr.error().value == ResponseDecodeError::NO_RESOURCE_RECORD_TTL);
}

TEST_CASE("NO_RESOURCE_RECORD_DATA_LEN", "[dns][error]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(1); // type
  raw.push_back(0);
  raw.push_back(1); // class

  raw.insert(raw.end(), {0, 0, 0, 10}); // TTL

  // missing RDLENGTH

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  CHECK(rr.error().value == ResponseDecodeError::NO_RESOURCE_RECORD_DATA_LEN);
}

TEST_CASE("RESOURCE_RECORD_DATA_BAD_NS", "[dns][error]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(2); // NS
  raw.push_back(0);
  raw.push_back(1);

  raw.insert(raw.end(), {0, 0, 0, 10}); // TTL

  raw.push_back(0);
  raw.push_back(1); // RDLENGTH = 1 (invalid)

  raw.push_back(0xFF); // garbage

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  CHECK(rr.error().value == ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_NS);
}

TEST_CASE("RESOURCE_RECORD_DATA_BAD_CNAME", "[dns][error]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("a.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(5); // CNAME
  raw.push_back(0);
  raw.push_back(1);

  raw.insert(raw.end(), {0, 0, 0, 10});

  raw.push_back(0);
  raw.push_back(1); // invalid len
  raw.push_back(0xFF);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  CHECK(rr.error().value == ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_CNAME);
}

TEST_CASE("RESOURCE_RECORD_DATA_BAD_SOA", "[dns][error]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(6); // SOA
  raw.push_back(0);
  raw.push_back(1);

  raw.insert(raw.end(), {0, 0, 0, 10});

  raw.push_back(0);
  raw.push_back(2); // too small

  raw.push_back(0);
  raw.push_back(0);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  CHECK(rr.error().value == ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_SOA);
}

TEST_CASE("RR bad single DN rdata types", "[dns][error]") {
  auto test_data = GENERATE(std::pair{12, ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_PTR},
                            std::pair{7, ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_MB},
                            std::pair{8, ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_MG},
                            std::pair{9, ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_MR});

  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("1.0.0.127.in-addr.arpa");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(test_data.first); // TYPE
  raw.push_back(0);
  raw.push_back(1);

  raw.insert(raw.end(), {0, 0, 0, 10});

  raw.push_back(0);
  raw.push_back(1); // invalid
  raw.push_back(0xFF);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  CHECK(rr.error().value == test_data.second);
}

TEST_CASE("RESOURCE_RECORD_DATA_BAD_HINFO", "[dns][error]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(13); // HINFO
  raw.push_back(0);
  raw.push_back(1);

  raw.insert(raw.end(), {0, 0, 0, 10});

  raw.push_back(0);
  raw.push_back(2);
  raw.push_back(1); // only one string length, missing second
  raw.push_back('a');

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  REQUIRE(rr.error().value == ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_HINFO);
}

TEST_CASE("RESOURCE_RECORD_DATA_BAD_MINFO", "[dns][error]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(14); // MINFO
  raw.push_back(0);
  raw.push_back(1);

  raw.insert(raw.end(), {0, 0, 0, 10});

  raw.push_back(0);
  raw.push_back(1); // too small

  raw.push_back(0xFF);

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE_FALSE(rr);
  CHECK(rr.error().value == ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_MINFO);
}

TEST_CASE("Decode NS record", "[dns][rdata][ns]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(2); // NS
  raw.push_back(0);
  raw.push_back(1);                     // IN
  raw.insert(raw.end(), {0, 0, 0, 10}); // TTL

  auto ns = encode_name("ns1.example.com");

  raw.push_back(static_cast<uint8_t>(ns.size() >> 8));
  raw.push_back(static_cast<uint8_t>(ns.size() & 0xFF));
  raw.insert(raw.end(), ns.begin(), ns.end());

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE(rr);

  REQUIRE(rr.value().rdata.holds<RDataNameServer>());
}

TEST_CASE("Decode CNAME record", "[dns][rdata][cname]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("www.example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(5); // CNAME
  raw.push_back(0);
  raw.push_back(1);
  raw.insert(raw.end(), {0, 0, 0, 10});

  auto cname = encode_name("example.com");

  raw.push_back(0);
  raw.push_back(static_cast<uint8_t>(cname.size()));
  raw.insert(raw.end(), cname.begin(), cname.end());

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE(rr);

  REQUIRE(rr.value().rdata.holds<RDataCanonicalName>());
}

TEST_CASE("Decode SOA record", "[dns][rdata][soa]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(6); // SOA
  raw.push_back(0);
  raw.push_back(1);
  raw.insert(raw.end(), {0, 0, 0, 10});

  auto mname = encode_name("ns1.example.com");
  auto rname = encode_name("hostmaster.example.com");

  std::vector<uint8_t> rdata;
  rdata.insert(rdata.end(), mname.begin(), mname.end());
  rdata.insert(rdata.end(), rname.begin(), rname.end());

  auto push32 = [&](uint32_t v) {
    rdata.push_back((v >> 24) & 0xFF);
    rdata.push_back((v >> 16) & 0xFF);
    rdata.push_back((v >> 8) & 0xFF);
    rdata.push_back(v & 0xFF);
  };

  push32(1);
  push32(2);
  push32(3);
  push32(4);
  push32(5);

  raw.push_back((rdata.size() >> 8) & 0xFF);
  raw.push_back(rdata.size() & 0xFF);
  raw.insert(raw.end(), rdata.begin(), rdata.end());

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE(rr);

  REQUIRE(rr.value().rdata.holds<RDataStartOfAuthority>());
}

TEMPLATE_TEST_CASE_SIG("Decode one DN record",
                       "[dns][rdata][ptr]",
                       ((uint8_t RTYPE, typename EXPECTED_VARIANT), RTYPE, EXPECTED_VARIANT),
                       (12, RDataPtr),
                       (7, RDataMailbox),
                       (8, RDataMailGroup),
                       (9, RDataMailboxRename)) {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("1.0.0.127.in-addr.arpa");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(RTYPE); // PTR
  raw.push_back(0);
  raw.push_back(1);
  raw.insert(raw.end(), {0, 0, 0, 10});

  auto ptr = encode_name("localhost");

  raw.push_back(0);
  raw.push_back(ptr.size());
  raw.insert(raw.end(), ptr.begin(), ptr.end());

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE(rr);

  REQUIRE(rr.value().rdata.holds<EXPECTED_VARIANT>());
}

TEST_CASE("Decode HINFO record", "[dns][rdata][hinfo]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(13); // HINFO
  raw.push_back(0);
  raw.push_back(1);
  raw.insert(raw.end(), {0, 0, 0, 10});

  std::vector<uint8_t> rdata = {3, 'x', '8', '6', 5, 'l', 'i', 'n', 'u', 'x'};

  raw.push_back(0);
  raw.push_back(rdata.size());
  raw.insert(raw.end(), rdata.begin(), rdata.end());

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE(rr);

  REQUIRE(rr.value().rdata.holds<RDataHardwareInfo>());
}
TEST_CASE("Decode MINFO record", "[dns][rdata][minfo]") {
  std::vector<uint8_t> raw;

  auto header = make_basic_header(0, 1, 0, 1, 0, 0);
  raw.insert(raw.end(), header.begin(), header.end());

  auto name = encode_name("example.com");
  raw.insert(raw.end(), name.begin(), name.end());

  raw.push_back(0);
  raw.push_back(14); // MINFO
  raw.push_back(0);
  raw.push_back(1);
  raw.insert(raw.end(), {0, 0, 0, 10});

  auto rmailbx = encode_name("admin.example.com");
  auto emailbx = encode_name("errors.example.com");

  std::vector<uint8_t> rdata;
  rdata.insert(rdata.end(), rmailbx.begin(), rmailbx.end());
  rdata.insert(rdata.end(), emailbx.begin(), emailbx.end());

  raw.push_back((rdata.size() >> 8) & 0xFF);
  raw.push_back(rdata.size() & 0xFF);
  raw.insert(raw.end(), rdata.begin(), rdata.end());

  ResponseDecoder d(raw);
  REQUIRE(d.consume_header());

  auto rr = d.consume_resource_record();
  REQUIRE(rr);

  REQUIRE(rr.value().rdata.holds<RDataMailInfo>());
}
