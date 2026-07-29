#include "corosig/container/Allocator.hpp"

#include "corosig/meta/AnAllocator.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#if COROSIG_ASAN_ENABLED
#include <sanitizer/asan_interface.h>
#include <utility>
#endif

namespace {

using namespace corosig;

static_assert(AnAllocator<Allocator>);

uintptr_t align_right_diff(char const *p, size_t alignment) noexcept {
  assert(std::has_single_bit(alignment));
  auto pval = reinterpret_cast<uintptr_t>(p);
  return ((pval + alignment - 1) & ~(alignment - 1)) - pval;
}

size_t ceil_div(size_t a, size_t b) noexcept {
  return a / b + static_cast<size_t>(a % b != 0);
}

#if COROSIG_ASAN_ENABLED
struct AsanUnpoisonGuard {
  AsanUnpoisonGuard(void *mem, size_t size) noexcept
      : m_mem{mem},
        m_size{size} {
    if (m_mem != nullptr) {
      ASAN_UNPOISON_MEMORY_REGION(m_mem, m_size);
    }
  }

  AsanUnpoisonGuard(AsanUnpoisonGuard &&rhs) noexcept
      : AsanUnpoisonGuard{
            std::exchange(rhs.m_mem, nullptr),
            std::exchange(rhs.m_size, 0),
        } {
  }

  AsanUnpoisonGuard &operator=(AsanUnpoisonGuard &&rhs) noexcept {
    if (this != &rhs) {
      this->~AsanUnpoisonGuard();
      new (this) AsanUnpoisonGuard{std::move(rhs)};
    }
    return *this;
  }

  AsanUnpoisonGuard(AsanUnpoisonGuard const &) = delete;
  AsanUnpoisonGuard &operator=(AsanUnpoisonGuard const &) = delete;

  void release() noexcept {
    m_mem = nullptr;
    m_size = 0;
  }

  ~AsanUnpoisonGuard() {
    if (m_mem != nullptr) {
      ASAN_POISON_MEMORY_REGION(m_mem, m_size);
    }
  }

private:
  void *m_mem;
  size_t m_size;
};
#else

struct AsanUnpoisonGuard {
  AsanUnpoisonGuard(void *, size_t) noexcept {
  }

  AsanUnpoisonGuard(AsanUnpoisonGuard const &) = delete;
  AsanUnpoisonGuard(AsanUnpoisonGuard &&) = default;
  AsanUnpoisonGuard &operator=(AsanUnpoisonGuard const &) = delete;
  AsanUnpoisonGuard &operator=(AsanUnpoisonGuard &&) = default;

  void release() noexcept {
  }
};
#endif

} // namespace

