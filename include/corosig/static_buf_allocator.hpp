#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <limits>

namespace corosig {

inline size_t padding_with_header(size_t base_address, size_t alignment, size_t header_size) {
  size_t multiplier = (base_address / alignment) + 1;
  size_t aligned_address = multiplier * alignment;
  size_t padding = aligned_address - base_address;
  size_t needed_space = header_size;

  if (padding < needed_space) {
    // Header does not fit - Calculate next aligned address that header fits
    needed_space -= padding;

    // How many alignments I need to fit the header
    if (needed_space % alignment > 0) {
      padding += alignment * (1 + (needed_space / alignment));
    } else {
      padding += alignment * (needed_space / alignment);
    }
  }

  return padding;
}

template <size_t SIZE>
struct StaticBufAllocator {
private:
  struct FreeHeader {
    size_t blockSize = 0;
  };

  struct FreeList {
    struct Node {
      FreeHeader data;
      Node *next = nullptr;
    };

    Node *head = nullptr;

    void insert(Node *previousNode, Node *newNode) noexcept {
      if (previousNode == nullptr) {
        // Is the first node
        if (head != nullptr) {
          // The list has more elements
          newNode->next = head;
        } else {
          newNode->next = nullptr;
        }
        head = newNode;
      } else {
        if (previousNode->next == nullptr) {
          // Is the last node
          previousNode->next = newNode;
          newNode->next = nullptr;
        } else {
          // Is a middle node
          newNode->next = previousNode->next;
          previousNode->next = newNode;
        }
      }
    }

    void remove(Node *previousNode, Node *deleteNode) noexcept {
      if (previousNode == nullptr) {
        // Is the first node
        if (deleteNode->next == nullptr) {
          // List only has one element
          head = nullptr;
        } else {
          // List has more elements
          head = deleteNode->next;
        }
      } else {
        previousNode->next = deleteNode->next;
      }
    }
  };

  struct AllocationHeader {
    size_t block_size = 0;
    char padding;
  };

  using Node = typename FreeList::Node;

  size_t m_used = 0;
  size_t m_peak = 0;
  FreeList m_free_list = {};
  alignas(Node) std::array<char, SIZE> m_mem{0};

public:
  constexpr static size_t MIN_ALIGNMENT = 8;

  StaticBufAllocator() noexcept {
    Node *firstNode = new (m_mem.data()) Node{};
    firstNode->data.blockSize = SIZE - sizeof(Node);
    firstNode->next = nullptr;
    m_free_list.insert(nullptr, firstNode);
  }

  void *allocate(size_t size, size_t alignment = MIN_ALIGNMENT) noexcept {
    size = std::max(size, sizeof(Node));
    alignment = std::max(alignment, MIN_ALIGNMENT);

    assert("Allocation size must be bigger" && size >= sizeof(Node));
    assert("Alignment must be 8 at least" && alignment >= 8);
    assert("Alignment must be a power of 2" && std::has_single_bit(alignment));

    // Search through the free list for a free block that has enough space to allocate our data
    size_t padding;
    Node *affected_node;
    Node *previous_node;
    find(size, alignment, padding, previous_node, affected_node);
    if (affected_node == nullptr) {
      return nullptr;
    }

    size_t alignmentPadding = padding - sizeof(AllocationHeader);
    size_t requiredSize = size + padding;

    size_t rest = affected_node->data.blockSize - requiredSize;

    if (rest > 0) {
      // We have to split the block into the data block and a free block of size 'rest'
      Node *new_free_node = (Node *)((size_t)affected_node + requiredSize);
      new_free_node->data.blockSize = rest;
      m_free_list.insert(affected_node, new_free_node);
    }
    m_free_list.remove(previous_node, affected_node);

    // Setup data block
    size_t header_address = (size_t)affected_node + alignmentPadding;
    size_t data_address = header_address + sizeof(AllocationHeader);
    std::bit_cast<AllocationHeader *>(header_address)->block_size = requiredSize;
    std::bit_cast<AllocationHeader *>(header_address)->padding = alignmentPadding;

    m_used += requiredSize;
    m_peak = std::max(m_peak, m_used);

    return (void *)data_address;
  }

  void free(void *ptr) noexcept {
    if (ptr == nullptr) {
      return;
    }

    // Insert it in a sorted position by the address number
    size_t current_address = (size_t)ptr;
    size_t header_address = current_address - sizeof(StaticBufAllocator::AllocationHeader);
    auto *allocation_header = std::bit_cast<AllocationHeader *>(header_address);

    Node *freenode = std::bit_cast<Node *>(header_address);
    freenode->data.blockSize = allocation_header->block_size + allocation_header->padding;
    freenode->next = nullptr;

    Node *it = m_free_list.head;
    Node *itPrev = nullptr;
    while (it != nullptr) {
      if (ptr < it) {
        m_free_list.insert(itPrev, freenode);
        break;
      }
      itPrev = it;
      it = it->next;
    }

    m_used -= freenode->data.blockSize;

    // Merge contiguous nodes
    coalescence(itPrev, freenode);
  }

private:
  StaticBufAllocator(StaticBufAllocator &freeListAllocator);

  void coalescence(Node *prevNode, Node *freeNode) noexcept {
    if (freeNode->next != nullptr &&
        size_t(freeNode) + freeNode->data.blockSize == size_t(freeNode->next)) {
      freeNode->data.blockSize += freeNode->next->data.blockSize;
      m_free_list.remove(freeNode, freeNode->next);
    }

    if (prevNode != nullptr && size_t(prevNode) + prevNode->data.blockSize == size_t(freeNode)) {
      prevNode->data.blockSize += freeNode->data.blockSize;
      m_free_list.remove(prevNode, freeNode);
    }
  }

  void find(size_t size, size_t alignment, size_t &padding, Node *&previousNode,
            Node *&foundNode) noexcept {
    Node *it = m_free_list.head;
    Node *itPrev = nullptr;

    while (it != nullptr) {
      padding = padding_with_header(size_t(it), alignment, sizeof(AllocationHeader));
      size_t requiredSpace = size + padding;
      if (it->data.blockSize >= requiredSpace) {
        break;
      }
      itPrev = it;
      it = it->next;
    }
    previousNode = itPrev;
    foundNode = it;
  }
};

} // namespace corosig
