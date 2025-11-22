#ifndef COROSIG_META_ALWAYS_OK_RESULT_HPP
#define COROSIG_META_ALWAYS_OK_RESULT_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"

#include <concepts>

namespace corosig {

template <typename R, typename E = void>
struct AlwaysOkResult {
private:
  struct Void {};

  using WrapVoidR = std::conditional_t<std::same_as<R, void>, Void, R>;

public:
  template <typename T>
    requires(!std::same_as<T, void>)
  AlwaysOkResult(Ok<T> &&success) noexcept : m_value{std::forward<T>(success.value)} {
  }

  template <typename T>
    requires(std::same_as<T, void>)
  AlwaysOkResult(Ok<T> &&) noexcept : m_value{} {
  }

  template <typename T>
    requires(!std::same_as<AlwaysOkResult, T>)
  AlwaysOkResult(T &&value) noexcept : AlwaysOkResult{Ok{std::forward<T>(value)}} {
  }

  static consteval bool is_always_ok() noexcept {
    return true;
  }

  explicit constexpr operator bool() const noexcept {
    return is_ok();
  }

  [[nodiscard]] constexpr bool is_nothing() const noexcept {
    return false;
  }

  [[nodiscard]] constexpr bool is_ok() const noexcept {
    return true;
  }

  [[nodiscard]] constexpr WrapVoidR const &value() const noexcept
    requires(!std::same_as<void, R>)
  {
    return m_value;
  }

  [[nodiscard]] constexpr WrapVoidR &value() noexcept
    requires(!std::same_as<void, R>)
  {
    return m_value;
  }

  [[nodiscard]] constexpr static NoError &error() noexcept;

private:
  WrapVoidR m_value;
};

} // namespace corosig

#endif
