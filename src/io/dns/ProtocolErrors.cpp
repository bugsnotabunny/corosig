#include "corosig/io/dns/ProtocolErrors.hpp"

#include <string_view>

namespace {

constexpr std::string_view LABEL_LEN_TOO_BIG_MSG = "Label exeeds 63 octets size limit";
constexpr std::string_view NAME_LEN_TOO_BIG_MSG = "Domain name exeeds 255 octets size limit";

} // namespace

namespace corosig::dns {

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
