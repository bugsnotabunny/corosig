#include "corosig/io/dns/Protocol.hpp"

#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"

#include <array>
#include <bit>
#include <climits>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>

namespace {

using namespace corosig;
using namespace corosig::dns;
using namespace corosig::dns::detail;

static_assert(sizeof(uint8_t) * CHAR_BIT == 8, "Platforms with wide uint8_t are not supported");
static_assert(sizeof(uint16_t) * CHAR_BIT == 16, "Platforms with wide uint16_t are not supported");
static_assert(std::ranges::range<DomainName>);

constexpr std::string_view LABEL_LEN_TOO_BIG_MSG = "Label exeeds 63 octets size limit";
constexpr std::string_view NAME_LEN_TOO_BIG_MSG = "Domain name exeeds 255 octets size limit";

template <std::input_iterator IN, std::unsigned_integral INT>
constexpr IN read_ntoh(IN in, INT &value) noexcept {
  value = 0;
  for (size_t i = 0; i < sizeof(value); ++i) {
    value <<= CHAR_BIT;
    value |= INT(*in++);
  }
  return in;
}

static_assert([]() -> bool {
  uint16_t n;
  read_ntoh(std::array<uint8_t, 2>{0xC8, 0x67}.begin(), n);
  return n == 0xC867;
}());

constexpr std::optional<CompressionPointer>
decode_compression_ptr(std::span<uint8_t const> input) noexcept {
  if (input.size() < 2) {
    return std::nullopt;
  }

  CompressionPointer result{.offset = 0};

  read_ntoh(input.begin(), result.offset);

  if ((result.offset & CompressionPointer::MARKER_BITS) != CompressionPointer::MARKER_BITS) {
    return std::nullopt;
  }
  result.offset &= ~CompressionPointer::MARKER_BITS;
  return result;
}

constexpr Result<std::span<uint8_t const>, ResponseDecodeError>
decode_label(std::span<uint8_t const> input) noexcept {
  if (input.empty()) {
    return Failure{ResponseDecodeError::LABEL_NO_LEN};
  }

  uint8_t len = input.front();

  if (len > 63) {
    return Failure{ResponseDecodeError::LABEL_LEN_TOO_BIG};
  }

  if (input.size() < uint8_t(len + 1)) { // +1 to count len octet itself
    return Failure{ResponseDecodeError::LABEL_LEN_OOB};
  }

  return std::span<uint8_t const>{input.data() + 1, len};
}

constexpr uint16_t QR = 0b1000000000000000;
constexpr uint16_t OPCODE = 0b0111100000000000;
constexpr uint16_t AA = 0b0000010000000000;
constexpr uint16_t TC = 0b0000001000000000;
constexpr uint16_t RD = 0b0000000100000000;
constexpr uint16_t RA = 0b0000000010000000;
constexpr uint16_t RCODE = 0b0000000000001111;

template <uint16_t FIELD>
  requires(FIELD != 0)
constexpr uint16_t set_bitfield(uint16_t mask, uint8_t value) noexcept {
  constexpr auto W = std::popcount(FIELD);
  assert(value < (1 << W) && "Value should be representible within FIELD width");
  return (mask & ~FIELD) | ((value & ((1 << W) - 1)) << std::countr_zero(FIELD));
}

template <uint16_t FIELD>
  requires(FIELD != 0)
constexpr uint8_t get_bitfield(uint16_t mask) noexcept {
  return (mask & FIELD) >> std::countr_zero(FIELD);
}

enum class Type : uint16_t {
  A = 1,
  AAAA = 28,
  NS = 2,
  MD = 3,
  MF = 4,
  CNAME = 5,
  SOA = 6,
  MB = 7,
  MG = 8,
  MR = 9,
  TNULL = 10,
  WKS = 11,
  PTR = 12,
  HINFO = 13,
  MINFO = 14,
  MX = 15,
  TXT = 16,
};

struct ParseDomainNameSuccess {
  uint8_t bytes_to_consume;
  DomainName domain_name;
};

constexpr Result<ParseDomainNameSuccess, ResponseDecodeError>
parse_domain_name(std::span<uint8_t const> original_message,
                  std::span<uint8_t const> label_position) noexcept {
  uint8_t const *domain_name_start = label_position.data();
  bool first_ptr = true;
  uint8_t bytes_to_consume = 0;
  size_t domain_name_len = 0;
  while (!label_position.empty()) {
    CompressionPointer first{.offset = uint16_t(-1)};
    while (std::optional ptr = decode_compression_ptr(label_position)) {
      if (ptr->offset > original_message.size() - 1) {
        return Failure{ResponseDecodeError::COMPRESSION_PTR_OOB};
      }
      if (ptr == first) {
        return Failure{ResponseDecodeError::COMPRESSION_PTR_INFINITE_LOOP};
      }

      if (first_ptr) {
        bytes_to_consume += 2;
        first = *ptr;
        first_ptr = false;
      }

      label_position = original_message.subspan(ptr->offset);
    }

    COROSIG_TRY(std::span label, decode_label(label_position));
    uint8_t label_size_with_len = label.size() + 1;

    domain_name_len += label_size_with_len;

    if (domain_name_len > FQDN_MAX_OCTET_LEN) {
      return Failure{ResponseDecodeError::NAME_LEN_TOO_BIG};
    }

    if (first_ptr) {
      bytes_to_consume += label_size_with_len;
    }

    if (!label.empty()) {
      label_position = label_position.subspan(label_size_with_len);
      continue;
    }

    return ParseDomainNameSuccess{
        .bytes_to_consume = bytes_to_consume,
        .domain_name = DomainName{original_message.data(), domain_name_start},
    };
  }

  return Failure{ResponseDecodeError::NO_ROOT_LABEL};
}

constexpr std::optional<CharacterString>
parse_character_string(std::span<uint8_t const> string_raw) noexcept {
  if (string_raw.empty()) {
    return std::nullopt;
  }
  size_t len = string_raw.front();
  if (len + 1 > string_raw.size()) {
    return std::nullopt;
  }

  return CharacterString{.data = string_raw.subspan(1, len)};
}

constexpr std::optional<uint16_t> consume_2_octets(std::span<uint8_t const> &data) noexcept {
  if (data.size() < 2) {
    return std::nullopt;
  }

  uint16_t result;
  read_ntoh(data.begin(), result);
  data = data.subspan(2);
  return result;
}

constexpr std::optional<uint32_t> consume_4_octets(std::span<uint8_t const> &data) noexcept {
  if (data.size() < 4) {
    return std::nullopt;
  }

  uint32_t result;
  read_ntoh(data.begin(), result);
  data = data.subspan(4);
  return result;
}

constexpr Result<RDataIpv4, ResponseDecodeError>
parse_rdata_ipv4(std::span<uint8_t const> rdata_raw) noexcept {
  if (rdata_raw.size() != 4) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_A};
  }

  std::array<uint8_t, 4> bytes;
  std::ranges::copy(rdata_raw, bytes.begin());
  return RDataIpv4{.addr = Ipv4Addr::from_bytes(bytes)};
}

