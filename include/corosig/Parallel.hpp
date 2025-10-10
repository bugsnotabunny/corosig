#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"

#include <boost/mp11/algorithm.hpp>
#include <optional>

namespace corosig {

template <typename... REACTOR, typename... R, typename... E>
Fut<std::tuple<Result<R, E>...>> when_all(Fut<R, E> &&...futs) noexcept {
  co_return std::tuple{co_await std::move(futs)...};
}

namespace detail {

template <typename ERROR, typename... RESULTS>
std::optional<ERROR> first_error(std::tuple<RESULTS...> &t) noexcept {
  std::optional<ERROR> error_opt = std::nullopt;
  std::apply(
      [&](RESULTS &...current_result) {
        ([&]() -> bool {
          if (current_result.has_error()) {
            error_opt.emplace(std::move(current_result.assume_error()));
            return true;
          }
          return false;
        }() || ...);
      },
      t);
  return error_opt;
}

} // namespace detail

template <typename... REACTOR, typename... R, typename... E>
Fut<std::tuple<R...>, extend_error<E...>> when_all_succeed(Fut<R, E> &&...futs) noexcept {
  Result results_res = co_await when_all(std::move(futs)...);
  if (results_res.has_error()) {
    co_return failure(results_res.assume_error());
  }
  std::tuple<Result<R, E>...> &results = results_res.assume_value();

  if (std::optional first_error = detail::first_error<extend_error<E...>>(results)) {
    co_return failure(std::move(*first_error));
  }

  co_return success(std::apply(
      [](Result<R, E> &&...current_result) {
        return std::tuple<R...>{std::move(current_result.assume_value())...};
      },
      std::move(results)));
}

} // namespace corosig
