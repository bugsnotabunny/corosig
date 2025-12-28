#ifndef COROSIG_SEMAPHORE_HPP
#define COROSIG_SEMAPHORE_HPP

#include "boost/intrusive/intrusive_fwd.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <optional>

namespace corosig {

/// @brief Synchronization primitive which helps to limit application parallelism which is usefull
///        to avoid resources exhaustion
struct Semaphore {

  /// @brief Releases acquired units from semaphore when destroyed
  struct Holder {
    Holder(const Holder &) = delete;
    Holder(Holder &&) = default;
    Holder &operator=(const Holder &) = delete;
    Holder &operator=(Holder &&) = default;

    ~Holder();

    /// @brief Releases acquired units from semaphore on call
    void reset() noexcept;

  private:
    friend Semaphore;
    Holder(Semaphore &semaphore, size_t units) noexcept;

    SetDefaultOnMove<size_t, 0> m_units;
    SetDefaultOnMove<Semaphore *, nullptr> m_semaphore;
  };

  /// @brief   Awaits for specified units count to become available in semaphore
  /// @warning Methods within this type are not really intended to be called directly in user code.
  ///          Prefer sticking to just operator co_await
  struct HolderAwaiter : CoroListNode {
    HolderAwaiter(const HolderAwaiter &) = delete;
    HolderAwaiter(HolderAwaiter &&) = delete;
    HolderAwaiter &operator=(const HolderAwaiter &) = delete;
    HolderAwaiter &operator=(HolderAwaiter &&) = delete;

    ~HolderAwaiter() override = default;

    std::coroutine_handle<> coro_from_this() noexcept override;

    [[nodiscard]] bool await_ready() noexcept;
    void await_suspend(std::coroutine_handle<> h) noexcept;
    [[nodiscard]] Holder await_resume() const noexcept;

  private:
    friend Semaphore;
    HolderAwaiter(Semaphore &semaphore, size_t units) noexcept;

    Semaphore &m_semaphore;
    size_t m_units;
    std::coroutine_handle<> m_waiting_coro = nullptr;
  };

  /// @brief Construct a semaphore with given reactor to schedule tasks with and specified amount of
  ///        maximum units amount
  Semaphore(Reactor &, size_t max_parallelism) noexcept;

  Semaphore(const Semaphore &) = delete;
  Semaphore(Semaphore &&) = delete;
  Semaphore &operator=(const Semaphore &) = delete;
  Semaphore &operator=(Semaphore &&) = delete;

  ~Semaphore();

  /// @brief Tell if requested amount of units could be allocated right now
  [[nodiscard]] bool would_block(size_t units) const noexcept;

  /// @brief Get amount maximum amount of units to be allocated
  [[nodiscard]] size_t max_parallelism() const noexcept;

  /// @brief Get amount of currently allocated units
  [[nodiscard]] size_t current_parallelism() const noexcept;

  /// @brief Immediately get Holder for specified units count or nullopt if such amount is not
  ///        awailable right now
  [[nodiscard]] std::optional<Holder> try_hold(size_t units) noexcept;

  /// @brief Await for specified units count to become available in semaphore
  [[nodiscard]] HolderAwaiter hold(size_t units) noexcept;

private:
  void take_units(size_t units) noexcept;
  void free_units(size_t units) noexcept;

  using waiters_queue_type = boost::intrusive::slist<HolderAwaiter,
                                                     boost::intrusive::constant_time_size<false>,
                                                     boost::intrusive::linear<true>,
                                                     boost::intrusive::cache_last<true>>;

  size_t m_max_parallelism;
  size_t m_current_parallelism = 0;
  waiters_queue_type m_waiters;
  Reactor &m_reactor;
};

} // namespace corosig

#endif
