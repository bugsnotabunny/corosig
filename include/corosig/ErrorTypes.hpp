#pragma once

#include "corosig/util/Overloaded.hpp"
#include <concepts>
#include <cstring>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>

namespace corosig {

namespace detail {

template <typename E>
concept WithDescription = requires(E const &e) {
  { e.description() } -> std::same_as<char const *>;
};

} // namespace detail

template <typename... TYPES>
struct Error : std::variant<TYPES...> {
private:
  using Base = std::variant<TYPES...>;

public:
  using Base::Base;

  char const *description() const noexcept {
    return visit(Overloaded{
        [](detail::WithDescription auto const &e) { return e.description(); },
        [](auto const &e) { return typeid(std::decay_t<decltype(e)>).name(); },
    });
  }

  template <typename T>
  bool holds() const noexcept {
    return std::holds_alternative<T>(*this);
  }

  template <typename F>
  decltype(auto) visit(F &&f) noexcept {
    return std::visit(std::forward<F>(f), *this);
  }
};

template <typename E1, typename E2>
struct extend_error;

template <typename... TYPES1, typename... TYPES2>
struct extend_error<Error<TYPES1...>, Error<TYPES2...>> {
  using type = Error<TYPES1..., TYPES2...>;
};

template <typename E1, typename E2>
struct extend_error {
  using type = typename extend_error<Error<E1>, Error<E2>>::type;
};

template <typename E1, typename E2>
using extend_error_t = typename extend_error<E1, E2>::type;

struct AllocationError {
  auto operator<=>(const AllocationError &) const noexcept = default;
};

struct SyscallError {
  auto operator<=>(const SyscallError &) const noexcept = default;

  static SyscallError current() noexcept;

  char const *description() const noexcept;

  int value;
};

} // namespace corosig
