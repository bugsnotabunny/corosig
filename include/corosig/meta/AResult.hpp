#ifndef COROSIG_META_A_RESULT_HPP
#define COROSIG_META_A_RESULT_HPP

#include <concepts>

namespace corosig {

template <typename R>
consteval bool result_is_always_ok() noexcept {
  constexpr bool HAS_ALWAYS_OK = requires {
    { R::is_always_ok() } -> std::convertible_to<bool>;
  };

  if constexpr (HAS_ALWAYS_OK) {
    return R::is_always_ok();
  } else {
    return false;
  }
}

template <typename R>
concept AResult = requires(R r) {
  { result_is_always_ok<R>() } -> std::convertible_to<bool>;
  { r.is_ok() } -> std::convertible_to<bool>;
  { r.value() };
  { r.error() };
};

} // namespace corosig

#endif
