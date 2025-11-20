/// Say 'hello world' from the inside of coroutinized signal handler

#include "corosig/Coro.hpp"
#include "corosig/Sighandler.hpp"
#include "corosig/io/Stdio.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <csignal>

namespace {

corosig::Fut<void> sighandler(corosig::Reactor &r, int) noexcept {
  (void)co_await corosig::STDOUT.write(r, "Hello, asynchronous signal handler's world!\n");
  co_return corosig::Ok{};
}

} // namespace

int main(int, char **) {
  corosig::set_sighandler<1024, sighandler>(SIGFPE);
  ::raise(SIGFPE);
  return 0;
}
