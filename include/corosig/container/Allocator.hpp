#ifndef COROSIG_ALLOC_HPP
#define COROSIG_ALLOC_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace corosig {

/// @brief An allocator which allocates memory from some non-resizeable buffer. If buffer runs out
///        of space, allocations fail. No new memory is allocated
struct Allocator {
private:
  constexpr static size_t BLOCK_SIZE = 16;

public:
  /// @brief Efficiently-aligned and sized memory buffer type
  template <size_t SIZE>
  struct alignas(BLOCK_SIZE) Memory : std::array<char, SIZE - SIZE % BLOCK_SIZE> {};

  /// @brief Construct an Allocator for which allocations always fail
  Allocator() noexcept = default;

  /// @brief Construct an Allocator over specified memory buffer
  Allocator(std::span<char> mem) noexcept;

  Allocator(Allocator const &) = delete;
  Allocator(Allocator &&) noexcept = delete;
  Allocator &operator=(Allocator const &) = delete;
  Allocator &operator=(Allocator &&) noexcept = delete;

  ~Allocator();

  /// @brief Get the maximum amount of memory used, in bytes
  [[nodiscard]] size_t peak_memory() const noexcept;

  /// @brief Get the amount of currently used memory, in bytes
  [[nodiscard]] size_t current_memory() const noexcept;

  /// @brief Allocate a chunk of memory of specified size and alignment
  /// @returns A pointer to allocated buffer or nullptr if an allocation has failed
  /// @warning Is UB if alignment is not a power of 2
  [[nodiscard]] void *allocate(size_t size, size_t alignment) noexcept;

  /// @brief Deallocate a chunk which begins at ptr
  /// @warning UB if ptr does not point to the chunk owned by this allocator
  void deallocate(void *ptr) noexcept;

private:
  struct BlockMetadata {
    void *get_mem() noexcept;
    void default_initialize() noexcept;

    uint32_t blocks_before : 31;
    bool is_used : 1 = false;
    uint32_t blocks_owned;
    uint32_t prev_free_block_idx = INVALID_IDX;
    uint32_t next_free_block_idx = INVALID_IDX;
  };

  void set_blocks_owned(size_t idx, uint32_t value) noexcept;
  size_t blocks_amount() const noexcept;
  size_t get_metadata_idx_from_addr(void *p) noexcept;
  BlockMetadata &get_block_metadata(size_t idx) noexcept;
  void push_free_node(size_t idx) noexcept;
  void unlink_free_node(size_t idx) noexcept;

  constexpr static auto INVALID_IDX = static_cast<uint32_t>((2 << 31) - 1);

  uint32_t m_first_free_block_idx = INVALID_IDX;
  uint32_t m_last_free_block_idx = INVALID_IDX;
  std::span<char> m_mem;
  size_t m_used = 0;
  size_t m_peak = 0;
};

} // namespace corosig

#endif
