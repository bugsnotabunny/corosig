#pragma once

#include <type_traits>
#include <utility>

namespace corosig {

namespace detail {

template <typename E>
constexpr auto to_underlying(E x) noexcept {
  return std::underlying_type_t<E>(x);
}

} // namespace detail

template <typename E>
  requires(std::is_enum_v<E>)
struct is_bitmask : std::false_type {};

template <typename E>
  requires(std::is_enum_v<E> && is_bitmask<E>::value)
constexpr E operator&(E x, E y) noexcept {
  return static_cast<E>(detail::to_underlying(x) & detail::to_underlying(y));
}

template <typename E>
  requires(std::is_enum_v<E> && is_bitmask<E>::value)
constexpr E operator|(E x, E y) noexcept {
  return static_cast<E>(detail::to_underlying(x) | detail::to_underlying(y));
}

template <typename E>
  requires(std::is_enum_v<E> && is_bitmask<E>::value)
constexpr E operator^(E x, E y) noexcept {
  return static_cast<E>(detail::to_underlying(x) ^ detail::to_underlying(y));
}

template <typename E>
  requires(std::is_enum_v<E> && is_bitmask<E>::value)
constexpr E operator~(E x) noexcept {
  return static_cast<E>(~detail::to_underlying(x));
}

template <typename E>
  requires(std::is_enum_v<E> && is_bitmask<E>::value)
constexpr E &operator&=(E &x, E y) noexcept {
  x = x & y;
  return x;
}

template <typename E>
  requires(std::is_enum_v<E> && is_bitmask<E>::value)
constexpr E &operator|=(E &x, E y) noexcept {
  x = x | y;
  return x;
}

template <typename E>
  requires(std::is_enum_v<E> && is_bitmask<E>::value)
constexpr E &operator^=(E &x, E y) {
  x = x ^ y;
  return x;
}

} // namespace corosig
