#ifndef COROSIG_UTIL_VARIANT_HPP
#define COROSIG_UTIL_VARIANT_HPP

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/bind.hpp>
#include <concepts>
#include <variant>

namespace corosig {

/// @brief A variant class with more convenient interface
template <typename... ALTS>
struct Variant : std::variant<ALTS...> {
private:
  template <typename... ALTS_OTHER>
  struct IsSameListButShuffled {
    constexpr static bool value = // NOLINT readability-identifier-naming
        !std::same_as<boost::mp11::mp_list<ALTS...>, boost::mp11::mp_list<ALTS_OTHER...>> &&
        boost::mp11::mp_all_of_q<boost::mp11::mp_list<ALTS_OTHER...>,
                                 boost::mp11::mp_bind_front<boost::mp11::mp_contains,
                                                            boost::mp11::mp_list<ALTS...>>>::value;
  };

public:
  using std::variant<ALTS...>::variant;

  template <typename... ALTS_OTHER>
    requires IsSameListButShuffled<ALTS_OTHER...>::value
  constexpr Variant(Variant<ALTS_OTHER...> rhs) noexcept
      : Variant{convert_other_variant(std::move(rhs))} {
  }

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

  template <typename T>
    requires(std::same_as<ALTS, T> || ...)
  constexpr bool operator==(T const &value) noexcept {
    return holds<T>() && as<T>() == value;
  }

private:
  template <typename... ALTS_OTHER>
  constexpr static Variant<ALTS...> convert_other_variant(Variant<ALTS_OTHER...> &&rhs) noexcept {
    return std::move(rhs).visit([]<typename T>(T &underlying_value) {
      return Variant<ALTS...>{std::move(underlying_value)};
    });
  }

  template <typename... FS>
  struct Overloaded : FS... {
    using FS::operator()...;
  };
};

} // namespace corosig

#endif
