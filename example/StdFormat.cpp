/// Using std::format together with vector
/// Warning: note that since push_back is noexcept, std::format has no way to propagate errors if
/// some of pushes fail. Moreover, if by some reason one push fails, but next ones are ok, you will
/// have a vector without one element in the middle. Such behaviour may be unwanted in some
/// scenarios

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Sighandler.hpp"
#include "corosig/container/Vector.hpp"
#include "corosig/io/Stdio.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <csignal>
#include <exception>
#include <format>
#include <unistd.h>

namespace {

corosig::Fut<void, corosig::Error<corosig::AllocationError, corosig::SyscallError>>
sighandler(corosig::Reactor &r, int signal) noexcept {
  using namespace corosig;
  Vector<char> string{r.allocator()};
  std::format_to(std::back_inserter(string), "Signal occured for process\n", signal, getpid());
  COROSIG_CO_TRYV(co_await STDOUT.write(r, string));
  co_return corosig::Ok{};
}

} // namespace

int main(int, char **) {
  try {
    corosig::set_sighandler<1024, sighandler>(SIGFPE);
    ::raise(SIGFPE);
    return 0;
  } catch (std::exception const &) {
    return 1;
  }
}
