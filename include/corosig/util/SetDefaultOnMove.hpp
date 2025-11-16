#ifndef COROSIG_UTILS_SET_DEFAULT_ON_MOVE_HPP
#define COROSIG_UTILS_SET_DEFAULT_ON_MOVE_HPP

#include <utility>

namespace corosig {

template <typename T, auto DEFAULT = T{}>
struct SetDefaultOnMove {
  SetDefaultOnMove() noexcept = default;

  template <typename... ARGS>
  SetDefaultOnMove(ARGS &&...args) noexcept : value{std::forward<ARGS>(args)...} {
  }

  SetDefaultOnMove(SetDefaultOnMove &&rhs) noexcept : value(std::exchange(rhs.value, DEFAULT)) {
  }

  SetDefaultOnMove(const SetDefaultOnMove &) = delete;

  SetDefaultOnMove &operator=(SetDefaultOnMove &&rhs) noexcept {
    this->~SetDefaultOnMove();
    new (this) SetDefaultOnMove(std::move(rhs));
    return *this;
  }

  SetDefaultOnMove &operator=(const SetDefaultOnMove &) = delete;

  T value = DEFAULT;
};

} // namespace corosig

#endif
