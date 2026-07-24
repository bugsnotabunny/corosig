#ifndef COROSIG_REACTOR_DEFAULT_HPP
#define COROSIG_REACTOR_DEFAULT_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/container/Vector.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/GcList.hpp"
#include "corosig/reactor/PollList.hpp"
#include "corosig/reactor/SleepList.hpp"

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/set.hpp>
#include <cstddef>
#include <span>

namespace corosig {

/// @brief A reactor which schedules and resumes coroutines
struct Reactor {
  Reactor(Reactor const &) = delete;
  Reactor(Reactor &&) = delete;
  Reactor &operator=(Reactor const &) = delete;
  Reactor &operator=(Reactor &&) = delete;

  ~Reactor();

  /// @brief Construct a reactor which allocates memory for it's coroutines from provided buffer
  Reactor(std::span<char> mem) noexcept;

  /// @brief Access underlying allocator. Usefull to allocate memory from it for containers
  Allocator &allocator() noexcept;

  /// @brief Schedule a coroutine to be executed
  void schedule(CoroListNode &) noexcept;

  /// @brief Schedule an object to be destroyed later
  void destroy_later(GcListNode &) noexcept;

  /// @brief Schedule a coroutine to be executed when handle recieves specified event
  void schedule_when_ready(PollListNode &) noexcept;

  /// @brief Schedule a coroutine to be executed when specified amount of time passes
  void schedule_when_time_passes(SleepListNode &) noexcept;

  /// @brief Tell if there are any tasks scheduled
  [[nodiscard]] bool has_active_tasks() const noexcept;

  /// @brief Do an event loop iteration possibly making some tasks ready
  Result<void, SyscallError> do_event_loop_iteration() noexcept;

  /// @brief Do an event loop iterations until there are no tasks left
  Result<void, SyscallError> drain_remaining_tasks() noexcept;

  /// @brief A shorthand for calling .allocator().peak_memory()
  [[nodiscard]] size_t peak_memory() const noexcept;

  /// @brief A shorthand for calling .allocator().current_memory()
  [[nodiscard]] size_t current_memory() const noexcept;

  bool &ref_current_coro_was_allocated() noexcept;

private:
  using int_milliseconds_type = std::chrono::duration<int, std::milli>;

  using PollList = boost::intrusive::list<PollListNode,
                                          boost::intrusive::cache_begin<true>,
                                          boost::intrusive::cache_last<true>,
                                          boost::intrusive::constant_time_size<false>,
                                          boost::intrusive::linear<true>>;

  using GcList = boost::intrusive::list<GcListNode,
                                        boost::intrusive::cache_begin<true>,
                                        boost::intrusive::cache_last<false>,
                                        boost::intrusive::constant_time_size<false>,
                                        boost::intrusive::linear<true>>;

  using CoroList = boost::intrusive::list<CoroListNode,
                                          boost::intrusive::cache_begin<true>,
                                          boost::intrusive::cache_last<true>,
                                          boost::intrusive::constant_time_size<false>,
                                          boost::intrusive::linear<true>>;

  using SleepList = boost::intrusive::multiset<SleepListNode,
                                               boost::intrusive::cache_begin<true>,
                                               boost::intrusive::cache_last<false>,
                                               boost::intrusive::constant_time_size<false>>;

  constexpr static auto MIN_POLL_BUFFER = 64;

  void resume_ready_sleepers() noexcept;
  void resume_ready() noexcept;
  void gc() noexcept;

  Result<void, SyscallError> poll_and_resume_fallback(int_milliseconds_type timeout) noexcept;
  Result<void, SyscallError> poll_and_resume_normal(int_milliseconds_type timeout) noexcept;
  Result<void, SyscallError> poll_and_resume_impl(std::span<::pollfd> poll_fds,
                                                  int_milliseconds_type timeout) noexcept;

  static int_milliseconds_type ceil_to_millis(std::chrono::nanoseconds nanos) noexcept;

  GcList m_gc_list;
  PollList m_polled;
  CoroList m_ready;
  SleepList m_sleeping;
  Allocator m_alloc;
  size_t m_previous_iteration_buffer{MIN_POLL_BUFFER};
  Vector<::pollfd> m_poll_buf{m_alloc};
  Result<void, SyscallError> (Reactor::*m_poll_and_resume_method)(int_milliseconds_type);
  bool m_current_coro_was_allocated = false;
};

} // namespace corosig

#endif
