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
  ChaCha20RandomGenerator(std::array<uint8_t, 32> seed,
                          std::array<uint32_t, 3> nonce = {},
                          uint32_t block_counter = 0) noexcept;

  void generate_bytes(std::span<std::byte> out) noexcept;

private:
  void refill_state() noexcept;

  static constexpr uint8_t STATE_WORDS = 16;
  static constexpr uint8_t BLOCK_BYTES = 64;

  std::array<uint32_t, STATE_WORDS> m_state;
  std::array<uint32_t, STATE_WORDS> m_working_state;
  uint8_t m_pos = BLOCK_BYTES;
};

} // namespace corosig

#endif
