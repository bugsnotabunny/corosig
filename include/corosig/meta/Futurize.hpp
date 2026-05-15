#ifndef COROSIG_META_FUTURIZE_HPP
#define COROSIG_META_FUTURIZE_HPP

#include "corosig/meta/AnAwaitable.hpp"

#include <coroutine>
#include <utility>

namespace corosig {

namespace detail {

template <typename VALUE>
struct ReadyAwaitable {
  explicit constexpr ReadyAwaitable(VALUE v) noexcept
      : value{std::move(v)} {
  }

  ReadyAwaitable(const ReadyAwaitable &) = delete;
  ReadyAwaitable(ReadyAwaitable &&) = delete;
  ReadyAwaitable &operator=(const ReadyAwaitable &) = delete;
  ReadyAwaitable &operator=(ReadyAwaitable &&) = delete;

  constexpr static bool await_ready() noexcept {
    return true;
  }

  constexpr void await_suspend(std::coroutine_handle<>) noexcept {
  }

  constexpr VALUE await_resume() noexcept {
    return std::move(value);
  }

  VALUE value;
};

} // namespace detail

template <typename T>
[[nodiscard]] constexpr decltype(auto) futurize(T &&value) {
  if constexpr (AnAwaitable<T>) {
    return std::forward<T>(value);
  } else {
    return detail::ReadyAwaitable{std::forward<T>(value)};
  }
}

} // namespace corosig

#endif