namespace corosig {

Allocator::Allocator(std::span<char> mem) noexcept
    : m_mem{mem} {
  static_assert(std::has_single_bit(BLOCK_SIZE),
                "Block size shall be power of 2 to enable more compiler optimizations");

  static_assert(sizeof(BlockMetadata) <= BLOCK_SIZE,
                "Block shall be able to contain metadata inside");

  m_mem = m_mem.subspan(align_right_diff(m_mem.data(), BLOCK_SIZE));
  m_mem = m_mem.subspan(0, m_mem.size() - m_mem.size() % BLOCK_SIZE);

  assert(m_mem.size() < BLOCK_SIZE * std::numeric_limits<uint32_t>::max());

#if COROSIG_ASAN_ENABLED
  ASAN_POISON_MEMORY_REGION(m_mem.data(), m_mem.size());
  m_mem = m_mem.subspan(std::min(m_mem.size(), BLOCK_SIZE));
  m_mem = m_mem.subspan(0, std::min(m_mem.size(), m_mem.size() - BLOCK_SIZE));
#endif

  m_mem = m_mem.subspan(
      0, std::min<size_t>(m_mem.size(), std::numeric_limits<uint32_t>::max() * BLOCK_SIZE));

  size_t const blocks_amount = this->blocks_amount();

  if (blocks_amount > 0) {
    size_t idx = 0;
    auto &metadata = get_block_metadata(idx);
    AsanUnpoisonGuard guard{&metadata, sizeof(BlockMetadata)};
    metadata = BlockMetadata{
        .blocks_before = 0,
        .blocks_owned = static_cast<uint32_t>(blocks_amount),
    };
    push_free_node(idx);
  }
}

Allocator::~Allocator() {
  assert(m_used == 0 && "Memory leak detected");
#if COROSIG_ASAN_ENABLED
  if (m_mem.size() != 0) {
    m_mem = std::span{m_mem.data() - BLOCK_SIZE, m_mem.size() + BLOCK_SIZE * 2};
    ASAN_UNPOISON_MEMORY_REGION(m_mem.data(), m_mem.size());
  }
#endif
}

size_t Allocator::peak_memory() const noexcept {
  return m_peak;
}

size_t Allocator::current_memory() const noexcept {
  return m_used;
}

void *Allocator::allocate(size_t size, size_t alignment) noexcept {
  assert(std::has_single_bit(alignment) && "Alignment must be a power of 2");

  for (size_t metadata_idx = m_first_free_block_idx; metadata_idx < blocks_amount();) {
    BlockMetadata *metadata = &get_block_metadata(metadata_idx);
    AsanUnpoisonGuard guard{metadata, sizeof(BlockMetadata)};

    assert(!metadata->is_used);
    assert(metadata->next_free_block_idx == INVALID_IDX ||
           metadata->next_free_block_idx != metadata_idx);

    size_t align_diff = align_right_diff(static_cast<char *>(metadata->get_mem()), alignment);
    size_t actual_size = sizeof(BlockMetadata) + size + align_diff;
    size_t blocks_needed = ceil_div(actual_size, BLOCK_SIZE);

    if (metadata->blocks_owned < blocks_needed) {
      metadata_idx = metadata->next_free_block_idx;
      continue;
    }

    size_t align_blocks_skip = align_diff / BLOCK_SIZE;
    size_t blocks_needed_no_align = blocks_needed - align_blocks_skip;
    if (align_blocks_skip != 0) {
      size_t blocks_owned = metadata->blocks_owned - align_blocks_skip;

      set_blocks_owned(metadata_idx, align_blocks_skip);
      unlink_free_node(metadata_idx);
      push_free_node(metadata_idx);

      metadata_idx += align_blocks_skip;
      metadata = &get_block_metadata(metadata_idx);
      guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};
      metadata->default_initialize();
      set_blocks_owned(metadata_idx, blocks_owned);

      guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};
    }

    BlockMetadata old_metadata = *metadata;
    unlink_free_node(metadata_idx);
    guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};
    metadata->is_used = true;
    set_blocks_owned(metadata_idx, blocks_needed_no_align);

    if (blocks_needed_no_align < old_metadata.blocks_owned) {
      size_t new_metadata_idx = metadata_idx + blocks_needed_no_align;

      BlockMetadata &new_metadata = get_block_metadata(new_metadata_idx);
      AsanUnpoisonGuard guard{&new_metadata, sizeof(BlockMetadata)};
      new_metadata.default_initialize();
      set_blocks_owned(new_metadata_idx,
                       static_cast<uint32_t>(old_metadata.blocks_owned - blocks_needed_no_align));
      push_free_node(new_metadata_idx);
    }

    m_used += blocks_needed_no_align * BLOCK_SIZE;
    m_peak = std::max(m_peak, m_used);

    assert(m_used <= m_mem.size());

    void *result =
        reinterpret_cast<char *>(metadata->get_mem()) + align_diff - align_blocks_skip * BLOCK_SIZE;

#if COROSIG_ASAN_ENABLED
    ASAN_UNPOISON_MEMORY_REGION(result, size);
#endif
    assert(reinterpret_cast<uintptr_t>(result) % alignment == 0);
    return result;
  }

  return nullptr;
}

