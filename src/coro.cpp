#include "coro.hpp"
#include <coroutine>

namespace sigawait {

future<int> coro(size_t) noexcept {
  co_await std::suspend_always{};
  co_return 0;
}

} // namespace sigawait
