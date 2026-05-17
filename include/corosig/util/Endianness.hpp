#ifndef COROSIG_UTIL_ENDIANNESS
#define COROSIG_UTIL_ENDIANNESS

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <type_traits>

namespace corosig {

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
constexpr T hton(T value) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    static_assert(std::has_unique_object_representations_v<T>, "INT may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(value_representation);
    return std::bit_cast<T>(value_representation);
  } else if (std::endian::native == std::endian::big) {
    return value;
  } else {
    static_assert(false, "Unsupported byte order");
  }
}

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
constexpr T ntoh(T value) noexcept {
  return hton(value);
}

} // namespace corosig

#endif