constexpr Result<RDataIpv6, ResponseDecodeError>
parse_rdata_ipv6(std::span<uint8_t const> rdata_raw) noexcept {
  if (rdata_raw.size() != 16) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_AAAA};
  }

  std::array<uint8_t, 16> bytes;
  std::ranges::copy(rdata_raw, bytes.begin());
  return RDataIpv6{.addr = Ipv6Addr::from_bytes(bytes)};
}

template <typename RDATA, ResponseDecodeError ERROR>
constexpr Result<RDATA, ResponseDecodeError>
parse_rdata_one_domain_name(std::span<uint8_t const> original_message,
                            std::span<uint8_t const> rdata_raw) noexcept {
  if (auto res = parse_domain_name(original_message, rdata_raw)) {
    return RDATA{res.value().domain_name};
  }
  return Failure{ERROR};
}

constexpr Result<RDataStartOfAuthority, ResponseDecodeError>
parse_rdata_soa(std::span<uint8_t const> original_message,
                std::span<uint8_t const> rdata_raw) noexcept {
  auto mname_res = parse_domain_name(original_message, rdata_raw);
  if (!mname_res) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_SOA};
  }
  rdata_raw = rdata_raw.subspan(mname_res.value().bytes_to_consume);

  auto rname_res = parse_domain_name(original_message, rdata_raw);
  if (!rname_res) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_SOA};
  }
  rdata_raw = rdata_raw.subspan(rname_res.value().bytes_to_consume);

  std::optional serial = consume_4_octets(rdata_raw);
  if (!serial) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_SOA};
  }

  std::optional refresh = consume_4_octets(rdata_raw);
  if (!refresh) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_SOA};
  }

  std::optional retry = consume_4_octets(rdata_raw);
  if (!retry) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_SOA};
  }

  std::optional expire = consume_4_octets(rdata_raw);
  if (!expire) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_SOA};
  }

  std::optional minimum = consume_4_octets(rdata_raw);
  if (!minimum) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_SOA};
  }

  return RDataStartOfAuthority{
      .mname = mname_res.value().domain_name,
      .rname = rname_res.value().domain_name,
      .serial = *serial,
      .refresh_after = seconds{*refresh},
      .retry_after = seconds{*retry},
      .expire_after = seconds{*expire},
      .minimum_ttl = seconds{*minimum},
  };
}

