#ifndef COROSIG_TESTING_DNS_HELPERS_HPP
#define COROSIG_TESTING_DNS_HELPERS_HPP

#include <cstdint>
#include <string_view>
#include <vector>

namespace corosig::testing {

std::vector<uint8_t>
make_basic_header(uint16_t id, uint16_t flags, uint16_t qd, uint16_t an, uint16_t ns, uint16_t ar);
std::vector<uint8_t> encode_name(std::string_view name);
uint8_t hex_to_int(char c) noexcept;

} // namespace corosig::testing

#endif
