#ifndef COROSIG_RESULT_HPP
#define COROSIG_RESULT_HPP

#include <cassert>
#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>

namespace corosig {

template <typename T>
struct Ok {
  Ok(T &&value) noexcept : value{std::forward<T>(value)} {
  }

  T &&value;
};

template <>
struct Ok<void> {};

template <typename T>
Ok(T const &) -> Ok<T const &>;

Ok() -> Ok<void>;

template <typename T>
struct Failure {
  Failure(T &&value) noexcept : value{std::forward<T>(value)} {
  }
  T &&value;
};

template <typename T>
Failure(T const &) -> Failure<T const &>;

template <typename R, typename E>
struct [[nodiscard]] Result {
  struct Nullopt {};
  struct Void {};

  using WrapVoidR = std::conditional_t<std::same_as<R, void>, Void, R>;

public:
  Result() noexcept = default;

  template <typename T>
    requires(!std::same_as<T, void> && std::convertible_to<T, R>)
  Result(Ok<T> &&success) noexcept
      : m_value{std::in_place_type<WrapVoidR>, std::forward<T>(success.value)} {
  }

  template <typename T>
    requires(std::same_as<T, void>)
  Result(Ok<T> &&) noexcept : m_value{std::in_place_type<WrapVoidR>} {
  }

  template <typename T>
    requires(std::convertible_to<T, E>)
  Result(Failure<T> &&failure) noexcept
      : m_value{std::in_place_type<E>, std::forward<T>(failure.value)} {
  }

  template <typename R1, typename E1>
    requires(!std::same_as<R, R1> || !std::same_as<E, E1>)
  Result(Result<R1, E1> &&other) noexcept
      : Result{converting_ctor_impl(std::forward<Result<R1, E1>>(other))} {
  }

  template <typename T>
    requires(!std::same_as<Result, T> && std::convertible_to<T, R>)
  Result(T &&value) noexcept : Result{Ok{std::forward<T>(value)}} {
  }

  static consteval bool is_always_ok() noexcept {
    return false;
  }

  explicit constexpr operator bool() const noexcept {
    return is_ok();
  }

  [[nodiscard]] constexpr bool is_nothing() const noexcept {
    return std::holds_alternative<Nullopt>(m_value);
  }

  [[nodiscard]] constexpr bool is_ok() const noexcept {
    return std::holds_alternative<WrapVoidR>(m_value);
  }

  [[nodiscard]] constexpr WrapVoidR const &value() const noexcept
    requires(!std::same_as<void, R>)
  {
    return value_impl(*this);
  }

  [[nodiscard]] constexpr WrapVoidR &value() noexcept
    requires(!std::same_as<void, R>)
  {
    return value_impl(*this);
  }

  constexpr E const &error() const noexcept {
    return error_impl(*this);
  }

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

#define COROSIG_TRY(NAME, ...)                                                                     \
  COROSIG_TRY_IMPL(NAME, COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, __VA_ARGS__)

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

#define COROSIG_TRYV(...)                                                                          \
  COROSIG_TRYV_IMPL(COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, __VA_ARGS__)

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

#define COROSIG_TRYTV(TYPE, ...)                                                                   \
  COROSIG_TRYTV_IMPL(COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, TYPE, __VA_ARGS__)

#endif
