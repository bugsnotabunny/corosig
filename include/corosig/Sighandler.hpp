#ifndef COROSIG_SIGHANDLER_HPP
#define COROSIG_SIGHANDLER_HPP

#include "corosig/io/Stdio.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <csignal>
#include <cstdlib>
#include <stdexcept>

namespace corosig {

namespace detail {

template <size_t MEMORY, auto F>
void sighandler(int sig) noexcept {
  std::signal(sig, SIG_DFL); // to avoid recursive call if something inside sighandler goes wrong
  Allocator::Memory<MEMORY> mem;
  Reactor reactor{mem};
  Result result = F(reactor, sig).block_on_with_reactor_drain();
  if (!result.is_ok()) {
    (void)STDERR.write(reactor, "Unhandled error was returned from sighandler\n").block_on();
  }
}

} // namespace detail

/// @brief  Sets a signal handler to work when sig is raised. This ensures there are no
///          recursive calls to the handler if something goes wrong inside. And also that all
///          unhandled errors from F are at least reported
/// @note   There is nothing wrong to write your own signal handler. This function is here only to
///          save users some boilerplate and give idea about what may be good for them to do in the
///          start of sighandling
template <size_t MEMORY, auto F>
void set_sighandler(int sig) {
  if (std::signal(sig, detail::sighandler<MEMORY, F>) == SIG_ERR) {
    throw std::runtime_error{"std::signal failed"};
  }
}

} // namespace corosig

#endif
