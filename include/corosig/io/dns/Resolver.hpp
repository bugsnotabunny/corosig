#ifndef COROSIG_IO_DNS_RESOLVER_HPP
#define COROSIG_IO_DNS_RESOLVER_HPP

#include "corosig/Clock.hpp"
#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Random.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/UdpSocket.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/io/dns/Cache.hpp"
#include "corosig/io/dns/Protocol.hpp"
#include "corosig/meta/Futurize.hpp"
#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <boost/intrusive/avl_set.hpp>
#include <boost/intrusive/avl_set_hook.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/options.hpp>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <span>
#include <string_view>
#include <sys/socket.h>
#include <variant>

namespace corosig::dns {

constexpr uint16_t STANDARD_PORT = 53;

struct ResolveErrorCode {
  enum Value : uint8_t {
    RESOLVE_ABORTED,
    TOO_MANY_PARALLEL_REQUESTS,
    NO_SERVERS_PROVIDED,
    GOT_REQUEST_INSTEAD,
    QUESTION_TRUNCATED,
    INVALID_QDCOUNT,
    QUESTION_VALUE_MISMATCH,
    NO_ANSWERS_GIVEN,
  } value;

  constexpr ResolveErrorCode(Value v) noexcept
      : value{v} {
  }

  constexpr auto operator<=>(const ResolveErrorCode &) const noexcept = default;

  [[nodiscard]] std::string_view description() const noexcept;
};

struct ResolveError
    : Error<ResolveErrorCode, ServerResponseCode, ResponseDecodeError, QuestionEncodeError> {
  using Error::Error;
};

struct CachelessResolver {
  CachelessResolver(ChaCha20RandomGenerator rand_gen, UdpSocket socket) noexcept;

  static Result<CachelessResolver, Error<AllocationError, SyscallError>>
  make(std::array<uint8_t, 32> rand_seed, SockaddrStorage const &local) noexcept;

  static Result<CachelessResolver, Error<AllocationError, SyscallError>>
  make(auto const &rand_seed, SockaddrStorage const &local) noexcept {
    return make(std::bit_cast<std::array<uint8_t, 32>>(rand_seed), local);
  }

  Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
  resolve_name(Reactor &r,
               std::span<SockaddrStorage const> dns_server_addrs,
               std::string_view ascii_name,
               std::span<ResolvedAddress<Ipv4Addr>> out) noexcept;

  Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
  resolve_name(Reactor &r,
               std::span<SockaddrStorage const> dns_server_addrs,
               std::string_view ascii_name,
               std::span<ResolvedAddress<Ipv6Addr>> out) noexcept;

  template <typename IP>
  Fut<ResolvedAddress<IP>, Error<AllocationError, SyscallError, ResolveError>>
  resolve_name1(Reactor &r,
                std::span<SockaddrStorage const> dns_server_addrs,
                std::string_view ascii_name) noexcept {
    ResolvedAddress<IP> addr;

    COROSIG_CO_TRY(size_t resolved,
                   co_await resolve_name(r, dns_server_addrs, ascii_name, std::span{&addr, 1}));
    if (resolved != 1) {
      co_return Failure{ResolveErrorCode::NO_ANSWERS_GIVEN};
    }

    co_return addr;
  }

private:
  template <typename IP>
  struct RDataMatcher;

  struct PendingRequestBase : CoroListNode {
    virtual void process_server_answer(Header, ResponseDecoder &) noexcept = 0;

    std::coroutine_handle<> coro_from_this() noexcept override {
      return waiter;
    }

    bool await_ready() noexcept;
    void await_suspend(std::coroutine_handle<> h) noexcept;
    static void await_resume() noexcept;

    constexpr auto operator<=>(uint16_t id) const noexcept {
      return this->question.id <=> id;
    }

    constexpr auto operator<=>(const PendingRequestBase &rhs) const noexcept {
      return *this <=> rhs.question.id;
    }

    using hook_type = boost::intrusive::avl_set_member_hook<
        boost::intrusive::optimize_size<true>,
        boost::intrusive::link_mode<boost::intrusive::auto_unlink>>;

    hook_type hook = hook_type{};
    Result<size_t, Error<AllocationError, SyscallError, ResolveError>> result = {};
    std::coroutine_handle<> waiter = nullptr;
    SteadyClock::time_point send_time = {};
    Question question = {};
  };

