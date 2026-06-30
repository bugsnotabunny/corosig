#ifndef COROSIG_PARALLEL_HPP
#define COROSIG_PARALLEL_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/meta/AResult.hpp"
#include "corosig/meta/AnAwaitable.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <coroutine>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace corosig {

namespace detail {

template <typename T>
using WrapVoid = std::conditional_t<std::same_as<void, T>, std::monostate, T>;

template <AnAwaitable AWAITABLE>
struct WrapVoidAwaitable;

template <AnAwaiter AWAITABLE>
struct WrapVoidAwaitable<AWAITABLE> {
  WrapVoidAwaitable(AWAITABLE &&awaitable) noexcept
      : awaitable{std::forward<AWAITABLE>(awaitable)} {
  }

  bool await_ready() const noexcept {
    return awaitable.await_ready();
  }

  template <typename PROMISE>
  decltype(auto) await_suspend(std::coroutine_handle<PROMISE> h) const noexcept {
    return awaitable.await_suspend(h);
  }

  WrapVoid<::corosig::AwaitResult<AWAITABLE>> await_resume() const noexcept {
    if constexpr (std::same_as<void, ::corosig::AwaitResult<AWAITABLE>>) {
      awaitable.await_resume();
      return std::monostate{};
    } else {
      return awaitable.await_resume();
    }
  }

  AWAITABLE &&awaitable;
};

template <HasMemberCoAwait AWAITABLE>
struct WrapVoidAwaitable<AWAITABLE>
    : WrapVoidAwaitable<decltype(std::declval<AWAITABLE>().operator co_await())> {
  WrapVoidAwaitable(AWAITABLE &&awaitable) noexcept
      : WrapVoidAwaitable<decltype(std::declval<AWAITABLE>().operator co_await())>{
            std::forward<AWAITABLE>(awaitable).operator co_await()} {};
};

template <HasNonMemberCoAwait AWAITABLE>
struct WrapVoidAwaitable<AWAITABLE>
    : WrapVoidAwaitable<decltype(std::declval<AWAITABLE>().operator co_await())> {
  WrapVoidAwaitable(AWAITABLE &&awaitable) noexcept
      : WrapVoidAwaitable<decltype(operator co_await(std::declval<AWAITABLE>()))>{operator co_await(
            std::forward<AWAITABLE>(awaitable))} {};
};

} // namespace detail

/// @brief Wait when all futures are ready. Return all of their results
template <AnAwaitable... AWAITABLE>
Fut<std::tuple<detail::WrapVoid<AwaitResult<AWAITABLE>>...>>
when_all(Reactor &, AWAITABLE &&...awaitables) noexcept {
  co_return std::tuple<detail::WrapVoid<AwaitResult<AWAITABLE>>...>{
      co_await detail::WrapVoidAwaitable<AWAITABLE>{std::forward<AWAITABLE>(awaitables)}...,
  };
}

namespace detail {

template <typename... RESULTS>
auto first_error(std::tuple<RESULTS...> &t) noexcept {
  std::optional<::corosig::extend_error<typename RESULTS::failure_type...>> error_opt =
      std::nullopt;
  std::apply(
      [&](RESULTS &...current_result) {
        ([&]() -> bool {
          if (!current_result.is_ok()) {
            error_opt.emplace(std::forward<typename RESULTS::failure_type>(current_result.error()));
            return true;
          }
          return false;
        }() || ...);
      },
      t);
  return error_opt;
}

} // namespace detail

/// @brief Wait when all futures are ready. Return all of values from their results or the first
///         error that occurred among them
template <typename... AWAITABLE>
  requires((AnAwaitable<AWAITABLE> && AResult<AwaitResult<AWAITABLE>>) && ...)
Fut<std::tuple<detail::WrapVoid<typename AwaitResult<AWAITABLE>::ok_type>...>,
    extend_error<typename AwaitResult<AWAITABLE>::failure_type...>>
when_all_succeed(Reactor &, AWAITABLE &&...awaitables) noexcept {
  std::tuple<AwaitResult<AWAITABLE>...> results = std::tuple{co_await std::move(awaitables)...};

  if (std::optional first_error = detail::first_error(results)) {
    co_return Failure{std::move(*first_error)};
  }

  co_return std::apply(
      []<AResult... RESULT>(RESULT &&...current_result) {
        return std::tuple{[](RESULT &&r) {
          if constexpr (std::same_as<void, typename RESULT::ok_type>) {
            return std::monostate{};
          } else {
            return std::forward<decltype(r.value())>(r.value());
          }
        }(std::forward<RESULT>(current_result))...};
      },
      std::move(results));
}

} // namespace corosig

#endif
