#pragma once

#include "boost/mp11/algorithm.hpp"
#include "boost/mp11/detail/mp_list.hpp"
#include "boost/mp11/detail/mp_rename.hpp"
#include "corosig/util/Overloaded.hpp"
#include <boost/mp11.hpp>
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

namespace detail {

template <template <typename...> typename TMPL, typename... TS>
using apply_unique =
    boost::mp11::mp_apply<TMPL, boost::mp11::mp_unique<boost::mp11::mp_list<TS...>>>;

template <typename E1, typename E2>
struct extend_error;

template <typename... E1, typename... E2>
struct extend_error<Error<E1...>, Error<E2...>> {
  using type = apply_unique<Error, E1..., E2...>;
};

template <typename E1, typename E2>
struct extend_error {
  using type = extend_error<Error<E1>, Error<E2>>::type;
};

template <typename... E1, typename E2>
struct extend_error<Error<E1...>, E2> {
  using type = extend_error<Error<E1...>, Error<E2>>::type;
};

template <typename E1, typename... E2>
struct extend_error<E1, Error<E2...>> {
  using type = extend_error<Error<E1>, Error<E2...>>::type;
};

} // namespace detail

template <typename E1, typename E2>
using extend_error = typename detail::extend_error<E1, E2>::type;

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
