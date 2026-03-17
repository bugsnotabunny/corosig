#include "corosig/io/dns/Protocol.hpp"

#include <cstdint>

namespace corosig::dns::detail {

Result<detail::Header, ResponseDecodeError> decode_header(std::span<uint8_t const> input) noexcept {
  if (input.size() != 12) {
    return Failure{ResponseDecodeError::WRONG_HEADER_BYTES_COUNT};
  }

  Header result;

  auto it = input.begin();

  result.id = uint16_t(*it++);
  result.id |= uint16_t(*it++) << 8;
  result.id = ntoh(result.id);

  result.flags.flags1 = *it++;
  result.flags.flags2 = *it++;

  result.qdcount = uint16_t(*it++);
  result.qdcount |= uint16_t(*it++) << 8;
  result.qdcount = ntoh(result.qdcount);

  result.ancount = uint16_t(*it++);
  result.ancount |= uint16_t(*it++) << 8;
  result.ancount = ntoh(result.ancount);

  result.nscount = uint16_t(*it++);
  result.nscount |= uint16_t(*it++) << 8;
  result.nscount = ntoh(result.nscount);

  result.arcount = uint16_t(*it++);
  result.arcount |= uint16_t(*it++) << 8;
  result.arcount = ntoh(result.arcount);

  return result;
}

} // namespace corosig::dns::detail
