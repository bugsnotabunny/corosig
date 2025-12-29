#include "corosig/Semaphore.hpp"

#include "corosig/reactor/Reactor.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <memory>

namespace corosig {

Semaphore::Holder::Holder(Semaphore &semaphore, size_t units) noexcept
    : m_units{units},
      m_semaphore{std::addressof(semaphore)} {
}

Semaphore::Holder::~Holder() {
  reset();
}

void Semaphore::Holder::reset() noexcept {
  if (*m_semaphore != nullptr && *m_units != 0) {
    m_semaphore.value->free_units(*m_units);
    m_semaphore.value = {};
    m_units.value = {};
  }
}

Semaphore::HolderAwaiter::HolderAwaiter(Semaphore &semaphore, size_t units) noexcept
    : m_semaphore{semaphore},
      m_units{units} {
}

std::coroutine_handle<> Semaphore::HolderAwaiter::coro_from_this() noexcept {
  return m_waiting_coro;
}

[[nodiscard]] bool Semaphore::HolderAwaiter::await_ready() noexcept {
  if (m_semaphore.would_block(m_units)) {
    return false;
  }
  m_semaphore.take_units(m_units);
  return true;
}

void Semaphore::HolderAwaiter::await_suspend(std::coroutine_handle<> h) noexcept {
  m_waiting_coro = h;
  m_semaphore.m_waiters.push_back(*this);
}

[[nodiscard]] Semaphore::Holder Semaphore::HolderAwaiter::await_resume() const noexcept {
  return Holder{m_semaphore, m_units};
}

Semaphore::Semaphore(Reactor &reactor, size_t max_parallelism) noexcept
    : m_max_parallelism{max_parallelism},
      m_reactor{reactor} {
}

Semaphore::~Semaphore() {
  assert(current_parallelism() == 0 &&
         "Destroying semaphore without releasing all of it's holders is forbidden");
}

bool Semaphore::would_block(size_t units) const noexcept {

  return m_current_parallelism + units > m_max_parallelism;
}

size_t Semaphore::max_parallelism() const noexcept {
  return m_max_parallelism;
}

size_t Semaphore::current_parallelism() const noexcept {
  return m_current_parallelism;
}

std::optional<Semaphore::Holder> Semaphore::try_hold(size_t units) noexcept {
  if (would_block(units)) {
    return std::nullopt;
  }
  take_units(units);
  return Holder{*this, units};
}

Semaphore::HolderAwaiter Semaphore::hold(size_t units) noexcept {
  assert(units <= m_max_parallelism &&
         "Requesting more units than max_parallelism allows would just cause a deadlock");
  return HolderAwaiter{*this, units};
}

void Semaphore::take_units(size_t units) noexcept {
  assert(!would_block(units));
  m_current_parallelism += units;
}

void Semaphore::free_units(size_t units) noexcept {
  assert(m_current_parallelism >= units);
  m_current_parallelism -= units;

  while (!m_waiters.empty() && !would_block(m_waiters.front().m_units)) {
    auto &waiter = m_waiters.front();
    m_waiters.pop_front();
    take_units(waiter.m_units);
    m_reactor.schedule(waiter);
  }
}

} // namespace corosig
