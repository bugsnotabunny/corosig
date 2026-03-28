#ifndef COROSIG_IO_DNS_PROTOCOL_ERROR_HPP
#define COROSIG_IO_DNS_PROTOCOL_ERROR_HPP

#include <cassert>
#include <cstdint>
#include <string_view>

namespace corosig::dns {

struct QuestionEncodeError {
  enum Value : uint8_t {
    TOO_MANY_QUESTION_ENTRIES,
    TOO_LONG_DOMAIN_NAME,
    TOO_LONG_DOMAIN_NAME_LABEL,
    EMPTY_LABEL,
  } value;

  constexpr QuestionEncodeError(Value v) noexcept
      : value{v} {
  }

  constexpr auto operator<=>(const QuestionEncodeError &) const noexcept = default;

  [[nodiscard]] std::string_view description() const noexcept;
};

struct ResponseDecodeError {
  enum Value : uint8_t {
    WRONG_HEADER_BYTES_COUNT,
    COMPRESSION_PTR_OOB,
    COMPRESSION_PTR_INFINITE_LOOP,
    LABEL_NO_LEN,
    LABEL_LEN_OOB,
    LABEL_LEN_TOO_BIG,
    NAME_LEN_TOO_BIG,
    NO_ROOT_LABEL,
    NO_QUESTION_TYPE,
    NO_QUESTION_CLASS,
    NO_RESOURCE_RECORD_TYPE,
    NO_RESOURCE_RECORD_CLASS,
    NO_RESOURCE_RECORD_TTL,
    NO_RESOURCE_RECORD_DATA_LEN,
    RESOURCE_RECORD_DATA_LEN_OOB,
    RESOURCE_RECORD_DATA_BAD_A,
    RESOURCE_RECORD_DATA_BAD_AAAA,
    RESOURCE_RECORD_DATA_BAD_NS,
    RESOURCE_RECORD_DATA_BAD_CNAME,
    RESOURCE_RECORD_DATA_BAD_SOA,
    RESOURCE_RECORD_DATA_BAD_MB,
    RESOURCE_RECORD_DATA_BAD_MG,
    RESOURCE_RECORD_DATA_BAD_MR,
    RESOURCE_RECORD_DATA_BAD_PTR,
    RESOURCE_RECORD_DATA_BAD_HINFO,
    RESOURCE_RECORD_DATA_BAD_MINFO,
  } value;

  constexpr ResponseDecodeError(Value v) noexcept
      : value{v} {
  }

  constexpr auto operator<=>(const ResponseDecodeError &) const noexcept = default;

  [[nodiscard]] std::string_view description() const noexcept;
};

} // namespace corosig::dns

#endif
