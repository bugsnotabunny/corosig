#ifndef COROSIG_BACKGROUND_HPP
#define COROSIG_BACKGROUND_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/Yield.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <boost/mp11/algorithm.hpp>
#include <coroutine>
#include <utility>

namespace corosig {

namespace detail {

struct BackgroundCoroutinePromiseType;

}

struct [[nodiscard("Check if task was spawned successfully")]] BackgroundTask
    : Result<void, AllocationError> {

  using promise_type = detail::BackgroundCoroutinePromiseType;

  using Result::Result;

  /// @note even though technically copying here works fine, some users may think that copying task
  /// creates a new coroutine which is wrong. If you need copy, then cast this object to Result
  BackgroundTask(const BackgroundTask &) = delete;

  /// @note even though technically copying here works fine, some users may think that copying task
  /// creates a new coroutine which is wrong. If you need copy, then cast this object to Result
  BackgroundTask &operator=(const BackgroundTask &) = delete;

  BackgroundTask(BackgroundTask &&) = default;
  BackgroundTask &operator=(BackgroundTask &&) = default;
};

namespace detail {

struct BackgroundCoroutinePromiseType : CoroListNode {
  BackgroundCoroutinePromiseType(Reactor &reactor, NotReactor auto const &...) noexcept
      : m_reactor{reactor} {
  }

  BackgroundCoroutinePromiseType(NotReactor auto const &,
                                 Reactor &reactor,
                                 NotReactor auto const &...) noexcept
      : m_reactor{reactor} {
  }

  BackgroundCoroutinePromiseType(const BackgroundCoroutinePromiseType &) = delete;
  BackgroundCoroutinePromiseType(BackgroundCoroutinePromiseType &&) = delete;
  BackgroundCoroutinePromiseType &operator=(const BackgroundCoroutinePromiseType &) = delete;
  BackgroundCoroutinePromiseType &operator=(BackgroundCoroutinePromiseType &&) = delete;

  static void *operator new(size_t n, Reactor &reactor, NotReactor auto const &...) noexcept {

    return reactor.allocator().allocate(n, alignof(std::max_align_t));
  }

  static void *operator new(size_t n,
                            NotReactor auto const &,
                            Reactor &reactor,
                            NotReactor auto const &...) noexcept {
    return reactor.allocator().allocate(n, alignof(std::max_align_t));
  }

  static void *operator new(size_t n,
                            std::align_val_t align,
                            Reactor &reactor,
                            NotReactor auto const &...) noexcept {
    return reactor.allocator().allocate(n, size_t(align));
  }

  static void *operator new(size_t n,
                            std::align_val_t align,
                            NotReactor auto const &,
                            Reactor &reactor,
                            NotReactor auto const &...) noexcept {
    return reactor.allocator().allocate(n, size_t(align));
  }

  static void operator delete(void *) noexcept {
    // Should forward to the reactor's allocator, but we don't have reactor context here
    // This is problematic - consider alternative design
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

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static BackgroundTask get_return_object() noexcept {
    return Ok{};
  }

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static BackgroundTask get_return_object_on_allocation_failure() noexcept {
    return Failure{AllocationError{}};
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
  static auto initial_suspend() noexcept {
    // all background tasks are not executed right away to make in impossible for their lifetime to
    // be strictly nested within caller's and thus break allocator on final_suspend
    return Yield{};
  }

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static auto final_suspend() noexcept {
    struct DestroySelf {
      static bool await_ready() noexcept {
        return false;
      }

      static void
      await_suspend(std::coroutine_handle<BackgroundCoroutinePromiseType> self) noexcept {
        Reactor &reactor = self.promise().m_reactor;
        void *addr = self.address();
        self.destroy();
        reactor.allocator().deallocate(addr);
      }

      static void await_resume() noexcept {
      }
    };

    return DestroySelf{};
  }

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void return_void() noexcept {
  }

private:
  std::coroutine_handle<> coro_from_this() noexcept override {
    return std::coroutine_handle<BackgroundCoroutinePromiseType>::from_promise(*this);
  }

  Reactor &m_reactor;
};

} // namespace detail

template <typename AWAITABLE>
BackgroundTask run_in_background(Reactor &, AWAITABLE awaitable) noexcept {
  (void)co_await std::move(awaitable);
}

} // namespace corosig

#endif
