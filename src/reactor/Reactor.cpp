#include "corosig/reactor/Reactor.hpp"

#include "corosig/Clock.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/PollList.hpp"
#include "corosig/reactor/SleepList.hpp"

#include <chrono>
#include <coroutine>
#include <cstddef>
#include <ratio>
#include <sys/poll.h>

namespace {

using namespace corosig;

void resume_ready_sleepers(SleepList &sleeping) noexcept {
  auto now = Clock::now();

  while (!sleeping.empty()) {
    SleepListNode &node = *sleeping.begin();
    if (node.awake_time > now) {
      break;
    }
    sleeping.erase(node);
    assert(node.waiting_coro != nullptr);
    assert(!node.waiting_coro.done());
    node.waiting_coro.resume();
  }
}

Result<void, SyscallError>
poll_and_resume(PollList &polled, std::chrono::duration<int, std::milli> timeout) noexcept {
  if (polled.empty()) {
    return Ok{};
  }

  constexpr size_t BUF_SIZE = 32;
  std::array<::pollfd, BUF_SIZE> poll_fds;

  size_t fds_count = 0;
  for (PollListNode &node : polled) {
    assert(node.handle != -1);

    ::pollfd &poll_fd = poll_fds[fds_count];

    poll_fd.fd = node.handle;
    poll_fd.events = short(node.event);

    ++fds_count;
  }

  int ret = ::poll(poll_fds.data(), fds_count, timeout.count());
  if (ret == -1) {
    return Failure{SyscallError::current()};
  }

  for (size_t i = 0; i < size_t(ret); ++i) {
    auto &node = polled.front();
    polled.pop_front();

    assert(node.waiting_coro != nullptr);
    assert(!node.waiting_coro.done());
    node.waiting_coro.resume();
  }
  return Ok{};
}

void resume(CoroList &ready) noexcept {
  constexpr auto ITERATIONS_LIMIT = 1024;

  for (size_t i = 0; !ready.empty() && i < ITERATIONS_LIMIT; ++i) {
    auto &node = ready.front();
    ready.pop_front();
    auto coro = node.coro_from_this();
    assert(coro != nullptr);
    assert(!coro.done());
    coro.resume();
  }
}

std::chrono::milliseconds ceil_to_millis(std::chrono::nanoseconds nanos) noexcept {
  return std::chrono::milliseconds{nanos.count() / 1000 + nanos.count() % 1000};
}

} // namespace

namespace corosig {

Allocator &Reactor::allocator() noexcept {
  return m_alloc;
}

void Reactor::schedule(CoroListNode &node) noexcept {
  m_ready.push_back(node);
}

void Reactor::schedule_when_ready(PollListNode &node) noexcept {
  m_polled.push_back(node);
}

void Reactor::schedule_when_time_passes(SleepListNode &node) noexcept {
  m_sleeping.insert(node);
}

bool Reactor::has_active_tasks() const noexcept {
  return !m_polled.empty() && !m_ready.empty();
}

size_t Reactor::peak_memory() const noexcept {
  return m_alloc.peak_memory();
}

size_t Reactor::current_memory() const noexcept {
  return m_alloc.current_memory();
}

Result<void, SyscallError> Reactor::do_event_loop_iteration() noexcept {
  assert((!m_sleeping.empty() || !m_ready.empty() || !m_polled.empty()) &&
         "Nothing to process. Deadlock will happen");
  resume_ready_sleepers(m_sleeping);
  resume(m_ready);

  using namespace std::chrono_literals;
  auto poll_timeout = -1ms;
  if (!m_ready.empty()) {
    poll_timeout = 0ms;
  } else if (!m_sleeping.empty()) {
    poll_timeout = std::max(0ms, ceil_to_millis(m_sleeping.begin()->awake_time - Clock::now()));
  }

  return poll_and_resume(m_polled, poll_timeout);
}

} // namespace corosig
