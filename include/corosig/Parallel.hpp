#ifndef COROSIG_PARALLEL_HPP
#define COROSIG_PARALLEL_HPP

#include "corosig/Clock.hpp"
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/Sleep.hpp"
#include "corosig/container/Vector.hpp"
#include "corosig/meta/AResult.hpp"
#include "corosig/meta/AnAwaitable.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/util/Variant.hpp"

#include <algorithm>
#include <concepts>
#include <coroutine>
#include <optional>
#include <ranges>
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

} // namespace detail

/// @brief Wait when all futures are ready. Return all of their results
/// @returns Future containing tuple of all awaitable results (void results are replaced with
/// std::monostate)
template <AnAwaitable... AWAITABLE>
Fut<std::tuple<detail::WrapVoid<AwaitResult<AWAITABLE>>...>>
when_all(Reactor &, AWAITABLE &&...awaitables) noexcept {
  co_return std::tuple<detail::WrapVoid<AwaitResult<AWAITABLE>>...>{
      co_await detail::WrapVoidAwaitable<AWAITABLE>{
          resolve_to_awaiter(std::forward<AWAITABLE>(awaitables))}...,
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
/// @returns Future containing tuple of all success values (void results are replaced with
/// std::monostate)
///          or error from first failed awaitable
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

/// @brief Error type raised when a timeout is encountered
struct TimedOutError {
  auto operator<=>(TimedOutError const &) const noexcept = default;

  [[nodiscard]] static std::string_view description() noexcept {
    return "Timed out";
  }
};

/// @brief Wait for awaitable to complete or deadline to expire, whichever comes first
/// @param deadline Absolute time point for timeout
/// @returns Awaiter that resolves to awaitable result if completed before deadline,
///          or TimedOutError if deadline expires first
template <AnAwaitable AWAITABLE>
auto with_deadline(Reactor &r, AWAITABLE &&awaitable, SteadyClock::time_point deadline) noexcept {
  struct WithDeadlineAwaiter {
    WithDeadlineAwaiter(Reactor &r, AWAITABLE &&awaitable, SteadyClock::time_point deadline)
        : m_futs{
              [](Reactor &,
                 AWAITABLE &&awaitable,
                 WithDeadlineAwaiter &promise) -> Fut<void, AllocationError> {
                if constexpr (!std::same_as<void, AwaitResult<AWAITABLE>>) {
                  promise.m_result = co_await std::forward<AWAITABLE>(awaitable);
                } else {
                  co_await std::forward<AWAITABLE>(awaitable);
                  promise.m_result = std::monostate{};
                }
                promise.m_waiting_coro.resume();
                co_return Ok{};
              }(r, std::forward<AWAITABLE>(awaitable), *this),

              [](Reactor &,
                 SteadyClock::time_point deadline,
                 WithDeadlineAwaiter &promise) -> Fut<void, AllocationError> {
                co_await Sleep{deadline};
                if (promise.m_result.template holds<WithDeadlineAwaiter::NotReady>()) {
                  promise.m_result = TimedOutError{};
                  promise.m_waiting_coro.resume();
                }
                co_return Ok{};
              }(r, deadline, *this),
          } {
    }

    WithDeadlineAwaiter(WithDeadlineAwaiter const &) = delete;
    WithDeadlineAwaiter(WithDeadlineAwaiter &&) = delete;
    WithDeadlineAwaiter &operator=(WithDeadlineAwaiter const &) = delete;
    WithDeadlineAwaiter &operator=(WithDeadlineAwaiter &&) = delete;

    bool await_ready() const noexcept {
      return !m_result.template holds<WithDeadlineAwaiter::NotReady>();
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
      m_waiting_coro = h;
    }

    Result<AwaitResult<AWAITABLE>, Error<AllocationError, TimedOutError>> await_resume() noexcept {
      assert(!m_result.template holds<WithDeadlineAwaiter::NotReady>());
      if (m_result.template holds<TimedOutError>()) {
        return Failure{TimedOutError{}};
      }

      if constexpr (!std::same_as<void, AwaitResult<AWAITABLE>>) {
        return Ok{std::move(m_result.template as<AwaitResult<AWAITABLE>>())};
      } else {
        return Ok{};
      }
    }

  private:
    struct NotReady {};

    Variant<NotReady,
            TimedOutError,
            std::conditional_t<std::same_as<void, AwaitResult<AWAITABLE>>,
                               std::monostate,
                               AwaitResult<AWAITABLE>>>
        m_result;
    std::coroutine_handle<> m_waiting_coro = std::noop_coroutine();
    std::array<Fut<void, AllocationError>, 2> m_futs;
  };

  return WithDeadlineAwaiter{r, std::forward<AWAITABLE>(awaitable), deadline};
}

/// @brief Wait for awaitable to complete or timeout to expire, whichever comes first
/// @param duration Timeout duration from current time
/// @returns Awaiter that resolves to awaitable result if completed before timeout,
///          or TimedOutError if timeout expires first
template <AnAwaitable AWAITABLE, typename PERIOD, typename REP>
auto with_deadline(Reactor &r,
                   AWAITABLE &&awaitable,
                   std::chrono::duration<PERIOD, REP> duration) noexcept {
  return with_deadline(r, std::forward<AWAITABLE>(awaitable), SteadyClock::now() + duration);
}

namespace detail {

template <std::ranges::range RANGE, typename LOOP_BODY>
using parallel_foreach_return_type =
    std::invoke_result_t<LOOP_BODY,
                         Reactor &,
                         std::add_lvalue_reference_t<std::ranges::range_value_t<RANGE>>>;

}

/// @brief Launch user-provided loop body for each value in range in parallel fashion
/// @param range Range of values to iterate over
/// @param loop_body Function that returns awaitable result for each value
/// @returns Future that resolves to void if all tasks succeed,
///          or an error from first failed task
template <std::ranges::range RANGE, typename LOOP_BODY>
detail::parallel_foreach_return_type<RANGE, LOOP_BODY>
parallel_foreach(Reactor &r, RANGE &&range, LOOP_BODY &&loop_body) noexcept {
  using Future = detail::parallel_foreach_return_type<RANGE, LOOP_BODY>;
  Vector<Future> tasks{r.allocator()};
  if constexpr (std::ranges::sized_range<RANGE>) {
    COROSIG_CO_TRYV(tasks.reserve(std::ranges::size(range)));
  }

  for (auto &&value : std::forward<RANGE>(range)) {
    COROSIG_CO_TRYV(tasks.push_back(loop_body(r, std::forward<decltype(value)>(value))));
  }

  using AwaitResult = AwaitResult<Future>;
  AwaitResult result = Ok{};

  for (auto &&task : tasks) {
    AwaitResult temp_result = co_await std::move(task);
    if (result.is_ok() && !temp_result.is_ok()) {
      result = std::move(temp_result);
    }
  }

  co_return result;
}

} // namespace corosig

#endif
