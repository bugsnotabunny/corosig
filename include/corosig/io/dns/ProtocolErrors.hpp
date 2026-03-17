#ifndef COROSIG_IO_DNS_PROTOCOL_ERROR_HPP
#define COROSIG_IO_DNS_PROTOCOL_ERROR_HPP

#include <cassert>
#include <string_view>

namespace corosig::dns {

struct QuestionEncodeError {
  enum Value {
    TOO_MANY_QUESTION_ENTRIES,
    TOO_LONG_DOMAIN_NAME,
    TOO_LONG_DOMAIN_NAME_LABEL,
    EMPTY_LABEL,
  } value;

  constexpr QuestionEncodeError(Value v)
      : value{v} {
  }

  constexpr auto operator<=>(const QuestionEncodeError &) const noexcept = default;

  [[nodiscard]] std::string_view description() const noexcept;
};

struct ResponseDecodeError {
  enum Value {
    WRONG_HEADER_BYTES_COUNT,
  } value;

  constexpr ResponseDecodeError(Value v)
      : value{v} {
  }

  constexpr auto operator<=>(const ResponseDecodeError &) const noexcept = default;

  [[nodiscard]] std::string_view description() const noexcept;
};

} // namespace corosig::dns

#endif
