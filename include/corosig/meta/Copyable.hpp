#ifndef COROSIG_CONTAINER_AN_ALLOCATOR_HPP
#define COROSIG_CONTAINER_AN_ALLOCATOR_HPP

#include "corosig/meta/AResult.hpp"
#include "corosig/meta/AlwaysOkResult.hpp"

#include <type_traits>

namespace corosig {

/// @brief Concept to check if object has .clone() method
template <typename T>
concept WithClone = requires(T const &object) {
  { object.clone() } -> AResult;
};

/// @brief Concept to check if object is copyable via copy ctor or clone()
template <typename T>
concept Copyable = WithClone<T> != std::is_nothrow_copy_constructible_v<T>;

/// @brief Function object to clone values via copy constructor or clone() method
/// @details Uses copy constructor for noexcept copyable types, otherwise uses .clone() method
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
