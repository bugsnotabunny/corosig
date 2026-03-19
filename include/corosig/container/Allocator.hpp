#ifndef COROSIG_ALLOC_HPP
#define COROSIG_ALLOC_HPP

#include "corosig/util/SetDefaultOnMove.hpp"

#include <array>
#include <boost/intrusive/avl_set.hpp>
#include <boost/intrusive/avl_set_hook.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/options.hpp>
#include <cassert>
#include <cstddef>
#include <span>

namespace corosig {

/// @brief An allocator which allocates memory from some non-resizeable buffer. If buffer runs out
///        of space, allocations fail. No new memory is allocated
struct Allocator {
public:
  /// @brief Memory buffer type
  template <size_t SIZE>
  using Memory = std::array<char, SIZE>;

  constexpr static size_t MIN_ALIGNMENT = 1;

  /// @brief Construct an Allocator for which allocations always fail
  Allocator() noexcept = default;

  /// @brief Construct an Allocator over specified memory buffer
  Allocator(std::span<char> mem) noexcept;

  Allocator(const Allocator &) = delete;
  Allocator(Allocator &&) noexcept = default;
  Allocator &operator=(const Allocator &) = delete;
  Allocator &operator=(Allocator &&) noexcept = default;

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
  struct FreeNode {
    struct CompareBlockSize {
      bool operator()(FreeNode const &lhs, FreeNode const &rhs) const noexcept {
        return lhs.block_size < rhs.block_size;
      }
    };

    struct CompareAddress {
      bool operator()(FreeNode const &lhs, FreeNode const &rhs) const noexcept {
        return &lhs < &rhs;
      }
    };

    using hook_type = boost::intrusive::avl_set_member_hook<
        boost::intrusive::optimize_size<true>,
        boost::intrusive::link_mode<boost::intrusive::link_mode_type::auto_unlink>>;

    hook_type nodes_by_addr_hook = {};
    hook_type nodes_by_size_hook = {};
    size_t block_size;
  };

  void link(FreeNode &) noexcept;
  static void unlink_and_destroy(FreeNode &) noexcept;
  void maybe_merge_with_next(FreeNode &) noexcept;

  using nodes_by_size_type = boost::intrusive::avl_multiset<
      FreeNode,
      boost::intrusive::compare<FreeNode::CompareBlockSize>,
      boost::intrusive::constant_time_size<false>,
      boost::intrusive::member_hook<FreeNode, FreeNode::hook_type, &FreeNode::nodes_by_size_hook>>;

  using nodes_by_addr_type = boost::intrusive::avl_multiset<
      FreeNode,
      boost::intrusive::compare<FreeNode::CompareAddress>,
      boost::intrusive::constant_time_size<false>,
      boost::intrusive::member_hook<FreeNode, FreeNode::hook_type, &FreeNode::nodes_by_addr_hook>>;

  nodes_by_size_type m_nodes_by_size;
  nodes_by_addr_type m_nodes_by_addr;
  SetDefaultOnMove<size_t> m_used;
  SetDefaultOnMove<size_t> m_peak;
  SetDefaultOnMove<char *> m_mem;
};

} // namespace corosig

#endif
