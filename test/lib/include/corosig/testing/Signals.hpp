#pragma once

#include "corosig/reactor/Custom.hpp"
#include "corosig/reactor/Default.hpp"

#include <catch2/catch.hpp>
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
  char buf[100];
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

template <typename F>
void test_in_sighandler(F &&f) {
  static std::optional<F> g_foo;
  COROSIG_REQUIRE(!g_foo);
  g_foo = std::forward<F>(f);

  reactor_provider<Reactor>::engine(); // allocate TLS for reactor

  constexpr auto sighandler = [](int sig) noexcept {
    std::signal(sig, SIG_DFL);

    Alloc::Memory<1024 * 8> mem;
    Reactor &reactor = reactor_provider<Reactor>::engine();
    reactor = Reactor{mem};

    (*g_foo)();
  };

  constexpr auto SIGNAL = SIGILL;
  auto old_handler = std::signal(SIGNAL, sighandler);
  COROSIG_REQUIRE(old_handler != SIG_ERR);
  ::raise(SIGNAL);
  COROSIG_REQUIRE(std::signal(SIGNAL, old_handler) != SIG_ERR);
}

} // namespace corosig
