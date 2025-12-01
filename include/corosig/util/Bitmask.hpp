#ifndef COROSIG_UTILS_BITMASK_HPP
#define COROSIG_UTILS_BITMASK_HPP

#include <type_traits>

namespace corosig {

namespace detail {

template <typename E>
constexpr auto to_underlying(E x) noexcept {
  return std::underlying_type_t<E>(x);
}

} // namespace detail

/// @brief Define an overload to this struct as std::true_type to enable bitwise operations for
///         your enum type
template <typename E>
  requires(std::is_enum_v<E>)
struct IsBitmask : std::false_type {};

template <typename E>
  requires(std::is_enum_v<E> && IsBitmask<E>::value)
constexpr E operator&(E x, E y) noexcept {
  // NOLINTNEXTLINE after-op enum is not a well-defined value
  return static_cast<E>(detail::to_underlying(x) & detail::to_underlying(y));
}

template <typename E>
  requires(std::is_enum_v<E> && IsBitmask<E>::value)
constexpr E operator|(E x, E y) noexcept {
  // NOLINTNEXTLINE after-op enum is not a well-defined value
  return static_cast<E>(detail::to_underlying(x) | detail::to_underlying(y));
}

template <typename E>
  requires(std::is_enum_v<E> && IsBitmask<E>::value)
constexpr E operator^(E x, E y) noexcept {
  // NOLINTNEXTLINE after-op enum is not a well-defined value
  return static_cast<E>(detail::to_underlying(x) ^ detail::to_underlying(y));
}

template <typename E>
  requires(std::is_enum_v<E> && IsBitmask<E>::value)
constexpr E operator~(E x) noexcept {
  // NOLINTNEXTLINE after-op enum is not a well-defined value
  return static_cast<E>(~detail::to_underlying(x));
}

template <typename E>
  requires(std::is_enum_v<E> && IsBitmask<E>::value)
constexpr E &operator&=(E &x, E y) noexcept {
  x = x & y;
  return x;
}

template <typename E>
  requires(std::is_enum_v<E> && IsBitmask<E>::value)
constexpr E &operator|=(E &x, E y) noexcept {
  x = x | y;
  return x;
}

template <typename E>
  requires(std::is_enum_v<E> && IsBitmask<E>::value)
constexpr E &operator^=(E &x, E y) {
  x = x ^ y;
  return x;
}

} // namespace corosig

#endif