constexpr Result<RDataHardwareInfo, ResponseDecodeError>
parse_rdata_hinfo(std::span<uint8_t const> rdata_raw) noexcept {
  std::optional cpu = parse_character_string(rdata_raw);
  if (!cpu) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_HINFO};
  }
  rdata_raw = rdata_raw.subspan(cpu->data.size() + 1);
  std::optional os = parse_character_string(rdata_raw);
  if (!os) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_HINFO};
  }

  return RDataHardwareInfo{.cpu = *cpu, .os = *os};
}

constexpr Result<RDataMailInfo, ResponseDecodeError>
parse_rdata_minfo(std::span<uint8_t const> original_message,
                  std::span<uint8_t const> rdata_raw) noexcept {
  auto rmailbx_res = parse_domain_name(original_message, rdata_raw);
  if (!rmailbx_res) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_MINFO};
  }
  rdata_raw = rdata_raw.subspan(rmailbx_res.value().bytes_to_consume);

  auto emailbx_res = parse_domain_name(original_message, rdata_raw);
  if (!emailbx_res) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_MINFO};
  }

  return RDataMailInfo{
      .rmailbx = rmailbx_res.value().domain_name,
      .emailbx = emailbx_res.value().domain_name,
  };
}

constexpr Result<RDataMailExchange, ResponseDecodeError>
parse_rdata_mx(std::span<uint8_t const> original_message,
               std::span<uint8_t const> rdata_raw) noexcept {
  std::optional preference = consume_2_octets(rdata_raw);
  if (!preference) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_MINFO};
  }

  auto exchange = parse_domain_name(original_message, rdata_raw);
  if (!exchange) {
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_BAD_MINFO};
  }

  return RDataMailExchange{.exchange = exchange.value().domain_name, .preference = *preference};
}

