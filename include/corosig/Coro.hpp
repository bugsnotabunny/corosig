#ifndef COROSIG_CORO_HPP
#define COROSIG_CORO_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/reactor/SleepList.hpp"
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
  /// @brief Construct new coroutine promise bound to reactor
  /// @note For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  CoroutinePromiseType(Reactor &reactor, NotReactor auto const &...) noexcept
      : m_reactor{reactor} {
  }

  /// @brief Construct new coroutine promise bound to reactor. This overload is used when some
  ///         object's method is declared as coroutine
  /// @note For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  CoroutinePromiseType(NotReactor auto const &,
                       Reactor &reactor,
                       NotReactor auto const &...) noexcept
      : CoroutinePromiseType{reactor} {
  }

  CoroutinePromiseType(const CoroutinePromiseType &) = delete;
  CoroutinePromiseType(CoroutinePromiseType &&) = delete;
  CoroutinePromiseType &operator=(const CoroutinePromiseType &) = delete;
  CoroutinePromiseType &operator=(CoroutinePromiseType &&) = delete;

  ~CoroutinePromiseType() override = default;

  /// @brief Allocate new coroutine frame using allocator from reactor
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void *operator new(size_t n, Reactor &reactor, NotReactor auto const &...) noexcept {
    return reactor.allocator().allocate(n);
  }

  /// @brief Allocate new coroutine frame using allocator from reactor. This overload is used when
  ///         some object's method is declared as coroutine
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void *operator new(size_t n,
                            NotReactor auto const &,
                            Reactor &reactor,
                            NotReactor auto const &...) noexcept {
    return reactor.allocator().allocate(n);
  }

  /// @brief Noop
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void operator delete(void *) noexcept {
    // nothing to do in here since reactor is not accessible. instead, a coro frame is released when
    // future is destroyed
  }

  /// @brief Add this as a CoroListNode into reactor to be executed later
  void yield_to_reactor() noexcept {
    m_reactor.schedule(*this);
  }

  /// @brief Add this SleepListNode into reactor to be executed later, when time comes
  void queue_to_reactor(SleepListNode &node) noexcept {
    m_reactor.schedule_when_time_passes(node);
  }

  /// @brief Add this PollListNode into reactor to be executed later, when event becomes awailable
  void poll_to_reactor(PollListNode &node) noexcept {
    m_reactor.schedule_when_ready(node);
  }

  /// @brief Call an abort. Corosig expects no exceptions to be thrown around since they are not
  ///         safe to throw in sighandlers.
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  [[noreturn]] static void unhandled_exception() noexcept {
    std::abort();
  }

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  Fut<T, E> get_return_object() noexcept;
  static Fut<T, E> get_return_object_on_allocation_failure() noexcept;

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  auto initial_suspend() noexcept {
    return std::suspend_never{};
  }

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
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

  /// @brief Return a value form coroutine
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  template <std::convertible_to<Result<T, E>> U>
  void return_value(U &&value) noexcept {
    assert(m_value.is_nothing());
    assert(!m_waiting_coro.done());
    m_value = Result<T, E>{std::forward<U>(value)};
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

/// @brief A result, which will become available in the future
template <typename T = void, typename E = AllocationError>
struct [[nodiscard("forgot to await?")]] Fut {
  /// @note C++20 coroutine's required typedef. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  using promise_type = detail::CoroutinePromiseType<T, E>;

  Fut(const Fut &) = delete;
  Fut(Fut &&rhs) noexcept = default;
  Fut &operator=(const Fut &) = delete;
  Fut &operator=(Fut &&rhs) noexcept = default;

  ~Fut() {
    if (m_handle.value != nullptr) {
      Reactor &reactor = promise().m_reactor;
      m_handle.value.destroy();
      reactor.allocator().deallocate(m_handle.value.address());
    }
  }

  /// @brief Tell if future is already available. General-purpose code shall not use this method
  ///         and just simply co_await a future
  [[nodiscard]] bool completed() const noexcept {
    return m_handle.value == nullptr || !promise().m_value.is_nothing();
  }

  /// @brief Run reactor's event loop until this future is ready
  Result<T, extend_error<E, SyscallError>> block_on() && noexcept {
    while (!completed()) {
      COROSIG_TRYV(promise().m_reactor.do_event_loop_iteration());
    }
    if (!m_handle.value) {
      return Failure{AllocationError{}};
    }
    return std::move(promise().m_value);
  }

  /// @brief Run reactor's event loop until this future is ready and there is no more tasks in
  ///        reactor
  Result<T, extend_error<E, SyscallError>> block_on_with_reactor_drain() && noexcept {
    Result result = std::move(*this).block_on();
    while (promise().m_reactor.has_active_tasks()) {
      COROSIG_TRYV(promise().m_reactor.do_event_loop_iteration());
    }
    return result;
  }

  /// @brief Await for result inside this future to become available
  auto operator co_await() && noexcept {
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
      Awaiter(Fut &future) noexcept
          : m_future{future} {
      }

      Fut &m_future;
    };

    return Awaiter{*this};
  }

private:
  Fut(std::coroutine_handle<promise_type> handle) noexcept
      : m_handle{handle} {
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
