#ifndef COROSIG_CORO_HPP
#define COROSIG_CORO_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <cassert>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <optional>
#include <utility>

namespace corosig {

template <typename T, typename E>
struct Fut;

namespace detail {

template <typename T, typename E>
struct CoroutinePromiseType : CoroListNode {
  CoroutinePromiseType() noexcept = default;

  CoroutinePromiseType(const CoroutinePromiseType &) = delete;
  CoroutinePromiseType(CoroutinePromiseType &&) = delete;
  CoroutinePromiseType &operator=(const CoroutinePromiseType &) = delete;
  CoroutinePromiseType &operator=(CoroutinePromiseType &&) = delete;

  ~CoroutinePromiseType() {
    assert(m_out);
    m_out->m_promise = nullptr;
  }

  template <typename... ARGS>
  static void *operator new(size_t n, ARGS &&...) noexcept {
    return Reactor::instance().allocate_frame(n);
  }

  static void operator delete(void *frame) noexcept {
    return Reactor::instance().free_frame(frame);
  }

  void yield_to_reactor() noexcept {
    Reactor::instance().yield(*this);
  }

  void poll_to_reactor(PollListNode &node) noexcept {
    Reactor::instance().poll(node);
  }

  [[noreturn]] static void unhandled_exception() noexcept {
    std::terminate();
  }

  Fut<T, E> get_return_object() noexcept;
  static Fut<T, E> get_return_object_on_allocation_failure() noexcept;

  auto initial_suspend() noexcept {
    return std::suspend_never{};
  }

  auto final_suspend() noexcept {
    return std::suspend_never{};
  }

  template <std::convertible_to<Result<T, E>> U>
  void return_value(U &&value) noexcept {
    // NOLINTBEGIN false positives about m_out being uninitialized
    assert(m_out);
    m_out->m_value.emplace(std::forward<U>(value));
    m_waiting_coro.resume();
    // NOLINTEND
  }

  template <std::convertible_to<T> T2, std::convertible_to<E> E2>
  void return_value(Result<T2, E2> &&result) noexcept {
    assert(m_out);
    if (result.has_value()) {
      if constexpr (std::same_as<void, T>) {
        m_out->m_value.emplace(success());
      } else {
        m_out->m_value.emplace(success(std::move(result.assume_value())));
      }
    } else {
      m_out->m_value.emplace(std::move(result.assume_error()));
    }
    m_waiting_coro.resume();
  }

private:
  std::coroutine_handle<> coro_from_this() noexcept override {
    return std::coroutine_handle<CoroutinePromiseType>::from_promise(*this);
  }

  friend struct Fut<T, E>;

  std::coroutine_handle<> m_waiting_coro = std::noop_coroutine();
  Fut<T, E> *m_out = nullptr;
};

} // namespace detail

template <typename T = void, typename E = AllocationError>
struct [[nodiscard("forgot to await?")]] Fut {
  using promise_type = detail::CoroutinePromiseType<T, E>;

  Fut(const Fut &) = delete;
  Fut(Fut &&rhs) noexcept
      : m_promise{std::exchange(rhs.m_promise, nullptr)},
        m_value{std::exchange(rhs.m_value, std::nullopt)} {
    if (m_promise) {
      m_promise->m_out = this;
    }
  }

  Fut &operator=(const Fut &) = delete;
  Fut &operator=(Fut &&rhs) noexcept {
    ~Fut();
    new (*this) Fut{std::move(rhs)};
    return *this;
  };

  [[nodiscard]] bool has_value() const noexcept {
    return m_value.has_value();
  }

  Result<T, extend_error<E, SyscallError>> block_on() && noexcept {
    while (!m_value.has_value()) {
      Result res = Reactor::instance().do_event_loop_iteration();
      if (!res) {
        return failure(std::move(res.assume_error()));
      }
    }
    if (m_value->has_value()) {
      if constexpr (std::same_as<void, T>) {
        return success();
      } else {
        return success(std::move(m_value->assume_value()));
      }
    } else {
      return failure(std::move(m_value->error()));
    }
  }

  struct Awaiter {
    Awaiter(const Awaiter &) = delete;
    Awaiter(Awaiter &&) = delete;
    Awaiter &operator=(const Awaiter &) = delete;
    Awaiter &operator=(Awaiter &&) = delete;

    [[nodiscard]] bool await_ready() const noexcept {
      return m_future.has_value();
    }

    void await_suspend(std::coroutine_handle<> h) const noexcept {
      m_future.m_promise->m_waiting_coro = h;
    }

    Result<T, E> await_resume() const noexcept {
      return *std::move(m_future.m_value);
    }

  private:
    friend Fut;
    Awaiter(Fut &future) noexcept : m_future{future} {
    }

    Fut &m_future;
  };

  Awaiter operator co_await() noexcept {
    return Awaiter{*this};
  }

private:
  Fut(promise_type &prom) noexcept : m_promise{&prom} {
  }

  Fut(AllocationError e) noexcept : m_value{failure(e)} {
  }

  friend promise_type;

  promise_type *m_promise = nullptr;

  struct CoroListNode : bi::slist_base_hook<bi::link_mode<bi::link_mode_type::auto_unlink>> {
    virtual std::coroutine_handle<> coro_from_this() noexcept = 0;
  };

  std::optional<Result<T, E>> m_value = std::nullopt;
};

template <typename T, typename E>
Fut<T, E> detail::CoroutinePromiseType<T, E>::get_return_object() noexcept {
  Fut<T, E> fut{*this};
  m_out = &fut;
  return fut;
}

template <typename T, typename E>
Fut<T, E>
detail::CoroutinePromiseType<T, E>::get_return_object_on_allocation_failure() noexcept {
  return Fut<T, E>{AllocationError{}};
}

} // namespace corosig

#endif
