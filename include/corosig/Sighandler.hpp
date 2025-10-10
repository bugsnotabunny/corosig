#ifndef COROSIG_SIGHANDLER_HPP
#define COROSIG_SIGHANDLER_HPP

#include "corosig/reactor/Default.hpp"

#include <csignal>
#include <stdexcept>

namespace corosig {

namespace detail {

template <size_t MEMORY, auto F>
void sighandler(int sig) noexcept {
  std::signal(sig, SIG_DFL);

  Alloc::Memory<MEMORY> mem;
  auto &reactor = Reactor::instance();
  reactor = Reactor{mem};

  (void)F(sig).block_on();
}

} // namespace detail

template <size_t MEMORY, auto F>
void set_sighandler(int sig) {
  // initialize underlying TLS with default reactor
  // important to do it here since it may cause allocation
  // which is not safe to do inside sighandler
  (void)Reactor::instance();

  if (std::signal(sig, detail::sighandler<MEMORY, F>) == SIG_ERR) {
    throw std::runtime_error{"std::signal failed"};
  }
}

} // namespace corosig

#endif