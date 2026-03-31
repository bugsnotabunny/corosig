#include "corosig/Random.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/io/File.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace {

void chacha20_quarter_round(uint32_t *s, size_t a, size_t b, size_t c, size_t d) noexcept {
  s[a] += s[b];
  s[d] ^= s[a];
  s[d] = (s[d] << 16) | (s[d] >> 16);
  s[c] += s[d];
  s[b] ^= s[c];
  s[b] = (s[b] << 12) | (s[b] >> 20);
  s[a] += s[b];
  s[d] ^= s[a];
  s[d] = (s[d] << 8) | (s[d] >> 24);
  s[c] += s[d];
  s[b] ^= s[c];
  s[b] = (s[b] << 7) | (s[b] >> 25);
}

} // namespace

namespace corosig {

Fut<size_t, Error<AllocationError, SyscallError>> read_dev_urandom(Reactor &r,
                                                                   std::span<char> out) noexcept {
  COROSIG_CO_TRY(auto dev_random, co_await File::open(r, "/dev/urandom"));
  COROSIG_CO_TRY(auto bytes_read, co_await dev_random.read(r, out));
  co_return bytes_read;
}

ChaCha20RandGen::ChaCha20RandGen(std::array<uint8_t, 32> seed) noexcept {
  constexpr std::array<uint32_t, 4> CONSTANT = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};

  auto *out = std::ranges::copy(CONSTANT, m_state.data()).out;
  std::memcpy(out, seed.data(), seed.size() * sizeof(uint8_t));

  // Counter and nonce (initial counter = 0, nonce = 0)
  m_state[12] = 0; // counter low
  m_state[13] = 0; // counter high (ChaCha20 uses 64‑bit counter)
  m_state[14] = 0; // nonce low
  m_state[15] = 0; // nonce high
}

void ChaCha20RandGen::generate_bytes(std::span<std::byte> out) noexcept {
  auto *dst = out.data();
  auto len = out.size();
  while (len > 0) {
    if (m_pos == 64) {
      refill();
    }
    size_t to_copy = (len < 64 - m_pos) ? len : 64 - m_pos;
    std::memcpy(dst, m_buffer.data() + m_pos, to_copy);
    dst += to_copy;
    len -= to_copy;
    m_pos += to_copy;
  }
}

void ChaCha20RandGen::refill() noexcept {
  auto working_state = m_state;

  for (size_t i = 0; i < 10; ++i) {
    chacha20_quarter_round(working_state.data(), 0, 4, 8, 12);
    chacha20_quarter_round(working_state.data(), 1, 5, 9, 13);
    chacha20_quarter_round(working_state.data(), 2, 6, 10, 14);
    chacha20_quarter_round(working_state.data(), 3, 7, 11, 15);

    chacha20_quarter_round(working_state.data(), 0, 5, 10, 15);
    chacha20_quarter_round(working_state.data(), 1, 6, 11, 12);
    chacha20_quarter_round(working_state.data(), 2, 7, 8, 13);
    chacha20_quarter_round(working_state.data(), 3, 4, 9, 14);
  }

  for (size_t i = 0; i < STATE_WORDS; ++i) {
    working_state[i] += m_state[i];
  }

  for (size_t i = 0; i < STATE_WORDS; ++i) {
    uint32_t w = working_state[i];
    m_buffer[i * 4 + 0] = uint8_t(w >> 0);
    m_buffer[i * 4 + 1] = uint8_t(w >> 8);
    m_buffer[i * 4 + 2] = uint8_t(w >> 16);
    m_buffer[i * 4 + 3] = uint8_t(w >> 24);
  }

  if (++m_state[12] == 0) {
    ++m_state[13];
  }

  m_pos = 0; // reset buffer position
}

} // namespace corosig