void Allocator::deallocate(void *ptr) noexcept {
  if (ptr == nullptr) {
    return;
  }

  assert(ptr >= &*m_mem.begin() && ptr < &*m_mem.end() &&
         "Given pointer is out of allocator's scope");

  size_t metadata_idx = get_metadata_idx_from_addr(ptr);

  BlockMetadata *metadata = &get_block_metadata(metadata_idx);
  AsanUnpoisonGuard guard{metadata, sizeof(BlockMetadata)};

  assert(metadata->is_used && "Double free detected");
  assert(metadata->next_free_block_idx == INVALID_IDX);
  assert(metadata->prev_free_block_idx == INVALID_IDX);
  assert(m_used >= metadata->blocks_owned * BLOCK_SIZE &&
         "Double free detected. Also there might have been other double frees before that");

  m_used -= metadata->blocks_owned * BLOCK_SIZE;

  size_t const blocks_amount = this->blocks_amount();

  if (metadata->blocks_before != 0) {
    size_t previous_block_idx = metadata_idx - metadata->blocks_before;
    BlockMetadata &previous_metadata = get_block_metadata(previous_block_idx);
    AsanUnpoisonGuard guard1{&previous_metadata, sizeof(BlockMetadata)};
    if (!previous_metadata.is_used) {
      set_blocks_owned(previous_block_idx, previous_metadata.blocks_owned + metadata->blocks_owned);
      unlink_free_node(previous_block_idx);

      metadata_idx = previous_block_idx;

      metadata = &previous_metadata;
      guard = std::move(guard1);

      assert(metadata->next_free_block_idx == INVALID_IDX);
      assert(metadata->prev_free_block_idx == INVALID_IDX);
    }
  }

  assert(metadata->prev_free_block_idx == INVALID_IDX);
  assert(metadata->next_free_block_idx == INVALID_IDX);

  size_t next_block_idx = metadata_idx + metadata->blocks_owned;
  if (next_block_idx < blocks_amount) {
    BlockMetadata &next_metadata = get_block_metadata(next_block_idx);
    AsanUnpoisonGuard guard2{&next_metadata, sizeof(BlockMetadata)};

    if (!next_metadata.is_used) {
      set_blocks_owned(metadata_idx, metadata->blocks_owned + next_metadata.blocks_owned);
      guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};
      assert(metadata->prev_free_block_idx == INVALID_IDX);
      assert(metadata->next_free_block_idx == INVALID_IDX);
      guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};
      guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};

      unlink_free_node(next_block_idx);
      guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};
      assert(metadata->prev_free_block_idx == INVALID_IDX);
      assert(metadata->next_free_block_idx == INVALID_IDX);
    }
  }
  guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};

  assert(metadata->prev_free_block_idx == INVALID_IDX);
  assert(metadata->next_free_block_idx == INVALID_IDX);

  metadata->is_used = false;
  unlink_free_node(metadata_idx);
  push_free_node(metadata_idx);

#if COROSIG_ASAN_ENABLED
  guard = AsanUnpoisonGuard{metadata, sizeof(BlockMetadata)};
  ASAN_POISON_MEMORY_REGION(metadata, metadata->blocks_owned * BLOCK_SIZE);
#endif
}

void Allocator::set_blocks_owned(size_t idx, uint32_t value) noexcept {
  BlockMetadata &metadata = get_block_metadata(idx);
  AsanUnpoisonGuard guard1{&metadata, sizeof(BlockMetadata)};
  metadata.blocks_owned = value;
  if (idx + value < blocks_amount()) {
    BlockMetadata &next_metadata = get_block_metadata(idx + value);
    AsanUnpoisonGuard guard2{&next_metadata, sizeof(BlockMetadata)};
    next_metadata.blocks_before = value;
  }
}

size_t Allocator::blocks_amount() const noexcept {
  return m_mem.size() / BLOCK_SIZE;
}

size_t Allocator::get_metadata_idx_from_addr(void *p) noexcept {
  size_t index = (reinterpret_cast<char *>(p) - m_mem.data()) / BLOCK_SIZE;
  if (reinterpret_cast<uintptr_t>(p) % BLOCK_SIZE == 0) {
    index -= 1;
  }
  return index;
}

Allocator::BlockMetadata &Allocator::get_block_metadata(size_t idx) noexcept {
  assert(idx < blocks_amount());
  return reinterpret_cast<Allocator::BlockMetadata &>(m_mem[idx * BLOCK_SIZE]);
}

