#ifndef COROSIG_REACTOR_DEFAULT_HPP
#define COROSIG_REACTOR_DEFAULT_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/PollList.hpp"

#include <cstddef>

namespace corosig {

/// @brief A thing which does scheduling for stopped coroutines
struct Reactor {
  Reactor(const Reactor &) = delete;
  Reactor(Reactor &&) = delete;
  Reactor &operator=(const Reactor &) = delete;
  Reactor &operator=(Reactor &&) = delete;

  /// @brief Construct a reactor which allocates memory for it's coroutines from provided static
  /// buffer
  template <size_t SIZE>
  Reactor(Allocator::Memory<SIZE> &mem)
      : m_alloc{mem} {
  }

  /// @brief Access underlying allocator. Usefull to allocate memory from it for containers
  Allocator &allocator() noexcept;

  /// @brief Schedule a coroutine to be executed
  void schedule(CoroListNode &) noexcept;

  /// @brief Schedule a coroutine to be executed when handle recieves specified event
  void schedule_when_ready(PollListNode &) noexcept;

  /// @brief Tell if there are any tasks scheduled
  [[nodiscard]] bool has_active_tasks() const noexcept;

  /// @note It is better to use Fut<...>.block_on() method instead of calling this method directly
  Result<void, SyscallError> do_event_loop_iteration() noexcept;

  /// @brief A shorthand for calling .allocator().peak_memory()
  [[nodiscard]] size_t peak_memory() const noexcept;

  /// @brief A shorthand for calling .allocator().current_memory()
  [[nodiscard]] size_t current_memory() const noexcept;

private:
  PollList m_polled;
  CoroList m_ready;
  Allocator m_alloc;
};

} // namespace corosig

#endif
