#ifndef COROSIG_META_LAMBDIZE_HPP
#define COROSIG_META_LAMBDIZE_HPP

/// @brief Turn a call of some function into a lambda. Mostly usable to make a template of a
///        function into an actually callable object
/// @note  Internal usage is at minimum since some compilers go crazy from this trick paired with
///        decltype
#define COROSIG_LAMBDIZE(...)                                                                      \
  []<typename... ARGS>(ARGS &&...args) { return __VA_ARGS__(std::forward<ARGS>(args)...); }

#endif