  template <typename IP>
  struct PendingRequest : PendingRequestBase {
    void process_server_answer(Header, ResponseDecoder &) noexcept override;
    Result<size_t, Error<AllocationError, SyscallError, ResolveError>>
    process_server_answer_impl(Header, ResponseDecoder &) noexcept;

    std::span<ResolvedAddress<IP>> out;
  };

  template <typename IP>
  Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
  formulate_and_process_request(Reactor &r,
                                std::span<SockaddrStorage const> dns_server_addrs,
                                std::string_view ascii_name,
                                std::span<ResolvedAddress<IP>> out) noexcept;

  Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
  process_request(Reactor &r,
                  PendingRequestBase &request,
                  std::span<SockaddrStorage const> dns_server_addrs) noexcept;

  Fut<void, Error<AllocationError, SyscallError>>
  periodic_background_send(Reactor &r,
                           std::span<char const> encoded_message,
                           PendingRequestBase &this_request,
                           UdpSocket &socket,
                           std::span<SockaddrStorage const> dns_server_addrs) noexcept;

  using receiver_type = Fut<void, Error<AllocationError, SyscallError>>;

  receiver_type receive_server_answers(Reactor &r) noexcept;

  using pending_requests_map_type =
      boost::intrusive::avl_multiset<PendingRequestBase,
                                     boost::intrusive::constant_time_size<false>,
                                     boost::intrusive::member_hook<PendingRequestBase,
                                                                   PendingRequestBase::hook_type,
                                                                   &PendingRequestBase::hook>>;
  using response_buffer_type = std::array<uint8_t, 512>;

  receiver_type m_receiver = receiver_type::make_failed_to_allocate();
  ChaCha20RandomGenerator m_rand_gen;
  UdpSocket m_udp_socket;
  pending_requests_map_type m_pending_requests;
};

template <ACache CACHE = Cache<>>
struct Resolver {
  Resolver(CACHE cache, CachelessResolver resolver) noexcept
      : m_cache{std::move(cache)},
        m_resolver{std::move(resolver)} {
  }

  Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
  resolve_name(Reactor &r,
               std::span<SockaddrStorage const> dns_server_addrs,
               std::string_view ascii_name,
               std::span<ResolvedAddress<Ipv4Addr>> out) noexcept {
    return resolve_name_impl(r, dns_server_addrs, ascii_name, out);
  }

  Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
  resolve_name(Reactor &r,
               std::span<SockaddrStorage const> dns_server_addrs,
               std::string_view ascii_name,
               std::span<ResolvedAddress<Ipv6Addr>> out) noexcept {
    return resolve_name_impl(r, dns_server_addrs, ascii_name, out);
  }

  template <typename IP>
  Fut<ResolvedAddress<IP>, Error<AllocationError, SyscallError, ResolveError>>
  resolve_name1(Reactor &r,
                std::span<SockaddrStorage const> dns_server_addrs,
                std::string_view ascii_name) noexcept {
    ResolvedAddress<IP> addr;
    COROSIG_CO_TRYV(co_await resolve_name(r, dns_server_addrs, ascii_name, std::span{&addr, 1}));
    co_return addr;
  }

private:
  template <typename IP>
  Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
  resolve_name_impl(Reactor &r,
                    std::span<SockaddrStorage const> dns_server_addrs,
                    std::string_view ascii_name,
                    std::span<ResolvedAddress<IP>> out) noexcept {
    assert(detail::debug_is_ascii(ascii_name));
    if constexpr (AMutableCache<CACHE>) {
      (void)m_cache.prune();
    }

    auto pull_res = co_await futurize(m_cache.pull(ascii_name, out));
    if (pull_res && pull_res.value() != 0) {
      co_return pull_res.value();
    }

    COROSIG_CO_TRY(size_t resolved_names,
                   co_await m_resolver.resolve_name(r, dns_server_addrs, ascii_name, out));

    if constexpr (AMutableCache<CACHE>) {
      (void)m_cache.push(ascii_name, out);
    }
    co_return resolved_names;
  }

  CACHE m_cache;
  CachelessResolver m_resolver;
};

extern template struct Resolver<Cache<>>;

} // namespace corosig::dns

#endif
