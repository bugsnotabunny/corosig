#ifndef COROSIG_CONTAINER_VECTOR_HPP
#define COROSIG_CONTAINER_VECTOR_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/meta/AnAllocator.hpp"
#include "corosig/meta/Copyable.hpp"
#include "corosig/meta/Lambdize.hpp"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

namespace corosig {

/// Mostly STL-compatible vector class.
template <typename T, AnAllocator ALLOCATOR = Allocator &>
  requires std::is_nothrow_move_constructible_v<T>
struct Vector {

public:
  using value_type = T;
  using allocator_type = ALLOCATOR;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type &;
  using const_reference = value_type const &;
  using pointer = value_type *;
  using const_pointer = value_type const *;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
  template <typename R, typename E>
  using result_extended_with_clone_errors =
      Result<R, extend_error<E, error_type<COROSIG_LAMBDIZE(corosig::clone), value_type const &>>>;

public:
  Vector(ALLOCATOR &&alloc) noexcept : m_alloc{std::forward<ALLOCATOR>(alloc)} {
  }

  Vector(const Vector &) noexcept = delete;
  Vector &operator=(const Vector &) = delete;

  Vector(Vector &&rhs) noexcept
      : m_data{std::exchange(rhs.m_data, nullptr)}, m_size{std::exchange(rhs.m_size, 0)},
        m_capacity{std::exchange(rhs.m_capacity, 0)}, m_alloc{rhs.m_alloc} {
  }

  Vector &operator=(Vector &&rhs) noexcept {
    if (std::addressof(rhs) != this) {
      this->~Vector();
      new (this) Vector{std::move(rhs)};
    }
    return *this;
  };

  ~Vector() {
    clear();
  }

  constexpr auto clone() const noexcept
    requires(Copyable<value_type>)
  {
    using Result = result_extended_with_clone_errors<Vector, AllocationError>;

    Vector copies{m_alloc};
    COROSIG_TRYTV(Result, copies.reserve(size()));
    for (value_type const &value : *this) {
      COROSIG_TRYTV(Result, copies.push_back(value));
    }
    return Result{std::move(copies)};
  }

  constexpr void clear() noexcept {
    while (size() > 0) {
      pop_back();
    }
    m_alloc.deallocate(m_data);
    m_data = nullptr;
    m_size = 0;
    m_capacity = 0;
  }

  constexpr Result<void, AllocationError> shrink_to_fit() noexcept {
    Vector new_vec{m_alloc};
    COROSIG_TRYV(new_vec.resize_uninitialized(size()));
    for (auto i : std::views::iota(size_type(0), size())) {
      new (new_vec.data() + i) value_type{std::move((m_data)[i])};
    }
    *this = std::move(new_vec);
    return Ok{};
  }

  constexpr Result<void, AllocationError> reserve(size_type count) noexcept {
    if (count <= m_capacity) {
      return Ok{};
    }

    COROSIG_TRY(pointer new_mem, allocate(count));
    for (auto i : std::views::iota(size_type(0), size())) {
      new (new_mem + i) value_type{std::move((m_data)[i])};
    }

    m_alloc.deallocate(m_data);
    m_data = new_mem;
    m_capacity = count;
    return Ok{};
  }

  constexpr auto resize(size_type count, const value_type &value = value_type{}) noexcept
    requires(Copyable<value_type>)
  {
    using Result = result_extended_with_clone_errors<void, AllocationError>;

    if (count == size()) {
      return Result{Ok{}};
    }

    if (count < size()) {
      do {
        pop_back();
      } while (count < size());
      return Result{Ok{}};
    }

    if constexpr (std::is_nothrow_copy_constructible_v<value_type>) {
      COROSIG_TRYTV(Result, reserve(size() + count));
      for (auto i : std::views::iota(size(), count)) {
        new (m_data + i) T{value};
      }
      m_size = count;
    } else {
      Vector copies{m_alloc};
      COROSIG_TRYTV(Result, copies.reserve(count));
      for ([[maybe_unused]] auto _ : std::views::iota(size(), count)) {
        COROSIG_TRYTV(Result, copies.push_back(value));
      }

      COROSIG_TRYTV(Result, reserve(size() + count));
      for (value_type &value : copies) {
        COROSIG_TRYTV(Result, push_back(std::move(value)));
      }
    }
    return Result{Ok{}};
  }

