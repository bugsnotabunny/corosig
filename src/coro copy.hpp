#include <cassert>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <foonathan/memory/static_allocator.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#pragma once

namespace sigawait {

template <size_t SIZE>
using allocator_storage = foonathan::memory::static_allocator_storage<SIZE>;
using allocator = foonathan::memory::static_allocator;

template <typename T>
struct future;

namespace detail {

template <typename T>
struct coroutine_promise_type;

template <typename T>
struct coroutine_promise_returner {

  template <std::convertible_to<T> U>
  void return_value(U &&value) noexcept {
    assert(impl().m_out);
    impl().m_out->m_value.emplace(std::forward<U>(value));
  }

private:
  auto &impl() noexcept {
    return static_cast<coroutine_promise_type<T> &>(*this);
  }
};

template <typename T>
struct coroutine_promise_type : public coroutine_promise_returner<T> {
  coroutine_promise_type() = default;
  coroutine_promise_type(const coroutine_promise_type &) = delete;
  coroutine_promise_type(coroutine_promise_type &&) = delete;
  coroutine_promise_type &operator=(const coroutine_promise_type &) = delete;
  coroutine_promise_type &operator=(coroutine_promise_type &&) = delete;

  static void *operator new(std::size_t n, allocator &alloc) noexcept {
    std::cout << "YAAA\n";
    alloc.allocate_node(n, alignof(std::max_align_t));
    alloc.deallocate_node(void *, std::size_t, std::size_t)
  }

  static void operator delete(void *) noexcept {}

  [[noreturn]] void unhandled_exception() noexcept { std::terminate(); }
  future<T> get_return_object() noexcept;
  auto initial_suspend() noexcept { return std::suspend_never{}; }
  auto final_suspend() noexcept { return std::suspend_never{}; }

private:
  friend struct coroutine_promise_returner<T>;
  friend struct future<T>;

  future<T> *m_out;
};

} // namespace detail

template <typename T>
struct future {
  using promise_type = detail::coroutine_promise_type<T>;

  future() = default;

  future(const future &) = delete;
  future(future &&rhs) noexcept
      : m_promise(std::exchange(rhs.m_promise, nullptr)) {
    m_promise->m_out = this;
  }

  future &operator=(const future &) = delete;
  future &operator=(future &&) = delete;

  bool has_value() const noexcept { return m_value.has_value(); }

  struct awaiter {
    awaiter(const awaiter &) = delete;
    awaiter(awaiter &&) = delete;
    awaiter &operator=(const awaiter &) = delete;
    awaiter &operator=(awaiter &&) = delete;

    bool await_ready() const noexcept { return m_future.has_value(); }
    void await_suspend(std::coroutine_handle<> h) const noexcept {
      m_future.m_waiting_coro = h;
    }
    T await_resume() const noexcept { return *std::move(m_future.m_value); }

  private:
    friend future;
    awaiter(future &future) : m_future{future} {}
    future &m_future;
  };

  awaiter operator co_await() noexcept { return awaiter{*this}; }

private:
  future(promise_type &prom) noexcept : m_promise{&prom} {}

  friend promise_type;
  friend detail::coroutine_promise_returner<T>;

  std::coroutine_handle<> m_waiting_coro = nullptr;
  promise_type *m_promise = nullptr;
  std::optional<T> m_value = std::nullopt;
};

template <typename T>
future<T> detail::coroutine_promise_type<T>::get_return_object() noexcept {
  future<T> fut{*this};
  m_out = &fut;
  return fut;
}

future<int> coro(size_t) noexcept;

} // namespace sigawait
