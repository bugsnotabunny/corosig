#ifndef COROSIG_TESTING_DNS_HELPERS_HPP
#define COROSIG_TESTING_DNS_HELPERS_HPP

#include <cstdint>
#include <string_view>
#include <vector>

namespace corosig::testing {

inline std::vector<uint8_t>
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

inline std::vector<uint8_t> encode_name(std::string_view name) {
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

static constexpr uint8_t hex_to_int(char c) noexcept {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return c - 'a' + 10;
}

} // namespace corosig::testing

#endif
