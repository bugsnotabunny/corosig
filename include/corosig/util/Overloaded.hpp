#pragma once

namespace corosig {

template <typename... TYPES>
struct Overloaded : TYPES... {
  using TYPES::operator()...;
};

} // namespace corosig
