#ifndef COROSIG_CONTAINER_AN_ALLOCATOR_HPP
#define COROSIG_CONTAINER_AN_ALLOCATOR_HPP

#include "corosig/meta/AResult.hpp"
#include "corosig/meta/AlwaysOkResult.hpp"

#include <type_traits>

namespace corosig {

/// @brief Tell if an object has .clone() method
template <typename T>
concept WithClone = requires(T const &object) {
  { object.clone() } -> AResult;
};

/// @brief Tell if an object is Copyable in some way
template <typename T>
concept Copyable = WithClone<T> != std::is_nothrow_copy_constructible_v<T>;

/// @brief Clone value with it's copy ctor, if it is noexcept, or, via .clone() method
struct CloneFn {
  template <Copyable T>
  auto operator()(T const &value) const noexcept {
    if constexpr (std::is_nothrow_copy_constructible_v<T>) {
      return AlwaysOkResult<T>{T{value}};
    } else {
      return value.clone();
    }
  }
};

inline constexpr CloneFn clone; // NOLINT (readability-identifier-naming)

} // namespace corosig

#endif
