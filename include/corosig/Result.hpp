#ifndef COROSIG_RESULT_HPP
#define COROSIG_RESULT_HPP

#include <cassert>
#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>

namespace corosig {

/// @brief Type sugar to make it explicit when a Result is constructed holding a value. An
///         unavoidable thing when you want to construct result holding void
template <typename T>
struct Ok {
  Ok(T &&value) noexcept
      : value{std::forward<T>(value)} {
  }

  T &&value;
};

template <>
struct Ok<void> {};

template <typename T>
Ok(T const &) -> Ok<T const &>;

Ok() -> Ok<void>;

/// @brief Type sugar for constructing a Result with error inside. This is the only way to make an
///         erroneous Results because it makes it clearly visible in code
template <typename T>
struct Failure {
  Failure(T &&value) noexcept
      : value{std::forward<T>(value)} {
  }
  T &&value;
};

template <typename T>
Failure(T const &) -> Failure<T const &>;

/// @brief A value or an error
template <typename R, typename E>
struct [[nodiscard]] Result {
  struct Nullopt {};
  struct Void {};

  using WrapVoidR = std::conditional_t<std::same_as<R, void>, Void, R>;

public:
  Result() noexcept = default;

  /// @brief Construct a result holding a value
  template <typename T>
    requires(!std::same_as<T, void> && std::convertible_to<T, R>)
  Result(Ok<T> &&success) noexcept
      : m_value{std::in_place_type<WrapVoidR>, std::forward<T>(success.value)} {
  }

  /// @brief Construct a result holding void value
  template <typename T>
    requires(std::same_as<T, void>)
  Result(Ok<T> &&) noexcept
      : m_value{std::in_place_type<WrapVoidR>} {
  }

  /// @brief Construct a result holding an error
  template <typename T>
    requires(std::convertible_to<T, E>)
  Result(Failure<T> &&failure) noexcept
      : m_value{std::in_place_type<E>, std::forward<T>(failure.value)} {
  }

  /// @brief Convert one result type to another. Only available if value in other result is
  ///         convertible to this's value and if an error from another result is convirtible to
  ///         this's error
  template <typename R1, typename E1>
    requires(!std::same_as<R, R1> || !std::same_as<E, E1>)
  Result(Result<R1, E1> &&other) noexcept
      : Result{converting_ctor_impl(std::forward<Result<R1, E1>>(other))} {
  }

  /// @brief Construct a result holding a value
  template <typename T>
    requires(!std::same_as<Result, T> && std::convertible_to<T, R>)
  Result(T &&value) noexcept
      : Result{Ok{std::forward<T>(value)}} {
  }

  /// @brief Metaprogramming stuff. Tell if this result always has a value inside
  static consteval bool is_always_ok() noexcept {
    return false;
  }

  /// @brief Tell if this result has a value inside
  explicit constexpr operator bool() const noexcept {
    return is_ok();
  }

  /// @brief Tell if this result has no result or error inside. General-purpose code shall not
  ///         check this and rely on the fact that there are something inside of result. This handle
  ///         is here only for optimization purposes and to reuse same result class in places where
  ///         value may not be available right away
  [[nodiscard]] constexpr bool is_nothing() const noexcept {
    return std::holds_alternative<Nullopt>(m_value);
  }

  /// @brief Tell if this result has a value inside
  [[nodiscard]] constexpr bool is_ok() const noexcept {
    return std::holds_alternative<WrapVoidR>(m_value);
  }

  /// @brief Return a reference to an underlying value
  /// @warning Is UB if this->is_ok() == false
  [[nodiscard]] constexpr WrapVoidR const &value() const noexcept
    requires(!std::same_as<void, R>)
  {
    return value_impl(*this);
  }

  /// @brief Return a reference to an underlying value
  /// @warning Is UB if this->is_ok() == false
  [[nodiscard]] constexpr WrapVoidR &value() noexcept
    requires(!std::same_as<void, R>)
  {
    return value_impl(*this);
  }

  /// @brief Return a reference to an underlying error
  /// @warning Is UB if this->is_ok() == true
  constexpr E const &error() const noexcept {
    return error_impl(*this);
  }

  /// @brief Return a reference to an underlying error
  /// @warning Is UB if this->is_ok() == true
  constexpr E &error() noexcept {
    return error_impl(*this);
  }

private:
  static constexpr auto &value_impl(auto &&self) noexcept {
    assert(self.is_ok() && "Incorrect Result access as error");
    return *std::get_if<WrapVoidR>(&self.m_value);
  }

  static constexpr auto &error_impl(auto &&self) noexcept {
    assert(!self.is_ok() && "Incorrect Result access as error");
    return *std::get_if<E>(&self.m_value);
  }

