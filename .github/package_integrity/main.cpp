#include "corosig/Coro.hpp"
#include "corosig/Sighandler.hpp"
#include "corosig/io/Stdio.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <csignal>
#include <cstdlib>
#include <exception>

namespace {

using namespace corosig;

bool sighandler_called = false;

Fut<void> sighandler(Reactor &, int) noexcept {
  sighandler_called = true;
  return Fut<void>::make_ready(Ok{});
}

} // namespace

int main(int, char **) {
  try {
    corosig::set_sighandler<1024 * 2, sighandler>(SIGFPE);
    ::raise(SIGFPE);
    return sighandler_called ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (std::exception const &) {
    return EXIT_FAILURE;
  }
}
