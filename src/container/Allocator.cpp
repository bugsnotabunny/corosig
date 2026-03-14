#include "corosig/container/Allocator.hpp"

#include "corosig/meta/AnAllocator.hpp"

#include <utility>

namespace {

using namespace corosig;

static_assert(AnAllocator<Allocator>);

char *align_right(char *p, size_t align) noexcept {
  return p + align - (size_t(p) % align);
}

size_t padding_with_header(size_t base_address, size_t alignment, size_t header_size) noexcept {
  size_t multiplier = (base_address / alignment) + 1;
  size_t aligned_address = multiplier * alignment;
  size_t padding = aligned_address - base_address;
  size_t needed_space = header_size;

  if (padding < needed_space) {
    needed_space -= padding;

    if (needed_space % alignment > 0) {
      padding += alignment * (1 + (needed_space / alignment));
    } else {
      padding += alignment * (needed_space / alignment);
    }
  }

  return padding;
}

} // namespace

namespace corosig {

Allocator::Allocator(Allocator &&rhs) noexcept
    : m_used{std::exchange(rhs.m_used, 0)},
      m_peak{std::exchange(rhs.m_peak, 0)},
      m_free_list{std::exchange(rhs.m_free_list, FreeList{})},
      m_mem{std::exchange(rhs.m_mem, nullptr)},
      m_mem_size{std::exchange(rhs.m_mem_size, 0)} {
}

Allocator &Allocator::operator=(Allocator &&rhs) noexcept {
  this->~Allocator();
  new (this) Allocator{std::move(rhs)};
  return *this;
}

Allocator::~Allocator() {
  assert(m_used == 0 && "Memory leak detected");
}

size_t Allocator::peak_memory() const noexcept {
  return m_peak;
}

size_t Allocator::current_memory() const noexcept {
  return m_used;
}

void Allocator::FreeList::insert(Node *previousNode, Node *newNode) noexcept {
  if (previousNode == nullptr) {
    if (head != nullptr) {
      newNode->next = head;
    } else {
      newNode->next = nullptr;
    }
    head = newNode;
  } else {
    if (previousNode->next == nullptr) {
      previousNode->next = newNode;
      newNode->next = nullptr;
    } else {
      newNode->next = previousNode->next;
      previousNode->next = newNode;
    }
  }
}

void Allocator::FreeList::remove(Node *previousNode, Node *deleteNode) noexcept {
  if (previousNode == nullptr) {
    if (deleteNode->next == nullptr) {
      head = nullptr;
    } else {
      head = deleteNode->next;
    }
  } else {
    previousNode->next = deleteNode->next;
  }
}

void *Allocator::allocate(size_t size, size_t alignment) noexcept {
  assert("Alignment must be a power of 2" && std::has_single_bit(alignment));

  size = std::max(size, sizeof(Node));
  alignment = std::max(alignment, MIN_ALIGNMENT);

  assert("Allocation size must be bigger" && size >= sizeof(Node));
  assert("Alignment must be 8 at least" && alignment >= 8);

  size_t padding;
  Node *affected_node;
  Node *previous_node;
  find(size, alignment, padding, previous_node, affected_node);
  if (affected_node == nullptr) {
    return nullptr;
  }

  size_t alignment_padding = padding - sizeof(AllocationHeader);
  size_t required_size = size + padding;
  size_t rest = affected_node->data.block_size - required_size;

  if (rest > 0) {
    char *new_free_node_addr = align_right((char *)affected_node + required_size, alignof(Node));
    if (new_free_node_addr + sizeof(Node) < m_mem + m_mem_size) {
      Node *new_free_node = (Node *)new_free_node_addr;
      new_free_node->data.block_size = rest;
      m_free_list.insert(affected_node, new_free_node);
    }
  }
  m_free_list.remove(previous_node, affected_node);

  auto *header_ptr = (AllocationHeader *)((char *)affected_node + alignment_padding);
  header_ptr->block_size = required_size;
  header_ptr->padding = alignment_padding;

  m_used += required_size;
  m_peak = std::max(m_peak, m_used);
  return (void *)((char *)header_ptr + sizeof(AllocationHeader));
}

void Allocator::deallocate(void *ptr) noexcept {
  if (ptr == nullptr) {
    return;
  }

  auto current_address = size_t(ptr);
  size_t header_address = current_address - sizeof(AllocationHeader);
  auto *allocation_header = std::bit_cast<AllocationHeader *>(header_address);

  Node *freenode = std::bit_cast<Node *>(header_address);
  freenode->data.block_size = allocation_header->block_size + allocation_header->padding;
  freenode->next = nullptr;
  assert(freenode);

  Node *it = m_free_list.head;
  Node *it_prev = nullptr;
  while (it != nullptr) {
    if (ptr < it) {
      m_free_list.insert(it_prev, freenode);
      break;
    }
    it_prev = it;
    it = it->next;
  }

  assert(m_used >= freenode->data.block_size && "Double free detected");
  m_used -= freenode->data.block_size;

  // Merge contiguous nodes
  coalescence(it_prev, freenode);
}

void Allocator::coalescence(Node *prevNode, Node *freeNode) noexcept {
  if (freeNode->next != nullptr &&
      size_t(freeNode) + freeNode->data.block_size == size_t(freeNode->next)) {
    freeNode->data.block_size += freeNode->next->data.block_size;
    m_free_list.remove(freeNode, freeNode->next);
  }

  if (prevNode != nullptr && size_t(prevNode) + prevNode->data.block_size == size_t(freeNode)) {
    prevNode->data.block_size += freeNode->data.block_size;
    m_free_list.remove(prevNode, freeNode);
  }
}

void Allocator::find(size_t size,
                     size_t alignment,
                     size_t &padding,
                     Node *&previousNode,
                     Node *&foundNode) const noexcept {
  Node *it = m_free_list.head;
  Node *it_prev = nullptr;

  while (it != nullptr) {
    padding = padding_with_header(size_t(it), alignment, sizeof(AllocationHeader));
    size_t required_space = size + padding;
    if (it->data.block_size >= required_space) {
      break;
    }
    it_prev = it;
    it = it->next;
  }
  previousNode = it_prev;
  foundNode = it;
}

} // namespace corosig