  template <typename R1, typename E1>
  static Result<R, E> converting_ctor_impl(Result<R1, E1> &&other) noexcept {
    if (!other.is_ok()) {
      return Failure{std::move(other.error())};
    }

    if constexpr (std::same_as<void, R1>) {
      return Ok{};
    } else {
      return Ok{std::move(other.value())};
    }
  }

  std::variant<Nullopt, WrapVoidR, E> m_value;
};

} // namespace corosig

#define COROSIG_TRY_UNIQUE_NAME2(NAME, LINE) NAME##LINE
#define COROSIG_TRY_UNIQUE_NAME1(NAME, LINE) COROSIG_TRY_UNIQUE_NAME2(NAME, LINE)
#define COROSIG_TRY_UNIQUE_NAME(NAME) COROSIG_TRY_UNIQUE_NAME1(NAME, __LINE__)

#define COROSIG_TRY_IMPL(NAME, TEMPORARY_NAME, RETURN, ...)                                        \
  auto TEMPORARY_NAME = __VA_ARGS__;                                                               \
  if constexpr (!decltype(TEMPORARY_NAME)::is_always_ok()) {                                       \
    if (!TEMPORARY_NAME.is_ok()) {                                                                 \
      RETURN ::corosig::Failure{::std::move(TEMPORARY_NAME.error())};                              \
    }                                                                                              \
  }                                                                                                \
  NAME = ::std::move(TEMPORARY_NAME.value())

/// @brief Assign a value from __VA_ARGS__ into NAME. If error occurs, return it
#define COROSIG_TRY(NAME, ...)                                                                     \
  COROSIG_TRY_IMPL(NAME, COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, __VA_ARGS__)

/// @brief Use with coroutines. Assign a value from __VA_ARGS__ into NAME. If error occurs, return
///         it
#define COROSIG_CO_TRY(NAME, ...)                                                                  \
  COROSIG_TRY_IMPL(NAME, COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), co_return, __VA_ARGS__)

#define COROSIG_TRYV_IMPL(TEMPORARY_NAME, RETURN, ...)                                             \
  do {                                                                                             \
    auto TEMPORARY_NAME = __VA_ARGS__;                                                             \
    if constexpr (!decltype(TEMPORARY_NAME)::is_always_ok()) {                                     \
      if (!TEMPORARY_NAME.is_ok()) {                                                               \
        RETURN ::corosig::Failure{::std::move(TEMPORARY_NAME.error())};                            \
      }                                                                                            \
    }                                                                                              \
  } while (false)

/// @brief Drop a value from __VA_ARGS__. If error occurs, return it
#define COROSIG_TRYV(...)                                                                          \
  COROSIG_TRYV_IMPL(COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, __VA_ARGS__)

/// @brief Use with coroutines. Drop a value from __VA_ARGS__. If error occurs, return it
#define COROSIG_CO_TRYV(...)                                                                       \
  COROSIG_TRYV_IMPL(COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), co_return, __VA_ARGS__)

#define COROSIG_TRYT_IMPL(NAME, TEMPORARY_NAME, RETURN, TYPE, ...)                                 \
  auto TEMPORARY_NAME = __VA_ARGS__;                                                               \
  if constexpr (!decltype(TEMPORARY_NAME)::is_always_ok()) {                                       \
    if (!TEMPORARY_NAME.is_ok()) {                                                                 \
      RETURN static_cast<TYPE>(::corosig::Failure{::std::move(TEMPORARY_NAME.error())});           \
    }                                                                                              \
  }                                                                                                \
  NAME = ::std::move(TEMPORARY_NAME.value())

/// @brief  Assign a value from __VA_ARGS__ into NAME. If error occurs, cast it to TYPE and return
///          it. Use this macro with functions returning auto
#define COROSIG_TRYT(TYPE, NAME, ...)                                                              \
  COROSIG_TRYT_IMPL(                                                                               \
      NAME, COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, TYPE, __VA_ARGS__)

#define COROSIG_TRYTV_IMPL(TEMPORARY_NAME, RETURN, TYPE, ...)                                      \
  do {                                                                                             \
    auto TEMPORARY_NAME = __VA_ARGS__;                                                             \
    if constexpr (!decltype(TEMPORARY_NAME)::is_always_ok()) {                                     \
      if (!TEMPORARY_NAME.is_ok()) {                                                               \
        RETURN static_cast<TYPE>(::corosig::Failure{::std::move(TEMPORARY_NAME.error())});         \
      }                                                                                            \
    }                                                                                              \
  } while (false)

/// @brief Drop a value from __VA_ARGS__. If error occurs, cast it to TYPE and return it. Use this
///         macro with functions returning auto
#define COROSIG_TRYTV(TYPE, ...)                                                                   \
  COROSIG_TRYTV_IMPL(COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, TYPE, __VA_ARGS__)

#endif
