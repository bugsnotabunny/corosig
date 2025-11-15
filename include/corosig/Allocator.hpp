#ifndef COROSIG_ALLOC_HPP
#define COROSIG_ALLOC_HPP

#include <array>
#include <cassert>
#include <cstddef>

namespace corosig {

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
  template <size_t SIZE>
  struct alignas(Node) Memory : public std::array<char, SIZE> {};

private:
  size_t m_used = 0;
  size_t m_peak = 0;
  FreeList m_free_list = {};
  char *m_mem = nullptr;
  size_t m_mem_size = 0;

public:
  constexpr static size_t MIN_ALIGNMENT = 8;

  Allocator() noexcept = default;

  template <size_t SIZE>
  Allocator(Memory<SIZE> &mem) noexcept : m_mem{mem.begin()}, m_mem_size{mem.size()} {
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

  [[nodiscard]] size_t peak_memory() const noexcept {
    return m_peak;
  }

  [[nodiscard]] size_t current_memory() const noexcept {
    return m_peak;
  }

  [[nodiscard]] void *allocate(size_t size, size_t alignment = MIN_ALIGNMENT) noexcept;
  void free(void *ptr) noexcept;

private:
  void coalescence(Node *prevNode, Node *freeNode) noexcept;
  void find(size_t size, size_t alignment, size_t &padding, Node *&previousNode,
            Node *&foundNode) const noexcept;
};

} // namespace corosig

#endif
