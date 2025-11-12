#include "corosig/reactor/Reactor.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/PollList.hpp"

#include <chrono>
#include <coroutine>
#include <cstddef>
#include <ratio>
#include <sys/poll.h>

namespace {

using namespace corosig;

Result<void, SyscallError>
poll_and_resume(PollList &polled, std::chrono::duration<int, std::milli> timeout) noexcept {
  if (polled.empty()) {
    return success();
  }

  constexpr size_t BUF_SIZE = 32;
  std::array<::pollfd, BUF_SIZE> poll_fds;

  size_t fds_count = 0;
  for (PollListNode &node : polled) {
    assert(node.handle != -1);

    ::pollfd &poll_fd = poll_fds[fds_count];

    poll_fd.fd = node.handle;
    poll_fd.events = [&]() -> short {
      switch (node.event) {
      case poll_event_e::CAN_READ:
        return POLLIN;
      case poll_event_e::CAN_WRITE:
        return POLLOUT;
      }
      assert(false && "Unsupported poll event type");
      return 0;
    }();

    ++fds_count;
  }

  int ret = ::poll(poll_fds.data(), fds_count, timeout.count());
  if (ret == -1) {
    return SyscallError::current();
  }

  for (size_t i = 0; i < size_t(ret); ++i) {
    auto &node = polled.front();
    polled.pop_front();
    node.waiting_coro.resume();
  }
  return success();
}

void resume(CoroList &ready) noexcept {
  constexpr auto ITERATIONS_LIMIT = 1024 * 1024;
  size_t i = 0;
  while (!ready.empty() && i < ITERATIONS_LIMIT) {
    auto &node = ready.front();
    ready.pop_front();
    node.coro_from_this().resume();
    ++i;
  }
}

} // namespace

namespace corosig {

Reactor &Reactor::instance() noexcept {
  thread_local Reactor thread_local_reactor;
  return thread_local_reactor;
}

void *Reactor::allocate_frame(size_t n) noexcept {
  return m_alloc.allocate(n);
}

void Reactor::free_frame(void *frame) noexcept {
  return m_alloc.free(frame);
}

void Reactor::yield(CoroListNode &node) noexcept {
  m_ready.push_back(node);
}

void Reactor::poll(PollListNode &node) noexcept {
  m_polled.push_back(node);
}

Result<void, SyscallError> Reactor::do_event_loop_iteration() noexcept {
  resume(m_ready);

  using namespace std::chrono_literals;
  auto poll_timeout = -1ms;
  if (!m_ready.empty()) {
    poll_timeout = 0ms;
  }

  return poll_and_resume(m_polled, poll_timeout);
}

} // namespace corosig
