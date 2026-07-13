#ifndef COROSIG_PROMISE_HPP
#define COROSIG_PROMISE_HPP

#include "corosig/ErrorTypes.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/util/SetDefaultOnMove.hpp"

#include <cassert>
#include <concepts>
#include <coroutine>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace corosig {

/// @brief Error type raised when a Promise is destroyed without being fulfilled
struct BrokenPromise {
  auto operator<=>(const BrokenPromise &) const noexcept = default;

  [[nodiscard]] static std::string_view description() noexcept {
    return "Broken promise";
  }
};

/// @brief  Promise<T> provides a one-shot channel for passing a value of type T from a producer
///         coroutine to a consumer coroutine. Once a value is set via set_value(), it can be
///         retrieved using co_await on the Awaiter returned by get_awaiter(). Each Promise can
///         only be awaited and set once; attempting to do otherwise is UB
template <typename T>
  requires(std::is_nothrow_move_constructible_v<T>)
struct Promise {
private:
  struct State : CoroListNode {
    State(Reactor &r) noexcept
        : reactor{r} {
    }

    State(const State &) = delete;
    State(State &&) = delete;
    State &operator=(const State &) = delete;
    State &operator=(State &&) = delete;

    ~State() override = default;

    std::coroutine_handle<> coro_from_this() noexcept override {
      return waiting_coro;
    }

    void downref() noexcept {
      --refcount;
      if (refcount == 0) {
        Reactor &local_reactor = reactor;
        this->~State();
        local_reactor.allocator().deallocate(this);
      }
    }

    Reactor &reactor;
    std::coroutine_handle<> waiting_coro = std::noop_coroutine();
    [[no_unique_address]] std::optional<T> value = std::nullopt;
    [[no_unique_address]] uint8_t refcount = 1;
  };

public:
  struct [[nodiscard("forgot to await?")]] Awaiter {
    Awaiter() noexcept = default;

    Awaiter(const Awaiter &) = delete;
    Awaiter(Awaiter &&rhs) noexcept = default;
    Awaiter &operator=(const Awaiter &) = delete;
    Awaiter &operator=(Awaiter &&rhs) noexcept {
      if (this != &rhs) {
        this->~Awaiter();
        new (this) Awaiter{std::move(rhs)};
      }
      return *this;
    }

    ~Awaiter() {
      if (*m_state) {
        m_state.value->downref();
      }
    }

    [[nodiscard]] bool await_ready() const noexcept {
      assert(*m_state != nullptr);
      return m_state.value->value.has_value();
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
      assert(*m_state != nullptr);
      m_state.value->m_waiting_coro = h;
    }

    T await_resume() noexcept {
      return std::forward<T>(*m_state.value->value);
    }

  private:
    friend Promise;

    Awaiter(State &state) noexcept
        : m_state{&state} {
      ++m_state.value->refcount;
    }

    SetDefaultOnMove<State *, nullptr> m_state;
  };

  Promise() noexcept = default;

  /// @brief Try to create a Promise object or get an allocation error
  /// @param r Reference to the Reactor used for scheduling coroutines and memory allocation
  static Result<Promise, AllocationError> make(Reactor &r) noexcept {
    void *state_buf = r.allocator().allocate(sizeof(State), alignof(State));
    if (state_buf == nullptr) {
      return Failure{AllocationError{}};
    }
    return Promise{*new (state_buf) State{r}};
  }

  Promise(const Promise &) = delete;
  Promise(Promise &&) noexcept = default;
  Promise &operator=(const Promise &) = delete;
  Promise &operator=(Promise &&rhs) noexcept {
    if (this != &rhs) {
      this->~Promise();
      new (this) Promise{std::move(rhs)};
    }
    return *this;
  }

  ~Promise() {
    if (*m_state) {
      if constexpr (std::convertible_to<Failure<BrokenPromise &&>, T>) {
        if (has_awaiter()) {
          set(Failure{BrokenPromise{}});
        }
      } else {
        assert(!has_awaiter());
      }

      m_state.value->downref();
    }
  }

  bool is_set() const noexcept {
    assert(*m_state != nullptr);
    return m_state.value->value.has_value();
  }

  bool has_awaiter() const noexcept {
    assert(*m_state != nullptr);
    return m_state.value->refcount == 2;
  }

  /// @brief Creates an Awaiter for this Promise
  ///
  /// @warn Each Promise can only generate one Awaiter. Attempting to call this method multiple
  ///       times on the same Promise is UB
  Awaiter get_awaiter() noexcept {
    assert(!has_awaiter() && "It is forbidden to create multiple awaiters for the same Promise");
    assert(*m_state != nullptr);
    return Awaiter{**m_state};
  }

  /// @brief    Fulfills the promise by storing a value and scheduling the waiting coroutine for
  ///           resumption
  ///
  /// @warn     This method should be called once per Promise. Doing otherwise is UB
  template <std::convertible_to<T> U>
  void set(U &&value) noexcept {
    assert(*m_state != nullptr);
    assert(!is_set());
    m_state.value->value.emplace(std::forward<U>(value));
    m_state.value->reactor.schedule(**m_state);
  }

private:
  Promise(State &state) noexcept
      : m_state{&state} {
  }

  SetDefaultOnMove<State *, nullptr> m_state;
};

} // namespace corosig

#endif
