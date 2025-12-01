#ifndef COROSIG_UTILS_OVERLOADED_HPP
#define COROSIG_UTILS_OVERLOADED_HPP

namespace corosig {

/// @brief A type sugar to make it easier to make a lambda with several overloads for call operator
/// @code
/// auto equal = Overloaded {
///   [](int a, int b) { return a == b; },
///   [](double a, double b) { return std::abs(a - b) <= std::numeric_limits<double>::epsilon(); },
/// };
/// @endcode
template <typename... TYPES>
struct Overloaded : TYPES... {
  using TYPES::operator()...;
};

} // namespace corosig

#endif
