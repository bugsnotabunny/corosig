#ifndef COROSIG_UTIL_VARIANT_HPP
#define COROSIG_UTIL_VARIANT_HPP

#include <variant>

namespace corosig {

/// @brief A variant class with more convenient interface
template <typename... ALTS>
struct Variant : std::variant<ALTS...> {
  using std::variant<ALTS...>::variant;

  /// @brief Return a reference to chosen alternative
  /// @warning Is UB when variant does not contain chosen alternative
  template <typename T>
  [[nodiscard]] constexpr T &as() noexcept {
    return *std::get_if<T>(this);
  }

  /// @brief Return a reference to chosen alternative
  /// @warning Is UB when variant does not contain chosen alternative
  template <typename T>
  [[nodiscard]] constexpr T const &as() const noexcept {
    return *std::get_if<T>(this);
  }

  /// @brief Tell if variant is chosen alternative
  template <typename T>
  [[nodiscard]] constexpr bool holds() const noexcept {
    return std::holds_alternative<T>(*this);
  }

  /// @brief Visit all possible values inside using f as visitor
  template <typename F>
  [[nodiscard]] constexpr decltype(auto) visit(F &&f) {
    return std::visit(std::forward<F>(f), *this);
  }

  /// @brief Visit all possible values inside using f as visitor
  template <typename F>
  [[nodiscard]] constexpr decltype(auto) visit(F &&f) const {
    return std::visit(std::forward<F>(f), *this);
  }

  /// @brief Visit all possible values inside using f as overloaded visitor
  template <typename... F>
  [[nodiscard]] constexpr decltype(auto) match(F &&...f) {
    return visit(Overloaded{std::forward<F>(f)...});
  }

  /// @brief Visit all possible values inside using f as overloaded visitor
  template <typename... F>
  [[nodiscard]] constexpr decltype(auto) match(F &&...f) const {
    return visit(Overloaded{std::forward<F>(f)...});
  }

private:
  template <typename... FS>
  struct Overloaded : FS... {
    using FS::operator()...;
  };
};

} // namespace corosig

#endif
