#include "corosig/reactor/Default.hpp"
#include "corosig/reactor/CoroList.hpp"

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

void Reactor::yield(CoroListNode &node) noexcept {
  m_ready.push_back(node);
}

Result<void, AllocationError> Reactor::do_event_loop_iteration() noexcept {
  for (CoroListNode &node : m_ready) {
    node.coro_from_this().resume();
  }
  return success();
}

} // namespace corosig
