#pragma once

#include "corosig/error_types.hpp"
#include "corosig/reactor/custom.hpp"
#include "corosig/reactor/default.hpp"
#include "corosig/result.hpp"

#include <cassert>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace corosig {

template <typename T, AnError E, AReactor REACTOR>
struct Fut;

namespace detail {

template <typename T, AnError E, AReactor REACTOR>
struct CoroutinePromiseType;

template <typename T, AnError E, AReactor REACTOR>
struct CoroutinePromiseReturner {
  template <std::convertible_to<Result<T, E>> U>
  void return_value(U &&value) noexcept {
    assert(impl().m_out);
    impl().m_out->m_value.emplace(std::forward<U>(value));
  }

private:
  auto &impl() noexcept {
    return static_cast<CoroutinePromiseType<T, E, REACTOR> &>(*this);
  }
};

template <typename T, AnError E, AReactor REACTOR>
struct CoroutinePromiseType : public CoroutinePromiseReturner<T, E, REACTOR> {
  CoroutinePromiseType() noexcept = default;

  CoroutinePromiseType(const CoroutinePromiseType &) = delete;
  CoroutinePromiseType(CoroutinePromiseType &&) = delete;
  CoroutinePromiseType &operator=(const CoroutinePromiseType &) = delete;
  CoroutinePromiseType &operator=(CoroutinePromiseType &&) = delete;

  template <typename... ARGS>
  static void *operator new(size_t n, ARGS &&...) noexcept {
    return reactor_provider<REACTOR>::engine().allocate_frame(n);
  }

  static void operator delete(void *frame) noexcept {
    return reactor_provider<REACTOR>::engine().free_frame(frame);
  }

  [[noreturn]] void unhandled_exception() noexcept {
    std::terminate();
  }

  Fut<T, E, REACTOR> get_return_object() noexcept;
  static Fut<T, E, REACTOR> get_return_object_on_allocation_failure() noexcept;

  auto initial_suspend() noexcept {
    return std::suspend_never{};
  }

  auto final_suspend() noexcept {
    return std::suspend_never{};
  }

private:
  friend struct CoroutinePromiseReturner<T, E, REACTOR>;
  friend struct Fut<T, E, REACTOR>;

  Fut<T, E, REACTOR> *m_out{nullptr};
};

} // namespace detail

template <typename T, AnError E = AllocationError, AReactor REACTOR = Reactor>
struct Fut {
  using promise_type = detail::CoroutinePromiseType<T, E, REACTOR>;

  Fut(const Fut &) = delete;
  Fut(Fut &&rhs) noexcept : m_promise(std::exchange(rhs.m_promise, nullptr)) {
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

  bool has_value() const noexcept {
    return m_value.has_value();
  }

  Result<T, E> block_on() && noexcept {
    while (!m_value.has_value()) {
      Result res = reactor_provider<REACTOR>::engine().do_event_loop_iteration();
      if (!res) {
        return res.error();
      }
    }
    return *std::move(m_value);
  }

  struct Awaiter {
    Awaiter(const Awaiter &) = delete;
    Awaiter(Awaiter &&) = delete;
    Awaiter &operator=(const Awaiter &) = delete;
    Awaiter &operator=(Awaiter &&) = delete;

    bool await_ready() const noexcept {
      return m_future.has_value();
    }
    void await_suspend(std::coroutine_handle<> h) const noexcept {
      // reactor_provider<REACTOR>::engine().
      m_future.m_waiting_coro = h;
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

  Fut(AllocationError e) noexcept : m_value{e} {
  }

  friend promise_type;
  friend detail::CoroutinePromiseReturner<T, E, REACTOR>;

  std::coroutine_handle<> m_waiting_coro = nullptr;
  promise_type *m_promise = nullptr;
  std::optional<Result<T, E>> m_value = std::nullopt;
};

template <typename T, AnError E, AReactor REACTOR>
Fut<T, E, REACTOR> detail::CoroutinePromiseType<T, E, REACTOR>::get_return_object() noexcept {
  Fut<T, E, REACTOR> fut{*this};
  m_out = &fut;
  return fut;
}

template <typename T, AnError E, AReactor REACTOR>
Fut<T, E, REACTOR>
detail::CoroutinePromiseType<T, E, REACTOR>::get_return_object_on_allocation_failure() noexcept {
  return Fut<T>{AllocationError{}};
}

} // namespace corosig
