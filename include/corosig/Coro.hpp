#pragma once

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Custom.hpp"
#include "corosig/reactor/Default.hpp"

#include <boost/intrusive/list.hpp>
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

template <typename T, typename E, AReactor REACTOR>
struct Fut;

namespace detail {

template <typename T, typename E, AReactor REACTOR>
struct CoroutinePromiseType;

template <typename T, typename E, AReactor REACTOR>
struct CoroutinePromiseReturner {
  template <std::convertible_to<Result<T, E>> U>
  void return_value(U &&value) noexcept {
    assert(impl().m_out);
    impl().m_out->m_value.emplace(std::forward<U>(value));
    impl().m_waiting_coro.resume();
  }

  template <std::convertible_to<T> T2, std::convertible_to<E> E2>
  void return_value(Result<T2, E2> &&value) noexcept {
    assert(impl().m_out);
    if (value.has_value()) {
      impl().m_out->m_value.emplace(std::forward<Result<T2, E2>>(value).assume_value());
    } else {
      impl().m_out->m_value.emplace(std::forward<Result<T2, E2>>(value).assume_error());
    }
    impl().m_waiting_coro.resume();
  }

private:
  auto &impl() noexcept {
    return static_cast<CoroutinePromiseType<T, E, REACTOR> &>(*this);
  }
};

template <typename T, typename E, AReactor REACTOR>
struct CoroutinePromiseType : CoroListNode, CoroutinePromiseReturner<T, E, REACTOR> {
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
    return reactor().allocate_frame(n);
  }

  static void operator delete(void *frame) noexcept {
    return reactor().free_frame(frame);
  }

  static REACTOR &reactor() noexcept {
    return reactor_provider<REACTOR>::engine();
  }

  void yield_to_reactor() noexcept {
    reactor().yield(*this);
  }

  void poll_to_reactor(PollListNode &node) noexcept {
    reactor().poll(node);
  }

  [[noreturn]] static void unhandled_exception() noexcept {
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
  std::coroutine_handle<> coro_from_this() noexcept override {
    return std::coroutine_handle<CoroutinePromiseType>::from_promise(*this);
  }

  friend struct CoroutinePromiseReturner<T, E, REACTOR>;
  friend struct Fut<T, E, REACTOR>;

  std::coroutine_handle<> m_waiting_coro = std::noop_coroutine();
  Fut<T, E, REACTOR> *m_out{nullptr};
};

} // namespace detail

template <typename T, typename E = AllocationError, AReactor REACTOR = Reactor>
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

  Result<T, extend_error_t<E, SyscallError>> block_on() && noexcept {
    while (!m_value.has_value()) {
      Result res = promise_type::reactor().do_event_loop_iteration();
      if (!res) {
        return std::move(res).assume_error();
      }
    }
    if (m_value->has_value()) {
      return std::move(m_value)->assume_value();
    } else {
      return std::move(m_value)->error();
    }
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

  Fut(AllocationError e) noexcept : m_value{e} {
  }

  friend promise_type;
  friend detail::CoroutinePromiseReturner<T, E, REACTOR>;

  promise_type *m_promise = nullptr;

  struct CoroListNode : bi::slist_base_hook<bi::link_mode<bi::link_mode_type::auto_unlink>> {
    virtual std::coroutine_handle<> coro_from_this() noexcept = 0;
  };

  std::optional<Result<T, E>> m_value = std::nullopt;
};

template <typename T, typename E, AReactor REACTOR>
Fut<T, E, REACTOR> detail::CoroutinePromiseType<T, E, REACTOR>::get_return_object() noexcept {
  Fut<T, E, REACTOR> fut{*this};
  m_out = &fut;
  return fut;
}

template <typename T, typename E, AReactor REACTOR>
Fut<T, E, REACTOR>
detail::CoroutinePromiseType<T, E, REACTOR>::get_return_object_on_allocation_failure() noexcept {
  return Fut<T, E, REACTOR>{AllocationError{}};
}

} // namespace corosig
