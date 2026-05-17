#ifndef COROSIG_RESULT_HPP
#define COROSIG_RESULT_HPP

#include "corosig/meta/AResult.hpp" // IWYU pragma: keep (used in macro)

#include <cassert>
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace corosig {

/// @brief Type sugar to make it explicit when a Result is constructed holding a value. An
///         unavoidable thing when you want to construct result holding void
template <typename T>
struct Ok {
  constexpr Ok(T &&value) noexcept
      : value{std::forward<T>(value)} {
  }

  T &&value;
};

template <>
struct Ok<void> {};

template <typename T>
Ok(T const &) -> Ok<T const &>;

template <typename T>
Ok(T &&) -> Ok<T &&>;

Ok() -> Ok<void>;

/// @brief Type sugar for constructing a Result with error inside. This is the only way to make an
///         erroneous Results because it makes it clearly visible in code
template <typename T>
struct Failure {
  constexpr Failure(T &&value) noexcept
      : value{std::forward<T>(value)} {
  }

  T &&value;
};

template <typename T>
Failure(T const &) -> Failure<T const &>;

template <typename T>
Failure(T &&) -> Failure<T &&>;

/// @brief A value or an error
template <typename R, typename E>
struct [[nodiscard]] Result {
  struct EStorage {
    E err;
  };
  struct Nullopt {};
  struct Void {};

  using RWrapVoid = std::conditional_t<std::same_as<R, void>, Void, R>;
  using RStorage = std::conditional_t<std::is_reference_v<R>,
                                      std::reference_wrapper<std::remove_reference_t<R>>,
                                      RWrapVoid>;

public:
  constexpr Result() noexcept = default;

  /// @brief Construct a result holding a value
  template <typename T>
    requires(!std::same_as<T, void> && std::convertible_to<T, R>)
  constexpr Result(Ok<T> &&success) noexcept
      : m_value{std::in_place_type<RStorage>, std::forward<T>(success.value)} {
  }

  /// @brief Construct a result holding void value
  template <typename T>
    requires(std::same_as<T, void>)
  constexpr Result(Ok<T> &&) noexcept
      : m_value{std::in_place_type<RStorage>} {
  }

  /// @brief Construct a result holding an error
  template <typename T>
    requires(std::convertible_to<T, E>)
  constexpr Result(Failure<T> &&failure) noexcept
      : m_value{std::in_place_type<EStorage>, std::forward<T>(failure.value)} {
  }

  /// @brief Convert one result type to another. Only available if value in other result is
  ///         convertible to this's value and if an error from another result is convirtible to
  ///         this's error
  template <typename RESULT>
    requires(!std::same_as<Result, std::decay_t<RESULT>> && AResult<RESULT>)
  constexpr Result(RESULT &&other) noexcept
      : Result{converting_ctor_impl(std::forward<RESULT>(other))} {
  }

  /// @brief Construct a result holding a value
  template <typename T>
    requires(!std::same_as<Result, std::decay_t<T>> && !AResult<T> && std::convertible_to<T, R>)
  constexpr Result(T &&value) noexcept
      : Result{Ok{std::forward<T>(value)}} {
  }

  /// @brief Tell if this result has a value inside
  [[nodiscard]] constexpr operator bool() const noexcept {
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
    return std::holds_alternative<RStorage>(m_value);
  }

  /// @brief Return a reference to an underlying value
  /// @warning Is UB if this->is_ok() == false
  constexpr void value() const & noexcept
    requires(std::same_as<void, R>)
  {
    (void)value_impl(*this);
  }

  /// @brief Return a reference to an underlying value
  /// @warning Is UB if this->is_ok() == false
  [[nodiscard]] constexpr RWrapVoid const &value() const & noexcept
    requires(!std::same_as<void, R>)
  {
    return value_impl(*this);
  }

  /// @brief Return a reference to an underlying value
  /// @warning Is UB if this->is_ok() == false
  [[nodiscard]] constexpr RWrapVoid &value() & noexcept
    requires(!std::same_as<void, R>)
  {
    return value_impl(*this);
  }

  /// @brief Return a reference to an underlying value
  /// @warning Is UB if this->is_ok() == false
  [[nodiscard]] constexpr RWrapVoid &&value() && noexcept
    requires(!std::same_as<void, R>)
  {
    return value_impl(std::move(*this));
  }

  /// @brief Return a reference to an underlying error
  /// @warning Is UB if this->is_ok() == true
  [[nodiscard]] constexpr E const &error() const & noexcept {
    return error_impl(*this);
  }

  /// @brief Return a reference to an underlying error
  /// @warning Is UB if this->is_ok() == true
  [[nodiscard]] constexpr E &error() & noexcept {
    return error_impl(*this);
  }

  /// @brief Return a reference to an underlying error
  /// @warning Is UB if this->is_ok() == true
  [[nodiscard]] constexpr E &&error() && noexcept {
    return error_impl(std::move(*this));
  }

private:
  static constexpr auto &&value_impl(auto &&self) noexcept {
    assert(self.is_ok() && "Incorrect Result access as value");
    auto *ptr = std::get_if<RStorage>(&self.m_value);
    if constexpr (std::is_rvalue_reference_v<decltype(self)>) {
      return std::move(*ptr);
    } else {
      return *ptr;
    }
  }

  static constexpr auto &&error_impl(auto &&self) noexcept {
    assert(!self.is_ok() && "Incorrect Result access as error");
    auto *ptr = std::get_if<EStorage>(&self.m_value);
    if constexpr (std::is_rvalue_reference_v<decltype(self)>) {
      return std::move(ptr->err);
    } else {
      return ptr->err;
    }
  }

  template <AResult RESULT>
  constexpr static Result<R, E> converting_ctor_impl(RESULT &&other_result) noexcept {
    if constexpr (!result_is_always_ok<RESULT>()) {
      if (!other_result.is_ok()) {
        return Failure{std::forward<RESULT>(other_result).error()};
      }
    }

    if constexpr (std::same_as<void, R>) {
      return Ok{};
    } else {
      return Ok{std::forward<RESULT>(other_result).value()};
    }
  }

  std::variant<Nullopt, RStorage, EStorage> m_value;
};

} // namespace corosig

#define COROSIG_TRY_UNIQUE_NAME2(NAME, LINE) NAME##LINE
#define COROSIG_TRY_UNIQUE_NAME1(NAME, LINE) COROSIG_TRY_UNIQUE_NAME2(NAME, LINE)
#define COROSIG_TRY_UNIQUE_NAME(NAME) COROSIG_TRY_UNIQUE_NAME1(NAME, __LINE__)

#define COROSIG_TRY_IMPL(NAME, TEMPORARY_NAME, RETURN, ...)                                        \
  auto TEMPORARY_NAME = __VA_ARGS__;                                                               \
  if constexpr (!result_is_always_ok<decltype(TEMPORARY_NAME)>()) {                                \
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
    if constexpr (!result_is_always_ok<decltype(TEMPORARY_NAME)>()) {                              \
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
  if constexpr (!result_is_always_ok<decltype(TEMPORARY_NAME)>()) {                                \
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
    if constexpr (!result_is_always_ok<decltype(TEMPORARY_NAME)>()) {                              \
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
