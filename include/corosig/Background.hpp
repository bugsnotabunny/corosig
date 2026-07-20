#ifndef COROSIG_BACKGROUND_HPP
#define COROSIG_BACKGROUND_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
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
  BackgroundTask(BackgroundTask const &) = delete;

  /// @note even though technically copying here works fine, some users may think that copying task
  /// creates a new coroutine which is wrong. If you need copy, then cast this object to Result
  BackgroundTask &operator=(BackgroundTask const &) = delete;

  BackgroundTask(BackgroundTask &&) = default;
  BackgroundTask &operator=(BackgroundTask &&) = default;
};

namespace detail {

/// @brief Promise type for background coroutines
struct BackgroundCoroutinePromiseType : CoroListNode {
  BackgroundCoroutinePromiseType(Reactor &reactor, NotReactor auto const &...) noexcept
      : m_reactor{reactor},
        m_needs_dealloc{std::exchange(reactor.ref_current_coro_was_allocated(), false)} {
  }

  BackgroundCoroutinePromiseType(NotReactor auto const &,
                                 Reactor &reactor,
                                 NotReactor auto const &...) noexcept
      : BackgroundCoroutinePromiseType{reactor} {
  }

  BackgroundCoroutinePromiseType(BackgroundCoroutinePromiseType const &) = delete;
  BackgroundCoroutinePromiseType(BackgroundCoroutinePromiseType &&) = delete;
  BackgroundCoroutinePromiseType &operator=(BackgroundCoroutinePromiseType const &) = delete;
  BackgroundCoroutinePromiseType &operator=(BackgroundCoroutinePromiseType &&) = delete;

  ~BackgroundCoroutinePromiseType() override = default;

  /// @brief Destroy and deallocate this coroutine
  void destroy() noexcept override {
    Reactor &reactor = m_reactor;
    auto handle = std::coroutine_handle<BackgroundCoroutinePromiseType>::from_promise(*this);
    void *addr = handle.address();
    bool needs_dealloc = m_needs_dealloc;
    handle.destroy();
    if (needs_dealloc) {
      reactor.allocator().deallocate(addr);
    }
  }

  /// @brief Allocate new coroutine frame using allocator from reactor
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void *operator new(size_t n,
                            std::align_val_t align,
                            Reactor &reactor,
                            NotReactor auto const &...) noexcept {
    assert(reactor.ref_current_coro_was_allocated() == false);
    reactor.ref_current_coro_was_allocated() = true;
    return reactor.allocator().allocate(n, static_cast<size_t>(align));
  }

  /// @brief Allocate new coroutine frame using allocator from reactor. This overload is used when
  ///         some object's method is declared as coroutine
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void *operator new(size_t n,
                            std::align_val_t align,
                            NotReactor auto const &,
                            Reactor &reactor,
                            NotReactor auto const &...) noexcept {
    return BackgroundCoroutinePromiseType::operator new(n, std::align_val_t{align}, reactor);
  }

  /// @brief Allocate new coroutine frame using allocator from reactor
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void *operator new(size_t n, Reactor &reactor, NotReactor auto const &...) noexcept {
    return BackgroundCoroutinePromiseType::operator new(
        n, std::align_val_t{alignof(std::max_align_t)}, reactor);
  }

  /// @brief Allocate new coroutine frame using allocator from reactor. This overload is used when
  ///         some object's method is declared as coroutine
  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void *operator new(size_t n,
                            NotReactor auto const &,
                            Reactor &reactor,
                            NotReactor auto const &...) noexcept {
    return BackgroundCoroutinePromiseType::operator new(
        n, std::align_val_t{alignof(std::max_align_t)}, reactor);
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

  /// @brief Add this PollListNode into reactor to be executed later, when event becomes available
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
    return std::suspend_never{};
  }

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static auto final_suspend() noexcept {
    struct ScheduleSelfDeletion {
      static bool await_ready() noexcept {
        return false;
      }

      static void
      await_suspend(std::coroutine_handle<BackgroundCoroutinePromiseType> self_coro) noexcept {
        BackgroundCoroutinePromiseType &self = self_coro.promise();
        self.m_reactor.destroy_later(self);
      }

      static void await_resume() noexcept {
      }
    };

    return ScheduleSelfDeletion{};
  }

  /// @note C++20 coroutine's required method. For more detailed explanation check
  ///        https://en.cppreference.com/w/cpp/language/coroutines.html
  static void return_void() noexcept {
  }

  /// @brief Cast this object to a resumable coroutine handle
  std::coroutine_handle<> coro_from_this() noexcept override {
    return std::coroutine_handle<BackgroundCoroutinePromiseType>::from_promise(*this);
  }

private:
  Reactor &m_reactor;
  [[no_unique_address]] bool m_needs_dealloc;
};

} // namespace detail

/// @brief Run an awaitable as a background task
/// @returns Background task which shall be checked for allocation errors
template <typename AWAITABLE>
BackgroundTask run_in_background(Reactor &, AWAITABLE awaitable) noexcept {
  (void)co_await std::move(awaitable);
}

} // namespace corosig

#endif