constexpr Result<RData, ResponseDecodeError>
parse_rdata(std::span<uint8_t const> original_message,
            uint16_t rtype,
            std::span<uint8_t const> rdata_raw) noexcept {
  using enum Type;
  using enum ResponseDecodeError::Value;

  switch (Type(rtype)) {
  case A:
    return parse_rdata_ipv4(rdata_raw);
  case AAAA:
    return parse_rdata_ipv6(rdata_raw);
  case NS:
    return parse_rdata_one_domain_name<RDataNameServer, RESOURCE_RECORD_DATA_BAD_NS>(
        original_message, rdata_raw);
  case CNAME:
    return parse_rdata_one_domain_name<RDataCanonicalName, RESOURCE_RECORD_DATA_BAD_CNAME>(
        original_message, rdata_raw);
  case MB:
    return parse_rdata_one_domain_name<RDataMailbox, RESOURCE_RECORD_DATA_BAD_MB>(original_message,
                                                                                  rdata_raw);
  case MG:
    return parse_rdata_one_domain_name<RDataMailGroup, RESOURCE_RECORD_DATA_BAD_MG>(
        original_message, rdata_raw);
  case MR:
    return parse_rdata_one_domain_name<RDataMailboxRename, RESOURCE_RECORD_DATA_BAD_MR>(
        original_message, rdata_raw);
  case PTR:
    return parse_rdata_one_domain_name<RDataPtr, RESOURCE_RECORD_DATA_BAD_PTR>(original_message,
                                                                               rdata_raw);
  case HINFO:
    return parse_rdata_hinfo(rdata_raw);
  case MINFO:
    return parse_rdata_minfo(original_message, rdata_raw);
  case MX:
    return parse_rdata_mx(original_message, rdata_raw);
  case SOA:
    return parse_rdata_soa(original_message, rdata_raw);
  case TNULL:
    return RDataNull{.anything = rdata_raw};

  case MD:
    [[fallthrough]];
  case MF:
    return RDataObsolete{.type = rtype, .raw = rdata_raw};

  case WKS:
    [[fallthrough]];
  case TXT:
    break;
  }

  return RDataNotYetSupported{.type = rtype, .raw = rdata_raw};
}

} // namespace