  constexpr Result<void, AllocationError> resize_uninitialized(size_type count) noexcept {
    if (count == size()) {
      return Ok{};
    }

    if (count < size()) {
      do {
        pop_back();
      } while (count < size());
      return Ok{};
    }

    COROSIG_TRYV(reserve(size() + count));
    m_size = count;
    return Ok{};
  }

  constexpr auto push_back(value_type const &value) noexcept
    requires(Copyable<value_type>)
  {
    using Result = result_extended_with_clone_errors<void, AllocationError>;
    COROSIG_TRYT(Result, value_type cloned, corosig::clone(value));
    return Result{push_back(std::move(cloned))};
  }

  constexpr Result<void, AllocationError> push_back(value_type &&value) noexcept {
    if (size() == capacity()) {
      COROSIG_TRYV(reserve(std::max<size_type>(size(), 8) * 2));
    }
    ++m_size;
    new (std::addressof(back())) T{std::move(value)};
    return Ok{};
  }

  constexpr void pop_back() noexcept {
    value_type &last = back();
    last.~value_type();
    --m_size;
  }

  constexpr reference operator[](size_type i) noexcept {
    assert(i < size() && "Out of bounds access into vector");
    return *(begin() + i);
  }

  constexpr const_reference operator[](size_type i) const noexcept {
    assert(i < size() && "Out of bounds access into vector");
    return *(begin() + i);
  }

  constexpr reference front() noexcept {
    assert(!empty() && "front() access on empty vector");
    return *begin();
  }

  constexpr const_reference front() const noexcept {
    assert(!empty() && "front() access on empty vector");
    return *begin();
  }

  constexpr reference back() noexcept {
    assert(!empty() && "back() access on empty vector");
    return *(end() - 1);
  }

  constexpr const_reference back() const noexcept {
    assert(!empty() && "back() access on empty vector");
    return *(end() - 1);
  }

  constexpr pointer data() noexcept {
    return m_data;
  }

  constexpr const_pointer data() const noexcept {
    return m_data;
  }

  constexpr iterator begin() noexcept {
    return m_data;
  }

  constexpr const_iterator begin() const noexcept {
    return m_data;
  }

  constexpr iterator end() noexcept {
    return m_data + size();
  }

  constexpr const_iterator end() const noexcept {
    return m_data + size();
  }

  [[nodiscard]] constexpr bool empty() const noexcept {
    return m_size == 0;
  }

  [[nodiscard]] constexpr size_type size() const noexcept {
    return m_size;
  }

  [[nodiscard]] constexpr size_type capacity() const noexcept {
    return m_capacity;
  }

  [[nodiscard]] constexpr static size_type max_size() noexcept {
    return std::numeric_limits<size_type>::max();
  }

  [[nodiscard]] constexpr allocator_type &get_allocator() noexcept {
    return m_alloc;
  }

private:
  Result<pointer, AllocationError> allocate(size_type count) noexcept {
    auto new_mem =
        static_cast<pointer>(m_alloc.allocate(count * sizeof(size_type), alignof(size_type)));
    if (new_mem == nullptr) {
      return Failure{AllocationError{}};
    }
    return new_mem;
  }

  value_type *m_data = nullptr;
  size_type m_size = 0;
  size_type m_capacity = 0;
  [[no_unique_address]] ALLOCATOR m_alloc;
};

} // namespace corosig

// constexpr iterator erase(const_iterator pos) noexcept;
// constexpr iterator erase(const_iterator first, const_iterator last) noexcept;

// constexpr Result<iterator, AllocationError> insert(const_iterator pos, T &&value) noexcept {
//   return insert(pos, std::views::single(std::forward<T>(value)));
// }

// template <std::ranges::range RANGE>
// constexpr Result<iterator, AllocationError> insert(const_iterator pos, RANGE &&values) noexcept {
// }

// template <std::ranges::range RANGE>
// constexpr Result<void, AllocationError> assign(RANGE &&values) noexcept {
// }

#endif
