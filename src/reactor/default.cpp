#include "corosig/reactor/default.hpp"
#include "corosig/reactor/custom.hpp"
#include <coroutine>
#include <variant>

namespace {

using namespace corosig;

Reactor reactor;

} // namespace

namespace corosig {

Reactor &reactor_provider<Reactor>::engine() noexcept {
  return reactor;
}

void *Reactor::allocate_frame(size_t n) noexcept {
  return m_alloc.allocate(n);
}

void Reactor::free_frame(void *frame) noexcept {
  return m_alloc.free(frame);
}

void Reactor::yield(std::coroutine_handle<> h) noexcept {
  m_ready.push_back(h);
}

Result<std::monostate, AllocationError> Reactor::do_event_loop_iteration() noexcept {
  for (std::coroutine_handle<> h : m_ready) {
    h.resume();
  }
  return std::monostate{};
}

} // namespace corosig
