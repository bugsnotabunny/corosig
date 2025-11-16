#ifndef COROSIG_PARALLEL_HPP
#define COROSIG_PARALLEL_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace corosig {

template <typename... R, typename... E>
Fut<std::tuple<Result<R, E>...>> when_all(Reactor &, Fut<R, E> &&...futs) noexcept {
  co_return std::tuple{co_await std::move(futs)...};
}

namespace detail {

template <typename ERROR, typename... RESULTS>
std::optional<ERROR> first_error(std::tuple<RESULTS...> &t) noexcept {
  std::optional<ERROR> error_opt = std::nullopt;
  std::apply(
      [&](RESULTS &...current_result) {
        ([&]() -> bool {
          if (!current_result.is_ok()) {
            error_opt.emplace(std::move(current_result.error()));
            return true;
          }
          return false;
        }() || ...);
      },
      t);
  return error_opt;
}

template <typename T>
using WrapVoid = std::conditional_t<std::same_as<void, T>, std::monostate, T>;

} // namespace detail

template <typename... R, typename... E>
Fut<std::tuple<detail::WrapVoid<R>...>, extend_error<E...>>
when_all_succeed(Reactor &, Fut<R, E> &&...futs) noexcept {
  std::tuple<Result<R, E>...> results = std::tuple{co_await std::move(futs)...};

  if (std::optional first_error = detail::first_error<extend_error<E...>>(results)) {
    co_return Failure{std::move(*first_error)};
  }

  co_return std::apply(
      [](Result<R, E> &&...current_result) {
        return std::tuple{[](Result<R, E> &&r) {
          if constexpr (std::same_as<void, R>) {
            return std::monostate{};
          } else {
            return std::move(r.value());
          }
        }(std::move(current_result))...};
      },
      std::move(results));
}

} // namespace corosig

#endif
