#include "corosig/reactor/Reactor.hpp"

#include "corosig/Clock.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Vector.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/GcList.hpp"
#include "corosig/reactor/PollList.hpp"
#include "corosig/reactor/SleepList.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <sys/poll.h>

namespace corosig {

Reactor::Reactor(std::span<char> mem) noexcept
    : m_alloc{mem} {
  if (m_poll_buf.reserve(MIN_POLL_BUFFER)) {
    m_poll_and_resume_method = &Reactor::poll_and_resume_normal;
  } else {
    m_poll_and_resume_method = &Reactor::poll_and_resume_fallback;
  }
}

Reactor::~Reactor() {
  gc();
  assert(m_gc_list.empty());
  assert(m_polled.empty());
  assert(m_ready.empty());
  assert(m_sleeping.empty());
}

Allocator &Reactor::allocator() noexcept {
  return m_alloc;
}

void Reactor::schedule(CoroListNode &node) noexcept {
  m_ready.push_back(node);
}

void Reactor::destroy_later(GcListNode &to_gc) noexcept {
  m_gc_list.push_front(to_gc);
}

void Reactor::schedule_when_ready(PollListNode &node) noexcept {
  m_polled.push_back(node);
}

void Reactor::schedule_when_time_passes(SleepListNode &node) noexcept {
  m_sleeping.insert(node);
}

bool Reactor::has_active_tasks() const noexcept {
  return !m_polled.empty() || !m_ready.empty() || !m_sleeping.empty();
}

size_t Reactor::peak_memory() const noexcept {
  return m_alloc.peak_memory();
}

size_t Reactor::current_memory() const noexcept {
  return m_alloc.current_memory();
}

Result<void, SyscallError> Reactor::do_event_loop_iteration() noexcept {
  assert(has_active_tasks() && "Nothing to process. Deadlock will happen");

  gc();
  resume_ready_sleepers();

  resume_ready();

  using namespace std::chrono_literals;

  // some kernels are just fucking stupid and may forget about waking you up when events arrive. if
  // timeout is infinite, we will get an effective deadlock. this is why we have to wake up
  // sometimes which forces kernel to check if there were any actual events while we have slept
  constexpr int_milliseconds_type DEFAULT_TIMEOUT = 100ms;
  int_milliseconds_type poll_timeout = 100ms;
  if (!m_ready.empty()) {
    poll_timeout = 0ms;
  } else if (!m_sleeping.empty()) {
    poll_timeout = std::max<int_milliseconds_type>(
        0ms, ceil_to_millis(m_sleeping.begin()->awake_time - SteadyClock::now()));
    poll_timeout = std::min<int_milliseconds_type>(poll_timeout, DEFAULT_TIMEOUT);
  }

  return std::invoke(m_poll_and_resume_method, this, poll_timeout);
}

Result<void, SyscallError> Reactor::poll_and_resume_normal(int_milliseconds_type timeout) noexcept {
  if (m_polled.empty()) {
    return Ok{};
  }

  size_t shrink_threshold = m_poll_buf.capacity() / 4;
  if (m_previous_iteration_buffer > MIN_POLL_BUFFER &&
      m_previous_iteration_buffer < shrink_threshold) {
    Vector<::pollfd> new_buf{m_alloc};
    if (new_buf.reserve(shrink_threshold)) {
      m_poll_buf = std::move(new_buf);
    }
  }

  m_poll_buf.clear();
  for (PollListNode const &node : m_polled) {
    assert(node.handle != -1);

    ::pollfd poll_fd;
    poll_fd.fd = node.handle;
    poll_fd.events = static_cast<short>(node.event);
    if (auto res = m_poll_buf.push_back(poll_fd); !res) {
      break;
    }
  }
  m_previous_iteration_buffer = m_poll_buf.size();

  return poll_and_resume_impl(m_poll_buf, timeout);
}

Result<void, SyscallError>
Reactor::poll_and_resume_fallback(int_milliseconds_type timeout) noexcept {
  if (m_polled.empty()) {
    return Ok{};
  }

  constexpr size_t BUF_SIZE = 64;
  std::array<::pollfd, BUF_SIZE> poll_fds;

  size_t fds_count = 0;
  for (auto it = m_polled.begin(); it != m_polled.end() && fds_count < BUF_SIZE;
       ++fds_count, ++it) {
    PollListNode &node = *it;
    assert(node.handle != -1);

    ::pollfd &poll_fd = poll_fds[fds_count];
    poll_fd.fd = node.handle;
    poll_fd.events = static_cast<short>(node.event);
  }

  return poll_and_resume_impl(std::span{poll_fds.data(), fds_count}, timeout);
}

Result<void, SyscallError> Reactor::poll_and_resume_impl(std::span<::pollfd> poll_fds,
                                                         int_milliseconds_type timeout) noexcept {
  int const ret = ::poll(poll_fds.data(), poll_fds.size(), timeout.count());
  if (ret == -1) {
    return Failure{SyscallError::current()};
  }
  // polled list may become empty if some coroutine cancels execution which may trigger deletion
  // of some of list nodes
  PollList polled = std::move(m_polled);

  size_t handled = 0;
  for (size_t i = 0; handled < static_cast<size_t>(ret) && i < poll_fds.size() && !polled.empty();
       ++i) {

    PollListNode &node = polled.front();

    ::pollfd const pollfd = poll_fds[i];
    if (pollfd.fd != node.handle) {
      if (pollfd.revents != 0) {
        ++handled;
      }
      continue;
    }
    if (pollfd.revents == 0) {
      polled.pop_front();
      m_polled.push_back(node);
      continue;
    }

    polled.pop_front();
    assert(node.waiting_coro != nullptr);
    assert(!node.waiting_coro.done());
    node.waiting_coro.resume();
    ++handled;
  }

  m_polled.splice(m_polled.end(), polled);
  assert(polled.empty());

  return Ok{};
}

bool &Reactor::ref_current_coro_was_allocated() noexcept {
  return m_current_coro_was_allocated;
}

Result<void, SyscallError> Reactor::drain_remaining_tasks() noexcept {
  while (has_active_tasks()) {
    COROSIG_TRYV(do_event_loop_iteration());
  }
  gc();
  return Ok{};
}

Reactor::int_milliseconds_type Reactor::ceil_to_millis(std::chrono::nanoseconds nanos) noexcept {
  return std::chrono::ceil<int_milliseconds_type>(nanos);
}

void Reactor::resume_ready() noexcept {
  // snapshot of all ready coros is taken to prevent deadlocks in case when the only one yield task
  // is pushed in a loop
  auto &ready = m_ready;
  while (!ready.empty()) {
    auto &node = ready.front();
    ready.pop_front();
    node.resume_coro();
  }
}

void Reactor::gc() noexcept {
  m_gc_list.clear_and_dispose([](GcListNode *node) { node->destroy(); });
}

void Reactor::resume_ready_sleepers() noexcept {
  if (m_sleeping.empty()) {
    return;
  }

  auto now = SteadyClock::now();
  while (!m_sleeping.empty()) {
    SleepListNode &node = *m_sleeping.begin();
    if (node.awake_time > now) {
      break;
    }
    m_sleeping.erase(node);
    assert(node.waiting_coro != nullptr);
    assert(!node.waiting_coro.done());
    node.waiting_coro.resume();
  }
}

} // namespace corosig
