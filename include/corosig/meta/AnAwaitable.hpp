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
struct AwaitResult;

template <AnAwaiter AWAITABLE>
struct AwaitResult<AWAITABLE> {
  using type = decltype(std::declval<AWAITABLE>().await_resume());
};

template <HasMemberCoAwait AWAITABLE>
struct AwaitResult<AWAITABLE> {
  using type = typename AwaitResult<decltype(std::declval<AWAITABLE>().operator co_await())>::type;
};

template <HasNonMemberCoAwait AWAITABLE>
struct AwaitResult<AWAITABLE> {
  using type = typename AwaitResult<decltype(operator co_await(std::declval<AWAITABLE>()))>::type;
};

} // namespace detail

template <AnAwaitable AWAITABLE>
using AwaitResult = detail::AwaitResult<AWAITABLE>::type;

} // namespace corosig

#endif
