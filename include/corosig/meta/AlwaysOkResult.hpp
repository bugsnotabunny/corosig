#ifndef COROSIG_META_ALWAYS_OK_RESULT_HPP
#define COROSIG_META_ALWAYS_OK_RESULT_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"

#include <concepts>

namespace corosig {

/// @brief Result which always holds value. Usefull to mimic regular Result
template <typename R, typename E = void>
struct AlwaysOkResult {
private:
  struct Void {};

  using WrapVoidR = std::conditional_t<std::same_as<R, void>, Void, R>;

public:
  /// @brief Construct a result holding a value
  template <typename T>
    requires(!std::same_as<T, void>)
  AlwaysOkResult(Ok<T> &&success) noexcept
      : m_value{std::forward<T>(success.value)} {
  }

  /// @brief Construct a result holding a void value
  template <typename T>
    requires(std::same_as<T, void>)
  AlwaysOkResult(Ok<T> &&) noexcept
      : m_value{} {
  }

  /// @brief Construct a result holding a value
  template <typename T>
    requires(!std::same_as<AlwaysOkResult, T>)
  AlwaysOkResult(T &&value) noexcept
      : AlwaysOkResult{Ok{std::forward<T>(value)}} {
  }

  /// @brief Metaprogramming stuff. Tell if this result always has a value inside
  static consteval bool is_always_ok() noexcept {
    return true;
  }

  /// @brief Tell if this result has a value inside
  explicit constexpr operator bool() const noexcept {
    return is_ok();
  }

  /// @brief Tell if this result has no result or error inside. Always false
  [[nodiscard]] constexpr bool is_nothing() const noexcept {
    return false;
  }

  /// @brief Tell if this result has a value inside. Always true
  [[nodiscard]] constexpr bool is_ok() const noexcept {
    return true;
  }

  /// @brief Return a reference to an underlying value
  [[nodiscard]] constexpr WrapVoidR const &value() const noexcept
    requires(!std::same_as<void, R>)
  {
    return m_value;
  }

  /// @brief Return a reference to an underlying value
  [[nodiscard]] constexpr WrapVoidR &value() noexcept
    requires(!std::same_as<void, R>)
  {
    return m_value;
  }

  /// @brief This method is here to make interfaces uniform with regular Result. When this method is
  ///        used in a potentially-evalutated context, the programm is ill-formed
  [[nodiscard]] constexpr static NoError &error() noexcept;

private:
  WrapVoidR m_value;
};

} // namespace corosig

#endif
