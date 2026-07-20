#include "corosig/meta/AnAwaitable.hpp"

#include "corosig/testing/Signals.hpp"

#include <concepts>
#include <string>

namespace {

struct SimpleAwaiter {
  static bool await_ready() noexcept {
    return true;
  }

  void await_suspend(std::coroutine_handle<>) const noexcept {
  }

  static int await_resume() noexcept {
    return 42;
  }
};

struct VoidAwaiter {
  static bool await_ready() noexcept {
    return true;
  }

  void await_suspend(std::coroutine_handle<>) const noexcept {
  }

  void await_resume() const noexcept {
  }
};

struct StringAwaiter {
  static bool await_ready() noexcept {
    return true;
  }

  void await_suspend(std::coroutine_handle<>) const noexcept {
  }

  static std::string await_resume() noexcept {
    return "hello";
  }
};

struct RefAwaiter {
  static bool await_ready() noexcept {
    return true;
  }

  void await_suspend(std::coroutine_handle<>) const noexcept {
  }

  static int &await_resume() noexcept {
    static int value = 99;
    return value;
  }
};

struct MemberAwaitable {
  bool ready = true;

  SimpleAwaiter operator co_await() && noexcept {
    return {};
  }
};

struct MemberAsyncAwaitable {
  bool ready = false;

  StringAwaiter operator co_await() && noexcept {
    return {};
  }
};

struct NonMemberAwaitable {
  bool ready = true;
};

struct NestedMemberAwaitable {
  MemberAsyncAwaitable operator co_await() && noexcept {
    return {};
  }
};

struct NestedNonMemberAwaitable {
  bool ready = true;
};

StringAwaiter operator co_await(NonMemberAwaitable &&) noexcept {
  return {};
}

struct NonMemberCustomAwaitable {
  bool ready = false;
};

RefAwaiter operator co_await(NonMemberCustomAwaitable &&) noexcept {
  return {};
}

MemberAwaitable operator co_await(NestedNonMemberAwaitable &&) noexcept {
  return {};
}

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("AnAwaitable metafunctions are sane") {
  using namespace corosig;

  static_assert(AnAwaiter<SimpleAwaiter>);
  static_assert(AnAwaiter<VoidAwaiter>);
  static_assert(AnAwaiter<StringAwaiter>);
  static_assert(AnAwaiter<RefAwaiter>);
  static_assert(!AnAwaiter<int>);
  static_assert(!AnAwaiter<std::string>);

  static_assert(HasMemberCoAwait<MemberAwaitable>);
  static_assert(HasMemberCoAwait<MemberAsyncAwaitable>);
  static_assert(!HasMemberCoAwait<SimpleAwaiter>);
  static_assert(!HasMemberCoAwait<NonMemberAwaitable>);

  static_assert(HasNonMemberCoAwait<NonMemberAwaitable>);
  static_assert(HasNonMemberCoAwait<NonMemberCustomAwaitable>);
  static_assert(!HasNonMemberCoAwait<MemberAwaitable>);
  static_assert(!HasNonMemberCoAwait<SimpleAwaiter>);

  static_assert(AnAwaitable<SimpleAwaiter>);
  static_assert(AnAwaitable<VoidAwaiter>);
  static_assert(AnAwaitable<MemberAwaitable>);
  static_assert(AnAwaitable<NonMemberAwaitable>);
  static_assert(AnAwaitable<MemberAsyncAwaitable>);
  static_assert(AnAwaitable<NonMemberCustomAwaitable>);
  static_assert(!AnAwaitable<int>);
  static_assert(!AnAwaitable<std::string>);

  static_assert(std::same_as<AwaitResult<SimpleAwaiter>, int>);
  static_assert(std::same_as<AwaitResult<VoidAwaiter>, void>);
  static_assert(std::same_as<AwaitResult<StringAwaiter>, std::string>);
  static_assert(std::same_as<AwaitResult<RefAwaiter>, int &>);

  static_assert(std::same_as<AwaitResult<MemberAwaitable>, int>);
  static_assert(std::same_as<AwaitResult<MemberAsyncAwaitable>, std::string>);

  static_assert(std::same_as<AwaitResult<NonMemberAwaitable>, std::string>);
  static_assert(std::same_as<AwaitResult<NonMemberCustomAwaitable>, int &>);

  static_assert(HasMemberCoAwait<NestedMemberAwaitable>);
  static_assert(AnAwaitable<NestedMemberAwaitable>);
  static_assert(
      std::same_as<AwaitResult<NestedMemberAwaitable>, AwaitResult<MemberAsyncAwaitable>>);

  static_assert(HasNonMemberCoAwait<NestedNonMemberAwaitable>);
  static_assert(AnAwaitable<NestedNonMemberAwaitable>);
  static_assert(std::same_as<AwaitResult<NestedNonMemberAwaitable>, AwaitResult<MemberAwaitable>>);
}
