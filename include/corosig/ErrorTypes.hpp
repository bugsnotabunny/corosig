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

/// @brief An error which contains one of specified errors inside
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

  /// @brief Return an error description. If an underlying error has method .description, it's
  ///        result is returned. Otherwise, it's typename is returned
  [[nodiscard]] std::string_view description() const noexcept {
    return visit(Overloaded{
        [](detail::WithDescription auto const &e) { return e.description(); },
        [](auto const &e) { return std::string_view{typeid(std::decay_t<decltype(e)>).name()}; },
    });
  }

  /// @brief Tell if an error holds a type T inside
  template <typename T>
  [[nodiscard]] bool holds() const noexcept {
    return std::holds_alternative<T>(*this);
  }

  /// @brief Visit all possible errors inside using f as visitor
  template <typename F>
  [[nodiscard]] decltype(auto) visit(F &&f) noexcept {
    return std::visit(std::forward<F>(f), *this);
  }

  /// @brief Visit all possible errors inside using f as visitor
  template <typename F>
  [[nodiscard]] decltype(auto) visit(F &&f) const noexcept {
    return std::visit(std::forward<F>(f), *this);
  }
};

/// @brief An error type that represents that there is no error. Used only for metaprogramming. Any
///         usage in potentially evaluated context makes program ill-formed
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

/// @brief Extend Error type with other options. If E is Error, it's underlying types are used for
///         extension, otherwise E itself is used. Any duplicates in the list are automatically
///         removed to form a list of unique options
template <typename... E>
using extend_error =
    boost::mp11::mp_fold<boost::mp11::mp_list<E...>, Error<>, detail::extend_error>;

/// @brief Get an error type which may be returned by functor CALLABLE when called with ARGS
template <auto CALLABLE, typename... ARGS>
using error_type = std::decay_t<
    decltype(std::declval<std::invoke_result_t<decltype(CALLABLE), ARGS...>>().error())>;

/// @brief An allocation error. Does it really need a description?
struct AllocationError {
  auto operator<=>(const AllocationError &) const noexcept = default;
};

/// @brief An error, propagated from a syscall
struct SyscallError {
  auto operator<=>(const SyscallError &) const noexcept = default;

  /// @brief Get current syscall error. In POSIX this function copies an errno into value
  static SyscallError current() noexcept;

  [[nodiscard]] std::string_view description() const noexcept;

  int value;
};

} // namespace corosig

#endif
