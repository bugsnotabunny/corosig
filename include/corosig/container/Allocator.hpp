#ifndef COROSIG_ALLOC_HPP
#define COROSIG_ALLOC_HPP

#include <array>
#include <cassert>
#include <cstddef>

namespace corosig {

/// @brief An allocator which allocates memory from some non-resizeable buffer. If buffer runs out
///        of space, allocations fail. No new memory is allocated
struct Allocator {
private:
  struct FreeHeader {
    size_t block_size = 0;
  };

  struct FreeList {
    struct Node {
      FreeHeader data;
      Node *next = nullptr;
    };

    Node *head = nullptr;

    void insert(Node *previousNode, Node *newNode) noexcept;
    void remove(Node *previousNode, Node *deleteNode) noexcept;
  };

  struct AllocationHeader {
    size_t block_size = 0;
    size_t padding = 0;
  };

  using Node = typename FreeList::Node;

public:
  /// @brief Properly-aligned memory buffer type
  template <size_t SIZE>
  struct alignas(Node) Memory : public std::array<char, SIZE> {};

  constexpr static size_t MIN_ALIGNMENT = 8;

  /// @brief Construct an Allocator for which allocations always fail
  Allocator() noexcept = default;

  /// @brief Construct an Allocator over specified memory buffer
  template <size_t SIZE>
  Allocator(Memory<SIZE> &mem) noexcept
      : m_mem{mem.begin()},
        m_mem_size{mem.size()} {
    Node *first_node = new (m_mem) Node{};
    first_node->data.block_size = SIZE - sizeof(Node);
    first_node->next = nullptr;
    m_free_list.insert(nullptr, first_node);
  }

  Allocator(const Allocator &) = delete;
  Allocator(Allocator &&) noexcept;
  Allocator &operator=(const Allocator &) = delete;
  Allocator &operator=(Allocator &&) noexcept;

  ~Allocator();

  /// @brief Get the maximum amount of memory used, in bytes
  [[nodiscard]] size_t peak_memory() const noexcept;

  /// @brief Get the amount of currently used memory, in bytes
  [[nodiscard]] size_t current_memory() const noexcept;

  /// @brief Allocate a chunk of memory of specified size and alignment
  /// @returns A pointer to allocated buffer or nullptr if an allocation has failed
  /// @warning Is UB if alignment is not a power of 2
  [[nodiscard]] void *allocate(size_t size, size_t alignment = MIN_ALIGNMENT) noexcept;

  /// @brief Deallocate a chunk which begins at ptr
  /// @warning UB if ptr does not point to the chunk owned by this allocator
  void deallocate(void *ptr) noexcept;

private:
  void coalescence(Node *prevNode, Node *freeNode) noexcept;
  void find(size_t size,
            size_t alignment,
            size_t &padding,
            Node *&previousNode,
            Node *&foundNode) const noexcept;

  size_t m_used = 0;
  size_t m_peak = 0;
  FreeList m_free_list = {};
  char *m_mem = nullptr;
  size_t m_mem_size = 0;
};

} // namespace corosig

#endif
