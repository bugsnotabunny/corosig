#ifndef COROSIG_META_AN_AWAITABLE_HPP
#define COROSIG_META_AN_AWAITABLE_HPP

#include <concepts>
#include <utility>

namespace corosig {

namespace detail {

template <typename T>
concept AnAwaiter = requires(T awaiter) {
  { awaiter.await_ready() } noexcept -> std::convertible_to<bool>;
  // await suspend is skipped since handle type may be restricted from type-erased version
  { awaiter.await_resume() } noexcept;
};

template <typename T>
concept HasMemberCoAwait = requires(T t) {
  { std::move(t).operator co_await() } noexcept -> AnAwaiter;
};

template <typename T>
concept HasNonMemberCoAwait = requires(T t) {
  { operator co_await(std::move(t)) } noexcept -> AnAwaiter;
};

} // namespace detail

template <typename T>
concept AnAwaitable =
    detail::AnAwaiter<T> || detail::HasMemberCoAwait<T> || detail::HasNonMemberCoAwait<T>;

} // namespace corosig

#endif
