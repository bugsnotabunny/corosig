#include "corosig/io/dns/ProtocolErrors.hpp"

#include <string_view>

namespace corosig::dns {

std::string_view QuestionEncodeError::description() const noexcept {
  switch (value) {
  case TOO_MANY_QUESTION_ENTRIES:
    return "QDCOUNT is u16 and given amount of entries was not representible by it";
  case TOO_LONG_DOMAIN_NAME:
    return "255 octets is maximum domain name len";
  case TOO_LONG_DOMAIN_NAME_LABEL:
    return "255 octets is maximum domain name label len";
  case EMPTY_LABEL:
    return "Empty labels are not allowed in domain name";
  }
  assert(false && "Should be unreachable");
  return {};
}

std::string_view ResponseDecodeError::description() const noexcept {
  switch (value) {
  case WRONG_HEADER_BYTES_COUNT:
    return "Headers are 12 octets long and given input does not satisfy this";
  }
  assert(false && "Should be unreachable");
  return {};
}

} // namespace corosig::dns
