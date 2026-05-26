#ifndef COROSIG_UTIL_ENDIANNESS
#define COROSIG_UTIL_ENDIANNESS

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <concepts>
#include <span>
#include <type_traits>

namespace corosig {

namespace detail {

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
// std::byteswap did not make it into c++20
constexpr T byteswap(T value) noexcept {
  static_assert(std::has_unique_object_representations_v<T>, "T shall not have padding bits");
  auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  std::ranges::reverse(value_representation);
  return std::bit_cast<T>(value_representation);
}

} // namespace detail

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
constexpr T betoh(T value) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return detail::byteswap(value);
  } else if (std::endian::native == std::endian::big) {
    return value;
  } else {
    static_assert(false, "Unsupported byte order");
  }
}

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
constexpr T letoh(T value) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return value;
  } else if (std::endian::native == std::endian::big) {
    return detail::byteswap(value);
  } else {
    static_assert(false, "Unsupported byte order");
  }
}

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
constexpr T htole(T value) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return value;
  } else if (std::endian::native == std::endian::big) {
    return betole(value);
  } else {
    static_assert(false, "Unsupported byte order");
  }
}

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
constexpr T htobe(T value) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return detail::byteswap(value);
  } else if (std::endian::native == std::endian::big) {
    return value;
  } else {
    static_assert(false, "Unsupported byte order");
  }
}

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
constexpr T hton(T value) noexcept {
  return htobe(value);
}

template <typename T>
  requires(std::integral<T> || std::is_enum_v<T>)
constexpr T ntoh(T value) noexcept {
  return betoh(value);
}

} // namespace corosig

#endif
