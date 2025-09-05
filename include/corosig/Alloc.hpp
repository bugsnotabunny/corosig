#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <limits>
#include <span>

namespace corosig {

struct Alloc {
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
    char padding;
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

  Alloc() noexcept = default;

  template <size_t SIZE>
  Alloc(Memory<SIZE> &mem) noexcept : m_mem{mem.begin()}, m_mem_size{mem.size()} {
    Node *firstNode = new (m_mem) Node{};
    firstNode->data.block_size = SIZE - sizeof(Node);
    firstNode->next = nullptr;
    m_free_list.insert(nullptr, firstNode);
  }

  Alloc(const Alloc &) = delete;
  Alloc(Alloc &&) noexcept;
  Alloc &operator=(const Alloc &) = delete;
  Alloc &operator=(Alloc &&) noexcept;
  ~Alloc();

  size_t peak_memory() noexcept {
    return m_peak;
  }

  size_t current_memory() noexcept {
    return m_peak;
  }

  void *allocate(size_t size, size_t alignment = MIN_ALIGNMENT) noexcept;
  void free(void *ptr) noexcept;

private:
  void coalescence(Node *prevNode, Node *freeNode) noexcept;
  void find(size_t size, size_t alignment, size_t &padding, Node *&previousNode,
            Node *&foundNode) noexcept;
};

} // namespace corosig
