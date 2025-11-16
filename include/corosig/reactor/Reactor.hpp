#ifndef COROSIG_REACTOR_DEFAULT_HPP
#define COROSIG_REACTOR_DEFAULT_HPP

#include "corosig/Allocator.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/StdAllocator.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/PollList.hpp"

#include <cstddef>
#include <string>
#include <vector>

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

  void *allocate(size_t) noexcept;
  void deallocate(void *) noexcept;

  template <typename T>
  StdAllocator<T, Reactor &> get_std_allocator() noexcept {
    return StdAllocator<T, Reactor &>{*this};
  }

  template <typename T>
  auto get_vector() noexcept {
    return std::vector<T, StdAllocator<T, Reactor &>>{get_std_allocator<T>()};
  }

  template <typename CHAR>
  auto get_string() noexcept {
    return std::basic_string<CHAR, std::char_traits<CHAR>, StdAllocator<CHAR, Reactor &>>{
        get_std_allocator<CHAR>()};
  }

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
