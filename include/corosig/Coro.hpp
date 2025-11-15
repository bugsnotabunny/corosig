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
#include <optional>
#include <utility>

namespace corosig {

template <typename T, typename E>
struct Fut;

namespace detail {

template <typename T>
concept NotReactor = !std::same_as<Reactor, T>;

template <typename T, typename E>
struct CoroutinePromiseType : CoroListNode {

  CoroutinePromiseType(Reactor &reactor, NotReactor auto const &...) noexcept
      // reuse m_out to pass reactor to future to avoid creating additional buffer
      : m_reactor{reactor} {
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
    // NOLINTBEGIN false positives about m_out being uninitialized
    assert(m_out);
    assert(!m_out->m_value);
    assert(!m_waiting_coro.done());
    m_out->m_value.emplace(std::forward<U>(value));
    // NOLINTEND
  }

  template <std::convertible_to<T> T2, std::convertible_to<E> E2>
  void return_value(Result<T2, E2> &&result) noexcept {
    assert(m_out);
    assert(!m_out->m_value);
    if (result.has_value()) {
      if constexpr (std::same_as<void, T>) {
        m_out->m_value.emplace(success());
      } else {
        m_out->m_value.emplace(success(std::move(result.assume_value())));
      }
    } else {
      m_out->m_value.emplace(std::move(result.assume_error()));
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
  Fut<T, E> *m_out = nullptr;
};

} // namespace detail

template <typename T = void, typename E = AllocationError>
struct [[nodiscard("forgot to await?")]] Fut {
  using promise_type = detail::CoroutinePromiseType<T, E>;

  Fut(const Fut &) = delete;
  Fut(Fut &&rhs) noexcept
      : m_handle{std::exchange(rhs.m_handle, nullptr)},
        m_value{std::exchange(rhs.m_value, std::nullopt)} {
    if (m_handle) {
      m_handle.promise().m_out = this;
    }
  }

  Fut &operator=(const Fut &) = delete;
  Fut &operator=(Fut &&rhs) noexcept {
    ~Fut();
    new (*this) Fut{std::move(rhs)};
    return *this;
  };

  ~Fut() {
    if (m_handle != nullptr) {
      Reactor &reactor = m_handle.promise().m_reactor;
      m_handle.destroy();
      reactor.free(m_handle.address());
    }
  }

  [[nodiscard]] bool has_value() const noexcept {
    return m_value.has_value();
  }

  Result<T, extend_error<E, SyscallError>> block_on() && noexcept {
    while (!m_value.has_value()) {
      Result res = m_handle.promise().m_reactor.do_event_loop_iteration();
      if (!res) {
        return failure(res.assume_error());
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
      m_future.m_handle.promise().m_waiting_coro = h;
    }

    Result<T, E> await_resume() const noexcept {
      assert(m_future.m_value.has_value());
      return *std::move(m_future.m_value);
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

  Fut(AllocationError e) noexcept : m_value{failure(e)} {
  }

  friend promise_type;

  std::coroutine_handle<promise_type> m_handle = nullptr;
  [[no_unique_address]] std::optional<Result<T, E>> m_value = std::nullopt;
};

template <typename T, typename E>
Fut<T, E> detail::CoroutinePromiseType<T, E>::get_return_object() noexcept {
  Fut<T, E> fut{std::coroutine_handle<CoroutinePromiseType>::from_promise(*this)};
  m_out = &fut;
  return fut;
}

template <typename T, typename E>
Fut<T, E> detail::CoroutinePromiseType<T, E>::get_return_object_on_allocation_failure() noexcept {
  return Fut<T, E>{AllocationError{}};
}

} // namespace corosig

#endif
