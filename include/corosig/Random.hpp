#ifndef COROSIG_RANDOM_HPP
#define COROSIG_RANDOM_HPP
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace corosig {

Fut<size_t, Error<AllocationError, SyscallError>> read_dev_urandom(Reactor &,
                                                                   std::span<char> out) noexcept;

template <typename T>
concept ARandomGenerator = requires(T gen) {
  { gen.generate_bytes(std::span<std::byte>{}) } noexcept -> std::same_as<void>;
};

struct ChaCha20RandomGenerator {
  ChaCha20RandomGenerator(std::array<uint8_t, 32> seed) noexcept;

  void generate_bytes(std::span<std::byte> out) noexcept;

private:
  void refill() noexcept;

  static constexpr size_t STATE_WORDS = 16;
  static constexpr size_t BLOCK_BYTES = 64;

  size_t m_pos = 64;
  std::array<uint32_t, STATE_WORDS> m_state;
  std::array<uint8_t, BLOCK_BYTES> m_buffer;
};

} // namespace corosig

#endif
