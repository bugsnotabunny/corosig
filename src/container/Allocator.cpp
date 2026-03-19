#include "corosig/container/Allocator.hpp"

#include "corosig/meta/AnAllocator.hpp"

#include <concepts>
#include <cstdint>
#include <limits>

namespace {

using namespace corosig;

static_assert(AnAllocator<Allocator>);

struct AllocationHeader {
  // located after header
  uint32_t block_size;
  // located before header
  uint32_t padding;
};

char *align_left(char *p, size_t align) noexcept {
  return p - (size_t(p) % align);
}

char *align_right(char *p, size_t align) noexcept {
  return size_t(p) % align == 0 ? p : align_left(p + align, align);
}

template <std::unsigned_integral T>
T sub_sat(T a, T b) noexcept {
  if (a < b) {
    return 0;
  }
  return a - b;
}

} // namespace

namespace corosig {

Allocator::Allocator(std::span<char> mem) noexcept {
  char *aligned_mem_start = align_right(mem.data(), alignof(FreeNode));
  m_mem = aligned_mem_start;

  size_t mem_size = mem.size() - size_t(aligned_mem_start - mem.data());
  if (mem_size >= sizeof(FreeNode)) {
    new (*m_mem) FreeNode{
        .block_size = mem_size - sizeof(AllocationHeader),
    };
    link(*(FreeNode *)*m_mem);
  }
}

Allocator::~Allocator() {
  assert(*m_used == 0 && "Memory leak detected");
}

size_t Allocator::peak_memory() const noexcept {
  return *m_peak;
}

size_t Allocator::current_memory() const noexcept {
  return *m_used;
}

void Allocator::link(FreeNode &node) noexcept {
  m_nodes_by_addr.insert(node);
  m_nodes_by_size.insert(node);
}

void Allocator::unlink_and_destroy(FreeNode &node) noexcept {

  node.nodes_by_addr_hook.unlink();
  node.nodes_by_size_hook.unlink();
  node.~FreeNode();
}

void *Allocator::allocate(size_t size, size_t alignment) noexcept {
  assert(std::has_single_bit(alignment) && "Alignment must be a power of 2");

  // we should always be able to construct FreeNode in allocation place
  // to make deallocate implementation more trivial
  size = std::max(size, sizeof(FreeNode) - sizeof(AllocationHeader));

  char *allocated_block = nullptr;

  for (auto it = m_nodes_by_size.lower_bound(FreeNode{.block_size = size});
       it != m_nodes_by_size.end();
       ++it) {
    char *const node_addr = (char *)&*it;
    char *allocation_header_addr = align_right(node_addr, alignof(AllocationHeader));
    char *const allocated_block_addr = allocation_header_addr + sizeof(AllocationHeader);
    char *const aligned_allocated_block_addr = align_right(allocated_block_addr, alignment);
    size_t const padding = aligned_allocated_block_addr - allocated_block_addr;
    if (padding > 0) {
      // make header lie right before allocated block. required to access it during deallocation
      allocation_header_addr += padding;
      assert(size_t(allocation_header_addr) % alignof(AllocationHeader) == 0);
    }
    size_t const aligned_allocated_block_size = ((FreeNode *)node_addr)->block_size - padding;

    if (aligned_allocated_block_size < size) {
      continue;
    }

    char *const new_free_node_addr =
        align_right(aligned_allocated_block_addr + size, alignof(FreeNode));
    size_t actually_allocated_size = new_free_node_addr - aligned_allocated_block_addr;
    size_t const extra_free_space = sub_sat(aligned_allocated_block_size, actually_allocated_size);

    if (extra_free_space < sizeof(FreeNode)) {
      // take whole block. don't divide it
      actually_allocated_size = aligned_allocated_block_size;
    } else {
      new (new_free_node_addr) FreeNode{.block_size = extra_free_space - sizeof(AllocationHeader)};
      link(*(FreeNode *)new_free_node_addr);
    }

    unlink_and_destroy(*(FreeNode *)node_addr);

    assert(padding <= std::numeric_limits<uint32_t>::max());
    assert(actually_allocated_size <= std::numeric_limits<uint32_t>::max());

    new (allocation_header_addr) AllocationHeader{
        .block_size = uint32_t(actually_allocated_size),
        .padding = uint32_t(padding),
    };

    allocated_block = aligned_allocated_block_addr;
    *m_used += actually_allocated_size + padding + sizeof(AllocationHeader);
    *m_peak = std::max(*m_peak, *m_used);
    break;
  }

  assert(size_t(allocated_block) % alignment == 0);
  return allocated_block;
}

void Allocator::deallocate(void *ptr) noexcept {
  if (ptr == nullptr) {
    return;
  }

  char *const current_addr = (char *)ptr;
  char *const header_addr = current_addr - sizeof(AllocationHeader);
  size_t const padding = ((AllocationHeader *)header_addr)->padding;
  size_t const block_size = ((AllocationHeader *)header_addr)->block_size;
  char *const node_addr = align_left(header_addr - padding, alignof(FreeNode));

  new (node_addr) FreeNode{
      .block_size = block_size + padding,
  };
  auto *node = (FreeNode *)node_addr;
  link(*node);

  assert(*m_used >= block_size &&
         "Double free detected. Also there might have been other double frees before that");
  *m_used -= block_size + padding + sizeof(AllocationHeader);

  // merge with neighbours if possible
  auto iter = m_nodes_by_addr.iterator_to(*node);

  do {
    auto iter_before = iter;
    --iter_before;

    if (iter_before == m_nodes_by_addr.end()) {
      break;
    }

    if ((char *)&*iter_before + sizeof(AllocationHeader) + iter_before->block_size ==
        (char *)&*iter) {
      iter_before->block_size += sizeof(AllocationHeader) + iter->block_size;
      unlink_and_destroy(*iter);
      iter = iter_before;
    }
  } while (false);

  do {
    auto iter_next = iter;
    ++iter_next;

    if (iter_next == m_nodes_by_addr.end()) {
      break;
    }

    if ((char *)&*iter + sizeof(AllocationHeader) + iter->block_size == (char *)&*iter_next) {
      iter->block_size += sizeof(AllocationHeader) + iter_next->block_size;
      unlink_and_destroy(*iter_next);
    }
  } while (false);
}

} // namespace corosig
