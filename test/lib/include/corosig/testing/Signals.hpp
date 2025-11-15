#ifndef COROSIG_TESTING_SIGNALS_HPP
#define COROSIG_TESTING_SIGNALS_HPP

#include "corosig/reactor/Reactor.hpp"

#include <catch2/catch_all.hpp>
#include <charconv>
#include <concepts>
#include <csignal>
#include <cstddef>
#include <optional>
#include <source_location>
#include <unistd.h>
#include <utility>

namespace corosig {

inline void print(std::string_view msg) noexcept {
  size_t written = 0;
  while (written < msg.size()) {
    ssize_t written_now = ::write(STDOUT_FILENO, msg.data() + written, msg.size() - written);
    if (written_now == -1) {
      return;
    }
    written += size_t(written_now);
  }
}

template <typename T>
inline void print_num(T value) noexcept {
  std::array<char, 100> buf;
  auto res = std::to_chars(std::begin(buf), std::end(buf), value);
  if (res.ec != std::errc{}) {
    return;
  }
  print(std::string_view{std::begin(buf), res.ptr});
}

#define COROSIG_REQUIRE(...)                                                                       \
  do {                                                                                             \
    if (!(__VA_ARGS__)) {                                                                          \
      auto loc = std::source_location::current();                                                  \
      print(loc.file_name());                                                                      \
      print(":");                                                                                  \
      print_num(loc.line());                                                                       \
      print("\n");                                                                                 \
      _exit(EXIT_FAILURE);                                                                         \
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
  static void INTERNAL_TEST_NAME(Reactor &reactor);                                                \
  TEST_CASE(__VA_ARGS__) {                                                                         \
    run_in_sighandler([](Reactor &reactor) { return INTERNAL_TEST_NAME(reactor); });               \
  }                                                                                                \
  void INTERNAL_TEST_NAME([[maybe_unused]] Reactor &reactor)

#define COROSIG_SIGHANDLER_TEST_CASE(...)                                                          \
  INTERNAL_COROSIG_SIGHANDLER_TEST_CASE(INTERNAL_CATCH_UNIQUE_NAME(COROSIG_SIGHANDLER_TEST_),      \
                                        __VA_ARGS__)

} // namespace corosig

#endif
