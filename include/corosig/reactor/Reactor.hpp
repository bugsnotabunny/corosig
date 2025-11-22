#ifndef COROSIG_REACTOR_DEFAULT_HPP
#define COROSIG_REACTOR_DEFAULT_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/PollList.hpp"

#include <cstddef>

namespace corosig {

struct Reactor {
  Reactor() noexcept = default;

  Reactor(const Reactor &) = delete;
  Reactor(Reactor &&) = delete;
  Reactor &operator=(const Reactor &) = delete;
  Reactor &operator=(Reactor &&) = delete;

  template <size_t SIZE>
  Reactor(Allocator::Memory<SIZE> &mem) : m_alloc{mem} {
  }

  Allocator &allocator() noexcept;

  void yield(CoroListNode &) noexcept;
  void poll(PollListNode &) noexcept;

  Result<void, SyscallError> do_event_loop_iteration() noexcept;

  bool has_active_tasks() noexcept {
    return !m_ready.empty() && !m_polled.empty();
  }

  size_t peak_memory() noexcept {
    return m_alloc.peak_memory();
  }

  size_t current_memory() noexcept {
    return m_alloc.current_memory();
  }

private:
  PollList m_polled;
  CoroList m_ready;
  Allocator m_alloc;
};

} // namespace corosig

#endif