namespace corosig::dns {

ResponseDecoder::ResponseDecoder(std::span<uint8_t const> response) noexcept
    : m_original_message{response} {
}

void ResponseDecoder::invalidate() noexcept {
  m_remaining_message = {};
  m_original_message = {};
}

Result<Header, ResponseDecodeError> ResponseDecoder::consume_header() noexcept {
  if (m_remaining_message.size() < 12) {
    return Failure{ResponseDecodeError::WRONG_HEADER_BYTES_COUNT};
  }

  Header result;

  auto it = m_remaining_message.begin();

  it = read_ntoh(it, result.id);
  it = read_ntoh(it, result.flags.value);
  it = read_ntoh(it, result.qdcount);
  it = read_ntoh(it, result.ancount);
  it = read_ntoh(it, result.nscount);
  it = read_ntoh(it, result.arcount);

  m_remaining_message = m_remaining_message.subspan(12);

  return result;
}

Result<DecodeQuestionEntrySuccess, ResponseDecodeError>
ResponseDecoder::consume_question_entry() noexcept {

  Result domain_name = parse_domain_name(m_original_message, m_remaining_message);
  if (!domain_name) {
    invalidate();
    return Failure{domain_name.error()};
  }
  m_remaining_message = m_remaining_message.subspan(domain_name.value().bytes_to_consume);

  std::optional qtype = consume_2_octets(m_remaining_message);
  if (!qtype) {
    invalidate();
    return Failure{ResponseDecodeError::NO_QUESTION_TYPE};
  }

  std::optional qclass = consume_2_octets(m_remaining_message);
  if (!qclass) {
    invalidate();
    return Failure{ResponseDecodeError::NO_QUESTION_CLASS};
  }

  return DecodeQuestionEntrySuccess{
      .name = domain_name.value().domain_name,
      .qtype = QueryType(*qtype),
      .qclass = QueryClass(*qclass),
  };
}

Result<ResourceRecord, ResponseDecodeError> ResponseDecoder::consume_resource_record() noexcept {
  Result domain_name = parse_domain_name(m_original_message, m_remaining_message);
  if (!domain_name) {
    invalidate();
    return Failure{domain_name.error()};
  }
  m_remaining_message = m_remaining_message.subspan(domain_name.value().bytes_to_consume);

  std::optional rtype = consume_2_octets(m_remaining_message);
  if (!rtype) {
    invalidate();
    return Failure{ResponseDecodeError::NO_RESOURCE_RECORD_TYPE};
  }

  std::optional rclass = consume_2_octets(m_remaining_message);
  if (!rclass) {
    invalidate();
    return Failure{ResponseDecodeError::NO_RESOURCE_RECORD_CLASS};
  }

  std::optional ttl = consume_4_octets(m_remaining_message);
  if (!ttl) {
    invalidate();
    return Failure{ResponseDecodeError::NO_RESOURCE_RECORD_TTL};
  }

  std::optional rdlen = consume_2_octets(m_remaining_message);
  if (!rdlen) {
    invalidate();
    return Failure{ResponseDecodeError::NO_RESOURCE_RECORD_DATA_LEN};
  }

  if (rdlen > m_remaining_message.size()) {
    invalidate();
    return Failure{ResponseDecodeError::RESOURCE_RECORD_DATA_LEN_OOB};
  }

  std::span rdata_raw{m_remaining_message.data(), *rdlen};
  m_remaining_message = m_remaining_message.subspan(*rdlen);

  Result rdata = parse_rdata(m_original_message, *rtype, rdata_raw);
  if (!rdata) {
    invalidate();
    return Failure{rdata.error()};
  }

  return ResourceRecord{
      .name = domain_name.value().domain_name,
      .rdata = rdata.value(),
      .ttl = seconds{*ttl},
  };
}

[[nodiscard]] bool Header::Flags::is_response() const noexcept {
  return get_bitfield<QR>(value) == 1;
}

Header::Flags &Header::Flags::set_response(bool v) noexcept {
  value = set_bitfield<QR>(value, uint8_t(v));
  return *this;
}

Header::Flags &Header::Flags::set_opcode(QueryOpcode opcode) noexcept {
  value = set_bitfield<OPCODE>(value, uint8_t(opcode));
  return *this;
}

[[nodiscard]] QueryOpcode Header::Flags::opcode() const noexcept {
  return QueryOpcode(get_bitfield<OPCODE>(value));
}

Header::Flags &Header::Flags::set_authoritative_answer(bool v) noexcept {
  value = set_bitfield<AA>(value, uint8_t(v));
  return *this;
}

[[nodiscard]] bool Header::Flags::authoritative_answer() const noexcept {
  return get_bitfield<AA>(value) == 1;
}

Header::Flags &Header::Flags::set_truncated(bool v) noexcept {
  value = set_bitfield<TC>(value, uint8_t(v));
  return *this;
}

[[nodiscard]] bool Header::Flags::truncated() const noexcept {
  return get_bitfield<TC>(value) == 1;
}

Header::Flags &Header::Flags::set_recursion_desired(bool v) noexcept {
  value = set_bitfield<RD>(value, uint8_t(v));
  return *this;
}

[[nodiscard]] bool Header::Flags::recursion_desired() const noexcept {
  return get_bitfield<RD>(value) == 1;
}

Header::Flags &Header::Flags::set_recursion_available(bool v) noexcept {
  value = set_bitfield<RA>(value, uint8_t(v));
  return *this;
}

[[nodiscard]] bool Header::Flags::recursion_available() const noexcept {
  return get_bitfield<RA>(value) == 1;
}

Header::Flags &Header::Flags::set_rcode(uint8_t v) noexcept {
  value = set_bitfield<RCODE>(value, v);
  return *this;
}

[[nodiscard]] uint8_t Header::Flags::get_rcode() const noexcept {
  return get_bitfield<RCODE>(value);
}

DomainName::LabelsIterator::LabelsIterator(DomainName domain_name) noexcept
    : m_original_message_start{domain_name.m_original_message_start},
      m_label_start{domain_name.m_self_start} {
  adwance();
}

DomainName::LabelsIterator::value_type DomainName::LabelsIterator::operator*() const noexcept {
  assert(*this != std::default_sentinel);
  return std::span<uint8_t const>{m_label_start + 1, *m_label_start};
}

void DomainName::LabelsIterator::adwance() noexcept {
  while (std::optional ptr = decode_compression_ptr({m_label_start, 2})) {
    m_label_start = m_original_message_start + ptr->offset;
  }

  if (*m_label_start == 0) {
    m_label_start = nullptr;
  }
}

DomainName::LabelsIterator &DomainName::LabelsIterator::operator++() noexcept {
  if (*this == std::default_sentinel) {
    return *this;
  }
  m_label_start += *m_label_start + 1;
  adwance();
  return *this;
}

DomainName::LabelsIterator DomainName::LabelsIterator::operator++(int) noexcept {
  auto it = *this;
  ++(*this);
  return it;
}

bool DomainName::LabelsIterator::operator==(std::default_sentinel_t) const noexcept {
  return m_label_start == nullptr;
}

DomainName::DomainName(uint8_t const *original_message_start, uint8_t const *self_start) noexcept
    : m_original_message_start{original_message_start},
      m_self_start{self_start} {
}

std::string_view QuestionEncodeError::description() const noexcept {
  switch (value) {
  case TOO_MANY_QUESTION_ENTRIES:
    return "QDCOUNT is u16 and given amount of entries was not representable by it";
  case TOO_LONG_DOMAIN_NAME:
    return NAME_LEN_TOO_BIG_MSG;
  case TOO_LONG_DOMAIN_NAME_LABEL:
    return LABEL_LEN_TOO_BIG_MSG;
  case EMPTY_LABEL:
    return "Empty labels are not allowed in domain name";
  }
  assert(false && "Should be unreachable");
  return {};
}

std::string_view ResponseDecodeError::description() const noexcept {
  switch (value) {
  case WRONG_HEADER_BYTES_COUNT:
    return "Expected 12 octets of HEADER";
  case COMPRESSION_PTR_OOB:
    return "Compression pointer points out of message bounds";
  case COMPRESSION_PTR_INFINITE_LOOP:
    return "Compression pointer points to itself directly or indirectly";
  case LABEL_NO_LEN:
    return "Expected 1 octet of label len";
  case LABEL_LEN_OOB:
    return "Label is longer than remaining message";
  case LABEL_LEN_TOO_BIG:
    return LABEL_LEN_TOO_BIG_MSG;
  case NO_ROOT_LABEL:
    return "Expected a root label to be at DN end";
  case NO_QUESTION_TYPE:
    return "Expected 2 octets of QTYPE";
  case NO_QUESTION_CLASS:
    return "Expected 2 octets of QCLASS";
  case NAME_LEN_TOO_BIG:
    return NAME_LEN_TOO_BIG_MSG;
  case NO_RESOURCE_RECORD_TYPE:
    return "Expected 2 octets of resource record TYPE";
  case NO_RESOURCE_RECORD_CLASS:
    return "Expected 2 octets of resource record CLASS";
  case NO_RESOURCE_RECORD_TTL:
    return "Expected 4 octets of resource record TTL";
  case NO_RESOURCE_RECORD_DATA_LEN:
    return "Expected 2 octets of resource record RDLENGTH";
  case RESOURCE_RECORD_DATA_LEN_OOB:
    return "Resource record RDLENGTH is bigger than remaining message";
  case RESOURCE_RECORD_DATA_BAD_A:
    return "Error parsing A type RDATA";
  case RESOURCE_RECORD_DATA_BAD_AAAA:
    return "Error parsing AAAA type RDATA";
  case RESOURCE_RECORD_DATA_BAD_NS:
    return "Error parsing NS type RDATA";
  case RESOURCE_RECORD_DATA_BAD_CNAME:
    return "Error parsing CNAME type RDATA";
  case RESOURCE_RECORD_DATA_BAD_SOA:
    return "Error parsing SOA type RDATA";
  case RESOURCE_RECORD_DATA_BAD_MB:
    return "Error parsing MB type RDATA";
  case RESOURCE_RECORD_DATA_BAD_MG:
    return "Error parsing MG type RDATA";
  case RESOURCE_RECORD_DATA_BAD_MR:
    return "Error parsing MR type RDATA";
  case RESOURCE_RECORD_DATA_BAD_PTR:
    return "Error parsing PTR type RDATA";
  case RESOURCE_RECORD_DATA_BAD_HINFO:
    return "Error parsing HINFO type RDATA";
  case RESOURCE_RECORD_DATA_BAD_MINFO:
    return "Error parsing MINFO type RDATA";
  }
  assert(false && "Should be unreachable");
  return {};
}

} // namespace corosig::dns