void Allocator::push_free_node(size_t idx) noexcept {
  BlockMetadata &metadata = get_block_metadata(idx);
  AsanUnpoisonGuard guard1{&metadata, sizeof(BlockMetadata)};

  assert(metadata.prev_free_block_idx == INVALID_IDX);
  assert(metadata.next_free_block_idx == INVALID_IDX);
  assert(!metadata.is_used);

  if (m_first_free_block_idx == INVALID_IDX) {
    m_first_free_block_idx = idx;
    m_last_free_block_idx = idx;
    return;
  }

  // if block is big enough, push to front to prioritize it for allocations
  // and thus ammortize linear search by a lot
  if (metadata.blocks_owned > blocks_amount() / 4) {
    BlockMetadata &front_metadata = get_block_metadata(m_first_free_block_idx);
    AsanUnpoisonGuard guard2{&front_metadata, sizeof(BlockMetadata)};

    assert(front_metadata.prev_free_block_idx == INVALID_IDX);
    assert(!front_metadata.is_used);

    assert(m_first_free_block_idx != idx);
    front_metadata.prev_free_block_idx = idx;
    metadata.next_free_block_idx = m_first_free_block_idx;
    m_first_free_block_idx = idx;
    return;
  }

  BlockMetadata &back_metadata = get_block_metadata(m_last_free_block_idx);
  AsanUnpoisonGuard guard2{&back_metadata, sizeof(BlockMetadata)};

  assert(back_metadata.next_free_block_idx == INVALID_IDX);
  assert(!back_metadata.is_used);

  assert(m_last_free_block_idx != idx);
  back_metadata.next_free_block_idx = idx;
  metadata.prev_free_block_idx = m_last_free_block_idx;
  m_last_free_block_idx = idx;
}

void Allocator::unlink_free_node(size_t idx) noexcept {
  BlockMetadata &metadata = get_block_metadata(idx);

  AsanUnpoisonGuard guard{&metadata, sizeof(BlockMetadata)};
  assert(!metadata.is_used);
  assert(metadata.prev_free_block_idx != idx);
  assert(metadata.next_free_block_idx != idx);

  if (metadata.next_free_block_idx != INVALID_IDX) {
    BlockMetadata &next_free_metadata = get_block_metadata(metadata.next_free_block_idx);

    AsanUnpoisonGuard guard{&next_free_metadata, sizeof(BlockMetadata)};
    assert(!next_free_metadata.is_used);
    assert(next_free_metadata.prev_free_block_idx == idx);
    next_free_metadata.prev_free_block_idx = metadata.prev_free_block_idx;
  }

  guard = AsanUnpoisonGuard{&metadata, sizeof(BlockMetadata)};
  if (metadata.prev_free_block_idx != INVALID_IDX) {
    BlockMetadata &prev_free_metadata = get_block_metadata(metadata.prev_free_block_idx);

    AsanUnpoisonGuard guard{&prev_free_metadata, sizeof(BlockMetadata)};
    assert(!prev_free_metadata.is_used);
    assert(prev_free_metadata.next_free_block_idx == idx);
    prev_free_metadata.next_free_block_idx = metadata.next_free_block_idx;
  }

  if (m_first_free_block_idx == idx) {
    assert(metadata.prev_free_block_idx == INVALID_IDX);
    m_first_free_block_idx = metadata.next_free_block_idx;
  }

  if (m_last_free_block_idx == idx) {
    assert(metadata.next_free_block_idx == INVALID_IDX);
    m_last_free_block_idx = metadata.prev_free_block_idx;
  }

  metadata.next_free_block_idx = INVALID_IDX;
  metadata.prev_free_block_idx = INVALID_IDX;
}

void *Allocator::BlockMetadata::get_mem() noexcept {
  return static_cast<void *>(this + 1);
}

void Allocator::BlockMetadata::default_initialize() noexcept {
  prev_free_block_idx = INVALID_IDX;
  next_free_block_idx = INVALID_IDX;
  is_used = false;
}

} // namespace corosig
