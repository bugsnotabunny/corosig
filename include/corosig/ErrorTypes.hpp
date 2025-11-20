#ifndef COROSIG_ERROR_TYPES_HPP
#define COROSIG_ERROR_TYPES_HPP

#include "corosig/util/Overloaded.hpp"

#include <boost/mp11.hpp>
#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>

namespace corosig {

namespace detail {

template <typename E>
concept WithDescription = requires(E const &e) {
  { e.description() } noexcept -> std::same_as<std::string_view>;
};

} // namespace detail

template <typename... TYPES>
struct Error : std::variant<TYPES...> {
private:
  static_assert(
      boost::mp11::mp_size<boost::mp11::mp_unique<boost::mp11::mp_list<TYPES...>>>::value ==
          sizeof...(TYPES),
      "Error must contain unique types");

  using Base = std::variant<TYPES...>;

public:
  using Base::Base;

  [[nodiscard]] std::string_view description() const noexcept {
    return visit(Overloaded{
        [](detail::WithDescription auto const &e) { return e.description(); },
        [](auto const &e) { return std::string_view{typeid(std::decay_t<decltype(e)>).name()}; },
    });
  }

  template <typename T>
  [[nodiscard]] bool holds() const noexcept {
    return std::holds_alternative<T>(*this);
  }

  template <typename F>
  [[nodiscard]] decltype(auto) visit(F &&f) noexcept {
    return std::visit(std::forward<F>(f), *this);
  }

  template <typename F>
  [[nodiscard]] decltype(auto) visit(F &&f) const noexcept {
    return std::visit(std::forward<F>(f), *this);
  }
};

namespace detail {

template <template <class...> class, class>
struct IsInstanceOf : std::false_type {};

template <template <class...> class TMPL, class... ARGS>
struct IsInstanceOf<TMPL, TMPL<ARGS...>> : std::true_type {};

template <template <typename...> typename TMPL, typename... TS>
using apply_unique =
    boost::mp11::mp_apply<TMPL, boost::mp11::mp_unique<boost::mp11::mp_list<TS...>>>;

template <typename, typename>
struct extend_error_impl;

template <typename... E1, typename... E2>
struct extend_error_impl<Error<E1...>, Error<E2...>> {
  using type = apply_unique<Error, E1..., E2...>;
};

template <typename E1, typename E2>
using extend_error =
    extend_error_impl<std::conditional_t<IsInstanceOf<Error, E1>::value, E1, Error<E1>>,
                      std::conditional_t<IsInstanceOf<Error, E2>::value, E2, Error<E2>>>::type;

} // namespace detail

template <typename... E>
using extend_error =
    boost::mp11::mp_fold<boost::mp11::mp_list<E...>, Error<>, detail::extend_error>;

struct AllocationError {
  auto operator<=>(const AllocationError &) const noexcept = default;
};

struct SyscallError {
  auto operator<=>(const SyscallError &) const noexcept = default;

  static SyscallError current() noexcept;

  [[nodiscard]] std::string_view description() const noexcept;

  int value;
};

} // namespace corosig

#endif
