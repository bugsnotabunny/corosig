#ifndef COROSIG_CONTAINER_BACK_INSERTER_HPP
#define COROSIG_CONTAINER_BACK_INSERTER_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"

#include <cstddef>
#include <functional>
#include <utility>

namespace corosig {

template <typename CONTAINER>
struct BackInserter {
private:
  constexpr static auto push_to_container(CONTAINER &container,
                                          CONTAINER::value_type &&value) noexcept {
    return container.push_back(std::move(value));
  }

  using push_back_error_type =
      error_type<push_to_container, CONTAINER &, typename CONTAINER::value_type &&>;

public:
  using difference_type = std::ptrdiff_t;
  using value_type = BackInserter;

  explicit constexpr BackInserter(CONTAINER &container) noexcept
      : m_container{container} {
  }

  constexpr BackInserter &operator=(CONTAINER::value_type &&value) noexcept {
    if (!m_result) {
      return *this;
    }
    m_result = push_to_container(m_container.get(), std::move(value));
    return *this;
  }

  constexpr BackInserter &operator*() noexcept {
    return *this;
  }

  constexpr BackInserter &operator++() noexcept {
    return *this;
  }

  constexpr BackInserter operator++(int) noexcept {
    auto it = *this;
    ++(*this);
    return it;
  }

  constexpr Result<void, push_back_error_type> const &result() const noexcept {
    return m_result;
  }

  constexpr Result<void, push_back_error_type> &result() noexcept {
    return m_result;
  }

private:
  std::reference_wrapper<CONTAINER> m_container;
  Result<void, push_back_error_type> m_result;
};

} // namespace corosig

#endif
