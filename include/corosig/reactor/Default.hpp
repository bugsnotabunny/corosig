#pragma once

#include "corosig/Alloc.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Custom.hpp"

#include <chrono>
#include <coroutine>
#include <cstddef>
#include <sys/poll.h>
#include <variant>
#include <vector>

namespace corosig {

struct Reactor {
  Reactor() = default;

  template <size_t SIZE>
  Reactor(Alloc::Memory<SIZE> &mem) : m_alloc{mem} {
  }

  void *allocate_frame(size_t) noexcept;
  void free_frame(void *) noexcept;

  void yield(CoroListNode &) noexcept;

  Result<void, AllocationError> do_event_loop_iteration() noexcept;

  size_t peak_memory() noexcept {
    return m_alloc.peak_memory();
  }

  size_t current_memory() noexcept {
    return m_alloc.current_memory();
  }

private:
  CoroList m_ready;
  Alloc m_alloc;
};

template <>
struct reactor_provider<Reactor> {
  static Reactor &engine() noexcept;
};

static_assert(AReactor<Reactor>);

} // namespace corosig
