#ifndef COROSIG_CONTAINER_AN_ALLOCATOR_HPP
#define COROSIG_CONTAINER_AN_ALLOCATOR_HPP

#include "corosig/Result.hpp"
#include "corosig/meta/AlwaysOkResult.hpp"
#include "corosig/meta/AnInstanceOf.hpp"

#include <type_traits>

namespace corosig {

template <typename T>
concept WithClone = requires(T const object) {
  { object.clone() } -> AnInstanceOf<Result>;
};

template <typename T>
concept Copyable = WithClone<T> != std::is_nothrow_copy_constructible_v<T>;

template <Copyable T>
auto clone(T const &value) noexcept {
  if constexpr (std::is_nothrow_copy_constructible_v<T>) {
    return AlwaysOkResult<T>{T{value}};
  } else {
    return value.clone();
  }
}

} // namespace corosig

#endif
