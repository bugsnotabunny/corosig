#ifndef COROSIG_ERROR_TYPES_HPP
#define COROSIG_ERROR_TYPES_HPP

#include "corosig/meta/AnInstanceOf.hpp"
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

struct NoError;

namespace detail {

template <typename T>
struct IsNotNoError {
  constexpr static bool value = !std::same_as<T, NoError>;
};

template <typename... ES>
using combined_errors = std::conditional_t<sizeof...(ES) == 1,
                                           boost::mp11::mp_front<boost::mp11::mp_list<ES...>>,
                                           Error<ES...>>;

template <typename... TS>
using filtered_unique_list =
    boost::mp11::mp_filter<IsNotNoError, boost::mp11::mp_unique<boost::mp11::mp_list<TS...>>>;

template <typename, typename>
struct extend_error_impl;

template <typename... E1, typename... E2>
struct extend_error_impl<Error<E1...>, Error<E2...>> {
  using type = boost::mp11::mp_apply<combined_errors, filtered_unique_list<E1..., E2...>>;
};

template <typename E1, typename E2>
using extend_error =
    extend_error_impl<std::conditional_t<AnInstanceOf<E1, Error>, E1, Error<E1>>,
                      std::conditional_t<AnInstanceOf<E2, Error>, E2, Error<E2>>>::type;

} // namespace detail

template <typename... E>
using extend_error =
    boost::mp11::mp_fold<boost::mp11::mp_list<E...>, Error<>, detail::extend_error>;

template <typename... E>
using extend_error =
    boost::mp11::mp_fold<boost::mp11::mp_list<E...>, Error<>, detail::extend_error>;

template <auto CALLABLE, typename... ARGS>
using error_type = std::decay_t<
    decltype(std::declval<std::invoke_result_t<decltype(CALLABLE), ARGS...>>().error())>;

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
