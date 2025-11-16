#ifndef COROSIG_CORO_HPP
#define COROSIG_CORO_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <cassert>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <utility>

namespace corosig {

template <typename T, typename E>
struct Fut;

namespace detail {

template <typename T>
concept NotReactor = !std::same_as<Reactor, T>;

template <typename T, typename E>
struct CoroutinePromiseType : CoroListNode {

  CoroutinePromiseType(Reactor &reactor, NotReactor auto const &...) noexcept : m_reactor{reactor} {
  }

  CoroutinePromiseType(NotReactor auto const &, Reactor &reactor,
                       NotReactor auto const &...) noexcept
      : CoroutinePromiseType{reactor} {
  }

  CoroutinePromiseType(const CoroutinePromiseType &) = delete;
  CoroutinePromiseType(CoroutinePromiseType &&) = delete;
  CoroutinePromiseType &operator=(const CoroutinePromiseType &) = delete;
  CoroutinePromiseType &operator=(CoroutinePromiseType &&) = delete;

  ~CoroutinePromiseType() override = default;

  static void *operator new(size_t n, Reactor &reactor, NotReactor auto const &...) noexcept {
    return reactor.allocate(n);
  }

  static void *operator new(size_t n, NotReactor auto const &, Reactor &reactor,
                            NotReactor auto const &...) noexcept {
    return reactor.allocate(n);
  }

  static void operator delete(void *) noexcept {
    // nothing to do in here since reactor is not accessible. instead, a coro frame is released when
    // future is destroyed
  }

  void yield_to_reactor() noexcept {
    m_reactor.yield(*this);
  }

  void poll_to_reactor(PollListNode &node) noexcept {
    m_reactor.poll(node);
  }

  [[noreturn]] static void unhandled_exception() noexcept {
    std::abort();
  }

  Fut<T, E> get_return_object() noexcept;
  static Fut<T, E> get_return_object_on_allocation_failure() noexcept;

  auto initial_suspend() noexcept {
    return std::suspend_never{};
  }

  auto final_suspend() noexcept {
    struct ReturnControlToCaller {
      static bool await_ready() noexcept {
        return false;
      }

      static auto await_suspend(std::coroutine_handle<CoroutinePromiseType> self) noexcept {
        return self.promise().m_waiting_coro;
      }

      static void await_resume() noexcept {
      }
    };

    // coro frame must be destroyed in Fut dtor so we don't let it fall off the end here
    return ReturnControlToCaller{};
  }

  template <std::convertible_to<Result<T, E>> U>
  void return_value(U &&value) noexcept {
    assert(m_value.is_nothing());
    assert(!m_waiting_coro.done());
    m_value = Result<T, E>{std::forward<U>(value)};
  }

  template <std::convertible_to<T> T2, std::convertible_to<E> E2>
  void return_value(Result<T2, E2> &&result) noexcept {
    assert(m_value.is_nothing());
    if (result.is_ok()) {
      if constexpr (std::same_as<void, T>) {
        m_value = Ok{};
      } else {
        m_value = Ok{std::move(result.value())};
      }
    } else {
      m_value = Failure{std::move(result.error())};
    }

    assert(!m_waiting_coro.done() && "Waiting coro was destroyed before child has finished");
  }

private:
  std::coroutine_handle<> coro_from_this() noexcept override {
    return std::coroutine_handle<CoroutinePromiseType>::from_promise(*this);
  }

  friend struct Fut<T, E>;

  std::coroutine_handle<> m_waiting_coro = std::noop_coroutine();
  Reactor &m_reactor;
  Result<T, E> m_value;
};

} // namespace detail

template <typename T = void, typename E = AllocationError>
struct [[nodiscard("forgot to await?")]] Fut {
  using promise_type = detail::CoroutinePromiseType<T, E>;

  Fut(const Fut &) = delete;
  Fut(Fut &&rhs) noexcept = default;
  Fut &operator=(const Fut &) = delete;
  Fut &operator=(Fut &&rhs) noexcept = default;

  ~Fut() {
    if (m_handle.value != nullptr) {
      Reactor &reactor = promise().m_reactor;
      m_handle.value.destroy();
      reactor.deallocate(m_handle.value.address());
    }
  }

  [[nodiscard]] bool completed() const noexcept {
    return m_handle.value == nullptr || !promise().m_value.is_nothing();
  }

  Result<T, extend_error<E, SyscallError>> block_on() && noexcept {
    while (!completed()) {
      COROSIG_TRYV(promise().m_reactor.do_event_loop_iteration());
    }
    if (!m_handle.value) {
      return Failure{AllocationError{}};
    }
    return std::move(promise().m_value);
  }

  struct Awaiter {
    Awaiter(const Awaiter &) = delete;
    Awaiter(Awaiter &&) = delete;
    Awaiter &operator=(const Awaiter &) = delete;
    Awaiter &operator=(Awaiter &&) = delete;

    [[nodiscard]] bool await_ready() const noexcept {
      return m_future.completed();
    }

    void await_suspend(std::coroutine_handle<> h) const noexcept {
      m_future.promise().m_waiting_coro = h;
    }

    Result<T, E> await_resume() const noexcept {
      if (m_future.m_handle.value == nullptr) {
        return Failure{AllocationError{}};
      }

      assert(!m_future.promise().m_value.is_nothing());
      return std::move(m_future.promise().m_value);
    }

  private:
    friend Fut;
    Awaiter(Fut &future) noexcept : m_future{future} {
    }

    Fut &m_future;
  };

  Awaiter operator co_await() && noexcept {
    return Awaiter{*this};
  }

private:
  Fut(std::coroutine_handle<promise_type> handle) noexcept : m_handle{handle} {
  }

  promise_type &promise() noexcept {
    return m_handle.value.promise();
  }

  promise_type const &promise() const noexcept {
    return m_handle.value.promise();
  }

  friend promise_type;

  SetDefaultOnMove<std::coroutine_handle<promise_type>, nullptr> m_handle;
};

template <typename T, typename E>
Fut<T, E> detail::CoroutinePromiseType<T, E>::get_return_object() noexcept {
  return Fut<T, E>{std::coroutine_handle<CoroutinePromiseType>::from_promise(*this)};
}

template <typename T, typename E>
Fut<T, E> detail::CoroutinePromiseType<T, E>::get_return_object_on_allocation_failure() noexcept {
  return Fut<T, E>{nullptr};
}

} // namespace corosig

#endif
