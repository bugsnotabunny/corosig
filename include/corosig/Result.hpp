#ifndef COROSIG_RESULT_HPP
#define COROSIG_RESULT_HPP

#include <cassert>
#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>

namespace corosig {

template <typename T>
struct success { // NOLINT
  success(T &&value) : value{std::forward<T>(value)} {
  }

  T &&value;
};

template <>
struct success<void> {};

template <typename T>
success(T const &) -> success<T const &>;

success() -> success<void>;

template <typename T>
struct failure { // NOLINT
  failure(T &&value) : value{std::forward<T>(value)} {
  }
  T &&value;
};

template <typename T>
failure(T const &) -> failure<T const &>;

namespace detail {

struct Nullopt {};
struct Void {};

} // namespace detail

template <typename R, typename E>
struct [[nodiscard]] Result {
  using WrapVoidR = std::conditional_t<std::same_as<R, void>, detail::Void, R>;

public:
  Result() = default;

  template <typename T>
    requires(!std::same_as<T, void>)
  Result(success<T> &&success)
      : m_value{std::in_place_type<WrapVoidR>, std::forward<T>(success.value)} {
  }

  template <typename T>
    requires(std::same_as<T, void>)
  Result(success<T> &&) : m_value{std::in_place_type<WrapVoidR>} {
  }

  template <typename T>
  Result(failure<T> &&failure) : m_value{std::in_place_type<E>, std::forward<T>(failure.value)} {
  }

  template <typename T>
    requires(!std::same_as<Result, T>)
  Result(T &&value) : Result{success{std::forward<T>(value)}} {
  }

  explicit constexpr operator bool() const noexcept {
    return is_ok();
  }

  [[nodiscard]] constexpr bool is_nothing() const noexcept {
    return std::holds_alternative<detail::Nullopt>(m_value);
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

  std::variant<detail::Nullopt, WrapVoidR, E> m_value;
};

} // namespace corosig

#define COROSIG_TRY_UNIQUE_NAME2(name, line) name##line
#define COROSIG_TRY_UNIQUE_NAME1(name, line) COROSIG_TRY_UNIQUE_NAME2(name, line)
#define COROSIG_TRY_UNIQUE_NAME(name) COROSIG_TRY_UNIQUE_NAME1(name, __LINE__)

#define COROSIG_TRY_IMPL(NAME, TEMPORARY_NAME, RETURN, ...)                                        \
  auto TEMPORARY_NAME = __VA_ARGS__;                                                               \
  if (!TEMPORARY_NAME.is_ok()) {                                                                   \
    RETURN ::corosig::failure{std::move(TEMPORARY_NAME.error())};                                  \
  }                                                                                                \
  NAME = std::move(TEMPORARY_NAME.value())

#define COROSIG_TRY(name, ...)                                                                     \
  COROSIG_TRY_IMPL(name, COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, __VA_ARGS__)

#define COROSIG_CO_TRY(name, ...)                                                                  \
  COROSIG_TRY_IMPL(name, COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), co_return, __VA_ARGS__)

#define COROSIG_TRYV_IMPL(TEMPORARY_NAME, RETURN, ...)                                             \
  do {                                                                                             \
    auto TEMPORARY_NAME = __VA_ARGS__;                                                             \
    if (!TEMPORARY_NAME.is_ok()) {                                                                 \
      RETURN ::corosig::failure{std::move(TEMPORARY_NAME.error())};                                \
    }                                                                                              \
  } while (false)

#define COROSIG_TRYV(...)                                                                          \
  COROSIG_TRYV_IMPL(COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), return, __VA_ARGS__)

#define COROSIG_CO_TRYV(...)                                                                       \
  COROSIG_TRYV_IMPL(COROSIG_TRY_UNIQUE_NAME(corosig_temporary_value_), co_return, __VA_ARGS__)

#endif
