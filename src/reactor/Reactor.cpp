#include "corosig/reactor/Reactor.hpp"

#include "corosig/Clock.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
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

namespace {

using namespace corosig;

constexpr auto MIN_REACTOR_POLL_BUFFER = 64;

} // namespace

namespace corosig {

Reactor::Reactor(std::span<char> mem) noexcept
    : m_alloc{mem},
      m_previous_iteration_buffer{MIN_REACTOR_POLL_BUFFER} {
  if (m_poll_buf.reserve(MIN_REACTOR_POLL_BUFFER)) {
    m_poll_and_resume_method = &Reactor::poll_and_resume_normal;
  } else {
    m_poll_and_resume_method = &Reactor::poll_and_resume_fallback;
  }
}

Reactor::~Reactor() {
  gc();
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
  int_milliseconds_type poll_timeout = -1ms;
  if (!m_ready.empty()) {
    poll_timeout = 0ms;
  } else if (!m_sleeping.empty()) {
    poll_timeout = std::max<int_milliseconds_type>(
        0ms, ceil_to_millis(m_sleeping.begin()->awake_time - SteadyClock::now()));
  }

  return std::invoke(&Reactor::poll_and_resume_fallback, this, poll_timeout);
}

Result<void, SyscallError> Reactor::poll_and_resume_normal(int_milliseconds_type timeout) noexcept {
  if (m_polled.empty()) {
    return Ok{};
  }

  size_t shrink_threshold = m_poll_buf.size() / 2;
  if (m_previous_iteration_buffer > MIN_REACTOR_POLL_BUFFER &&
      m_previous_iteration_buffer < shrink_threshold) {
    if (m_poll_buf.resize(shrink_threshold)) {
      (void)m_poll_buf.shrink_to_fit();
    }
  }

  m_poll_buf.clear();
  for (PollListNode const &node : m_polled) {
    assert(node.handle != -1);

    auto poll_fd = ::pollfd{
        .fd = node.handle,
        .events = static_cast<short>(node.event),
        .revents = {},
    };
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
  int ret = ::poll(poll_fds.data(), poll_fds.size(), timeout.count());
  if (ret == -1) {
    return Failure{SyscallError::current()};
  }

  // polled list may become empty if some coroutine cancels execution which may trigger deletion
  // of some of list nodes
  for (size_t i = 0; !m_polled.empty() && i < static_cast<size_t>(ret); ++i) {
    PollListNode &node = m_polled.front();
    // list node was deleted due to coroutine cancelling
    if (poll_fds[i].fd != node.handle) {
      continue;
    }

    m_polled.pop_front();

    assert(node.waiting_coro != nullptr);
    assert(!node.waiting_coro.done());
    node.waiting_coro.resume();
  }

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
