#ifndef COROSIG_REACTOR_DEFAULT_HPP
#define COROSIG_REACTOR_DEFAULT_HPP

#include "corosig/Alloc.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/PollList.hpp"

#include <cstddef>

namespace corosig {

struct Reactor {
  static Reactor &instance() noexcept;

  Reactor() noexcept = default;

  template <size_t SIZE>
  Reactor(Alloc::Memory<SIZE> &mem) : m_alloc{mem} {
  }

  void *allocate_frame(size_t) noexcept;
  void free_frame(void *) noexcept;

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
  Alloc m_alloc;
};

} // namespace corosig

#endif
