#ifndef COROSIG_TESTING_SIGNALS_HPP
#define COROSIG_TESTING_SIGNALS_HPP

#include "corosig/reactor/Reactor.hpp"

#include <catch2/catch_all.hpp>
#include <concepts>
#include <csignal>
#include <cstddef>
#include <optional>
#include <source_location>
#include <unistd.h>
#include <utility>

namespace corosig {

void print(std::string_view msg) noexcept;
void print_num(size_t value) noexcept;

#define COROSIG_REQUIRE(...)                                                                       \
  do {                                                                                             \
    if (!(__VA_ARGS__)) {                                                                          \
      auto loc = ::std::source_location::current();                                                \
      ::corosig::print(loc.file_name());                                                           \
      ::corosig::print(":");                                                                       \
      ::corosig::print_num(loc.line());                                                            \
      ::corosig::print("\n");                                                                      \
      ::_exit(EXIT_FAILURE);                                                                       \
    }                                                                                              \
  } while (false)

template <std::invocable<Reactor &> F>
void run_in_sighandler(F &&f) {
  static std::optional<F> g_foo;
  COROSIG_REQUIRE(!g_foo);
  g_foo = std::forward<F>(f);

  constexpr auto SIGHANDLER = [](int sig) noexcept {
    std::signal(sig, SIG_DFL);

    Allocator::Memory<size_t(1024 * 8)> mem;
    Reactor reactor{mem};
    (*g_foo)(reactor);
  };

  constexpr auto SIGNAL = SIGILL;
  auto old_handler = std::signal(SIGNAL, SIGHANDLER);
  COROSIG_REQUIRE(old_handler != SIG_ERR);
  ::raise(SIGNAL);
  COROSIG_REQUIRE(std::signal(SIGNAL, old_handler) != SIG_ERR);
}

#define INTERNAL_COROSIG_SIGHANDLER_TEST_CASE(INTERNAL_TEST_NAME, ...)                             \
  static void INTERNAL_TEST_NAME(::corosig::Reactor &reactor);                                     \
  TEST_CASE(__VA_ARGS__) {                                                                         \
    ::corosig::run_in_sighandler(                                                                  \
        [](::corosig::Reactor &reactor) { return INTERNAL_TEST_NAME(reactor); });                  \
  }                                                                                                \
  void INTERNAL_TEST_NAME([[maybe_unused]] ::corosig::Reactor &reactor)

#define COROSIG_SIGHANDLER_TEST_CASE(...)                                                          \
  INTERNAL_COROSIG_SIGHANDLER_TEST_CASE(INTERNAL_CATCH_UNIQUE_NAME(COROSIG_SIGHANDLER_TEST_),      \
                                        __VA_ARGS__)

} // namespace corosig

#endif
