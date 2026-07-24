#ifndef COROSIG_UTILS_SET_DEFAULT_ON_MOVE_HPP
#define COROSIG_UTILS_SET_DEFAULT_ON_MOVE_HPP

#include <memory>
#include <type_traits>
#include <utility>

namespace corosig {

/// @brief Write default value into moved object when it is moved
/// @details Mostly for usage with trivial types since nontrivial types often provide a reasonable
///          enough move ctor/operator
/// @tparam T Value type
/// @param DEFAULT Default value to set on move (defaults to T{})
template <typename T, auto DEFAULT = T{}>
struct SetDefaultOnMove {
  constexpr SetDefaultOnMove() noexcept = default;

  template <typename... ARGS>
    requires(!std::same_as<std::tuple<SetDefaultOnMove>, std::tuple<std::remove_cvref_t<ARGS>...>>)
  constexpr SetDefaultOnMove(ARGS &&...args) noexcept
      : value{std::forward<ARGS>(args)...} {
  }

  constexpr SetDefaultOnMove(SetDefaultOnMove &&rhs) noexcept
      : value{std::exchange(rhs.value, DEFAULT)} {
  }

  constexpr SetDefaultOnMove(SetDefaultOnMove const &) = delete;

  constexpr SetDefaultOnMove &operator=(SetDefaultOnMove &&rhs) noexcept {
    value = std::exchange(rhs.value, DEFAULT);
    return *this;
  }

  constexpr SetDefaultOnMove &operator=(SetDefaultOnMove const &) = delete;

  constexpr T *operator->() noexcept {
    return std::addressof(value);
  }

  constexpr T const *operator->() const noexcept {
    return std::addressof(value);
  }

  constexpr T &operator*() noexcept {
    return value;
  }

  constexpr T const &operator*() const noexcept {
    return value;
  }

  T value = DEFAULT;
};

} // namespace corosig

#endif
