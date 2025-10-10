#ifndef COROSIG_UTILS_OVERLOADED_HPP
#define COROSIG_UTILS_OVERLOADED_HPP

namespace corosig {

template <typename... TYPES>
struct Overloaded : TYPES... {
  using TYPES::operator()...;
};

} // namespace corosig

#endif
