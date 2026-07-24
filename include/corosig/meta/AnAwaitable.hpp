#ifndef COROSIG_META_AN_AWAITABLE_HPP
#define COROSIG_META_AN_AWAITABLE_HPP

#include <concepts>
#include <utility>

namespace corosig {

/// @brief Concept for awaiter types
template <typename T>
concept AnAwaiter = requires(T awaiter) {
  { awaiter.await_ready() } noexcept -> std::convertible_to<bool>;
  // await suspend is skipped since handle type may be restricted from type-erased version
  { awaiter.await_resume() } noexcept;
};

/// @brief Concept for types with member co_await operator
template <typename T>
concept HasMemberCoAwait = requires(T t) {
  { std::move(t).operator co_await() } noexcept;
};

/// @brief Concept for types with non-member co_await operator
template <typename T>
concept HasNonMemberCoAwait = requires(T t) {
  { operator co_await(std::move(t)) } noexcept;
};

/// @brief Concept for awaitable types
template <typename T>
concept AnAwaitable = AnAwaiter<T> || HasMemberCoAwait<T> || HasNonMemberCoAwait<T>;

namespace detail {

template <AnAwaitable AWAITABLE>
struct ResolveAwaiterType;

template <AnAwaiter AWAITABLE>
struct ResolveAwaiterType<AWAITABLE> {
  using type = AWAITABLE;
};

template <HasMemberCoAwait AWAITABLE>
struct ResolveAwaiterType<AWAITABLE> {
  using type = ResolveAwaiterType<decltype(std::declval<AWAITABLE>().operator co_await())>::type;
};

template <HasNonMemberCoAwait AWAITABLE>
struct ResolveAwaiterType<AWAITABLE> {
  using type = ResolveAwaiterType<decltype(operator co_await(std::declval<AWAITABLE>()))>::type;
};

} // namespace detail

template <AnAwaiter AWAITABLE>
decltype(auto) resolve_to_awaiter(AWAITABLE &&awaitable) noexcept {
  return std::forward<AWAITABLE>(awaitable);
}

template <HasMemberCoAwait AWAITABLE>
decltype(auto) resolve_to_awaiter(AWAITABLE &&awaitable) noexcept {
  return resolve_to_awaiter(std::forward<AWAITABLE>(awaitable).operator co_await());
}

template <HasNonMemberCoAwait AWAITABLE>
decltype(auto) resolve_to_awaiter(AWAITABLE &&awaitable) noexcept {
  return resolve_to_awaiter(operator co_await(std::forward<AWAITABLE>(awaitable)));
}

template <AnAwaitable AWAITABLE>
using AwaitResult = decltype(resolve_to_awaiter(std::declval<AWAITABLE>()).await_resume());

} // namespace corosig

#endif
