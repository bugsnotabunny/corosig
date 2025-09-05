#pragma once

#include <concepts>
#include <cstddef>

namespace corosig {

template <typename REACTOR>
concept AReactor = requires(REACTOR &reactor) {
  { reactor.allocate_frame(std::declval<size_t>()) } -> std::same_as<void *>;
  { reactor.free_frame(std::declval<void *>()) } -> std::same_as<void>;
};

// static REACTOR& engine() noexcept;
template <AReactor REACTOR>
struct reactor_provider;

} // namespace corosig
