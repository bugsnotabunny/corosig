#include <bit>
#include <cassert>
#include <cctype>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#pragma once

namespace corosig::detail {

// clang-format off
    template <uintmax_t VALUE>
    using uint_value_t =
        std::conditional_t<VALUE <= std::numeric_limits<uint8_t>::max() ,
        uint8_t, std::conditional_t<VALUE <=
        std::numeric_limits<uint16_t>::max() , uint16_t,
        std::conditional_t<VALUE <= std::numeric_limits<uint32_t>::max() ,
        uint32_t, std::conditional_t<VALUE <=
        std::numeric_limits<uint64_t>::max() , uint64_t, uintmax_t>>>>;
// clang-format on

} // namespace corosig::detail

namespace corosig {

template <size_t SIZE, size_t ALIGN = alignof(max_align_t)>
struct static_buf_allocator {
private:
  // 1 index value is reserved as npos
  using memory_t = std::array<std::byte, SIZE - 1>;
  using index_t = detail::uint_value_t<SIZE - 1>;
  constexpr static index_t npos = SIZE - 1;

  struct block {
    std::byte *mem_end() noexcept {
      return mem_begin() + size;
    }

    std::byte *mem_begin() noexcept {
      return std::launder(reinterpret_cast<std::byte *>(this + 1));
    }

    bool contains(std::byte *p) noexcept {
      return this < p && mem_end() > p;
    }

    constexpr void split_at(std::byte *p) noexcept {
      assert(contains(p) && contains(p + sizeof(block)));
      index_t old_size = std::exchange(size, p - mem_begin());
      new (p) block{
          .size = old_size - size,
          .bytes_before = size,
      };
    }

    index_t right = npos;
    index_t left = npos;
    index_t size = 0;
    bool is_used = false;
  };

  alignas(ALIGN) memory_t m_memory;
  block *m_root;

public:
  static_buf_allocator() noexcept {
    new (m_memory.data()) block{
        .size = SIZE - sizeof(block),
    };
  }

  static static_buf_allocator zeroed() noexcept {
    static_buf_allocator alloc;
    std::memset(alloc.m_memory.data() + sizeof(block), 0, SIZE - sizeof(block));
    return alloc;
  }

  std::byte *allocate(size_t size, size_t align) noexcept {
    assert(std::has_single_bit(align) && "Alignment must be a power of 2");
    size = std::max(size, align);
    block *root = find_best_fit(size);

    // void *buf = root->mem_begin();
    // size_t buf_size = root->size;

    // void *aligned_ptr = std::align(align, size, buf, buf_size);
    // if (!aligned_ptr) {
    //   return nullptr;
    // }

    // if (buf_size < size + sizeof(block)) {
    //   return nullptr;
    // }

    // std::byte *aligned_byte_ptr = static_cast<std::byte *>(aligned_ptr);
    // std::byte *aprox_allocation_place =
    //     aligned_byte_ptr + size * ((root->mem_end() - aligned_byte_ptr) / size - 1);

    // std::byte *allocation_place =
    //     aprox_allocation_place +
    //     align * ((root->mem_end() - (aprox_allocation_place + size)) / align);

    // std::byte *new_block_place = allocation_place - sizeof(block);

    // root->size -= root->mem_end() - new_block_place;
    // return allocation_place;
  }

  void deallocate(std::byte *obj) noexcept {
    // for (std::reference_wrapper<free_space_node> node = &root(); &node.get() < m_memory.end();
    //      node = node.get().next()) {

    //   if (!node.get().contains)

    //     allocation_header header;
    //   std::memcpy(&header, obj - sizeof(header), sizeof(header));
    // }
  }

  std::span<std::byte const> view_mem() const noexcept {
    return m_memory;
  }

private:
  block *bptr(index_t i) noexcept {
    return i != npos ? std::launder(reinterpret_cast<block *>(m_memory.data() + i)) : nullptr;
  }

  std::byte *aligned(std::byte *p, size_t align) noexcept {
    assert(std::has_single_bit(align));
    assert(p != nullptr);

    auto addr = std::bit_cast<uintptr_t>(p);
    if (addr % align != 0) {
      addr += align - addr % align;
    }
    return std::bit_cast<std::byte *>(addr);
  }

  block *find_best_fit(size_t size, size_t align) noexcept {
    block *root = m_root;
    while (root) {
      if (size <= root->size) {
        if (block *b = bptr(root->left); b && size <= b->size) {
          root = b;
        } else {
          return root;
        }
      } else {
        root = bptr(root->right);
      }
    }
  }
};

} // namespace corosig
