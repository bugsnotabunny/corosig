#include "corosig/testing/Signals.hpp"

#include <cstddef>
#include <unistd.h>

namespace corosig {

void print(std::string_view msg) noexcept {
  size_t written = 0;
  while (written < msg.size()) {
    ssize_t written_now = ::write(STDOUT_FILENO, msg.data() + written, msg.size() - written);
    if (written_now == -1) {
      return;
    }
    written += size_t(written_now);
  }
}

void print_num(size_t value) noexcept {
  std::array<char, 100> buf;
  auto res = std::to_chars(std::begin(buf), std::end(buf), value);
  if (res.ec != std::errc{}) {
    return;
  }
  print(std::string_view{std::begin(buf), res.ptr});
}

} // namespace corosig
