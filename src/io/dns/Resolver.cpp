#include "corosig/io/dns/Resolver.hpp"

#include "corosig/Clock.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/Sleep.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/Protocol.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <string_view>

namespace {

using namespace corosig;
using namespace corosig::dns;

Result<void, ResolveError> void_consume_ns_ar_rrs(Header header,
                                                  ResponseDecoder &decoder) noexcept {
  for (size_t i = 0; i != header.nscount; ++i) {
    COROSIG_TRYV(decoder.consume_resource_record());
  }

  for (size_t i = 0; i != header.arcount; ++i) {
    COROSIG_TRYV(decoder.consume_resource_record());
  }

  return Ok{};
}

Result<void, Error<ResolveError>>
validate_response(Header header, ResponseDecoder &decoder, Question question) noexcept {

  ServerResponseCode rcode = header.flags.get_rcode();
  if (rcode != ServerResponseCode::NOERROR) {
    return Failure{rcode};
  }

  if (!header.flags.is_response()) {
    return Failure{ResolveErrorCode::GOT_REQUEST_INSTEAD};
  }

  if (header.flags.truncated()) {
    return Failure{ResolveErrorCode::QUESTION_TRUNCATED};
  }

  if (header.qdcount != Header::THE_ONLY_VALID_QDCOUNT) {
    return Failure{ResolveErrorCode::INVALID_QDCOUNT};
  }

  COROSIG_TRY(auto question_entry, decoder.consume_question_entry());
  if (!std::ranges::equal(question_entry.name.labels_svs(), split_into_labels(question.name)) ||
      question_entry.qclass != question.qclass || question_entry.qtype != question.qtype) {
    return Failure{ResolveErrorCode::QUESTION_VALUE_MISMATCH};
  }

  return Ok{};
}

} // namespace

namespace corosig::dns {

std::string_view ResolveErrorCode::description() const noexcept {
  switch (value) {
  case GOT_REQUEST_INSTEAD:
    return "Got request when expected response";
  case QUESTION_TRUNCATED:
    return "Question was truncated by server";
  case INVALID_QDCOUNT:
    return "The only valid number for QDCOUNT in successfull request is 1";
  case QUESTION_VALUE_MISMATCH:
    return "Questions in response are not the same as in question";
  case RESOLVE_ABORTED:
    return "Resolve was aborted";
  case TOO_MANY_PARALLEL_REQUESTS:
    return "There are more parallel requests than 16 bit request id field can store";
  case NO_ANSWERS_GIVEN:
    return "Received query response with no answers";
  case NO_SERVERS_PROVIDED:
    return "Provided dns servers list is empty";
  }
  assert(false && "Should be unreachable");
  return {};
}

CachelessResolver::CachelessResolver(ChaCha20RandomGenerator rand_gen, UdpSocket socket) noexcept
    : m_rand_gen{rand_gen},
      m_udp_socket{std::move(socket)} {
}

Result<CachelessResolver, Error<AllocationError, SyscallError>>
CachelessResolver::make(std::array<uint8_t, 32> rand_seed, SockaddrStorage const &local) noexcept {
  ChaCha20RandomGenerator rand_gen{rand_seed};
  COROSIG_TRY(UdpSocket socket, UdpSocket::bound(local));
  return CachelessResolver{rand_gen, std::move(socket)};
}

Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
CachelessResolver::resolve_name(Reactor &r,
                                std::span<SockaddrStorage const> dns_server_addrs,
                                std::string_view ascii_name,
                                std::span<ResolvedAddress<Ipv4Addr>> out) noexcept {
  return formulate_and_process_request(r, dns_server_addrs, ascii_name, out);
}

Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
CachelessResolver::resolve_name(Reactor &r,
                                std::span<SockaddrStorage const> dns_server_addrs,
                                std::string_view ascii_name,
                                std::span<ResolvedAddress<Ipv6Addr>> out) noexcept {
  return formulate_and_process_request(r, dns_server_addrs, ascii_name, out);
}

template <typename IP>
Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
CachelessResolver::formulate_and_process_request(Reactor &r,
                                                 std::span<SockaddrStorage const> dns_server_addrs,
                                                 std::string_view ascii_name,
                                                 std::span<ResolvedAddress<IP>> out) noexcept {
  if (m_pending_requests.size() == std::numeric_limits<uint16_t>::max()) {
    co_return Failure{ResolveErrorCode::TOO_MANY_PARALLEL_REQUESTS};
  }

  if (dns_server_addrs.empty()) {
    co_return Failure{ResolveErrorCode::NO_SERVERS_PROVIDED};
  }

  PendingRequest<IP> this_request;
  this_request.question = Question{
      .id = 0, // is set to a proper value a bit later
      .opcode = QueryOpcode::STANDARD,
      .recursion_desired = true,
      .name = ascii_name,
      .qtype = std::same_as<IP, Ipv6Addr> ? QueryType::AAAA : QueryType::A,
      .qclass = QueryClass::IN,
  };

  this_request.out = out;
  co_return co_await process_request(r, this_request, dns_server_addrs);
}

Fut<size_t, Error<AllocationError, SyscallError, ResolveError>>
CachelessResolver::process_request(Reactor &r,
                                   PendingRequestBase &request,
                                   std::span<SockaddrStorage const> dns_server_addrs) noexcept {
  m_rand_gen.generate_bytes(std::as_writable_bytes(std::span{&request.question.id, 1}));

  // make sure there are no collisions for request ids
  auto it = m_pending_requests.find(request);
  while (it != m_pending_requests.end() && request.question.id == it->question.id) {
    if (request.question.id == std::numeric_limits<uint16_t>::max()) {
      it = m_pending_requests.begin();
      continue;
    }
    ++it;
  }

  m_pending_requests.insert(request);

  // enough for single-entry question even in the worst-case scenario with 255-len domain name
  std::array<uint8_t, 512> encode_buf;

  COROSIG_CO_TRY(auto *end, encode_question(encode_buf.begin(), request.question, r.allocator()));

  std::span encoded_message{reinterpret_cast<char const *>(encode_buf.begin()),
                            reinterpret_cast<char const *>(end)};

  Fut periodic_background_send_fut =
      periodic_background_send(r, encoded_message, request, m_udp_socket, dns_server_addrs);

  // wait until send fails with syscall or receiver gets proper matching answer or receiver gets
  // improper but matching-by-id answer
  co_await request;
  co_return request.result;
}

Fut<void, Error<AllocationError, SyscallError>> CachelessResolver::periodic_background_send(
    Reactor &r,
    std::span<char const> encoded_message,
    PendingRequestBase &this_request,
    UdpSocket &socket,
    std::span<SockaddrStorage const> dns_server_addrs) noexcept {
  while (true) {
    for (SockaddrStorage const &dns_server_addr : dns_server_addrs) {
      this_request.send_time = SteadyClock::now();
      auto result = co_await socket.send_to(r, encoded_message, dns_server_addr);

      if (m_receiver.completed()) {
        m_receiver = receive_server_answers(r);
      }

      if (!result) {
        this_request.result = Failure{result.error()};
        this_request.hook.unlink();
        r.schedule(this_request);
        co_return Ok{};
      }

      using namespace std::chrono_literals;
      co_await Sleep{2s}; // Minimal timeout proposed by RFC1035
    }
  }
}

CachelessResolver::receiver_type CachelessResolver::receive_server_answers(Reactor &r) noexcept {
  while (!m_pending_requests.empty()) {
    std::array<char, 512> response_buf;

    COROSIG_CO_TRY(size_t response_size, co_await m_udp_socket.recv_from(r, response_buf, nullptr));

    response_size = std::min(response_size, response_buf.size());

    ResponseDecoder decoder{std::span{
        reinterpret_cast<uint8_t const *>(response_buf.data()),
        response_size / sizeof(uint8_t),
    }};

    Result header_result = decoder.consume_header();
    if (!header_result) {
      continue;
    }
    Header header = header_result.value();

    auto current_request = m_pending_requests.find(header.id, std::less<>{});

    if (current_request == m_pending_requests.end()) {
      continue;
    }

    current_request->process_server_answer(header, decoder);
    current_request->hook.unlink();
    r.schedule(*current_request);
  }

  co_return Ok{};
}

template <typename IP>
Result<size_t, Error<AllocationError, SyscallError, ResolveError>>
CachelessResolver::PendingRequest<IP>::process_server_answer_impl(
    Header header, ResponseDecoder &decoder) noexcept {
  COROSIG_TRYV(validate_response(header, decoder, question));

  auto out_begin = out.begin();
  for (size_t i = 0; i != header.ancount; ++i) {
    COROSIG_TRY(ResourceRecord rr, decoder.consume_resource_record());

    if (out.empty()) {
      continue;
    }

    rr.rdata.visit([&]<typename RDATA>(RDATA const &rdata) noexcept {
      if constexpr ((std::constructible_from<IP, Ipv4Addr> && std::same_as<RDATA, RDataIpv4>) ||
                    (std::constructible_from<IP, Ipv6Addr> && std::same_as<RDATA, RDataIpv6>)) {
        out.front() = ResolvedAddress<IP>{
            .address = IP{rdata.addr},
            .expires_at = send_time + rr.ttl,
        };
        out = out.subspan(1);
      }
    });
  }

  COROSIG_TRYV(void_consume_ns_ar_rrs(header, decoder));

  return size_t(out.begin() - out_begin);
}

template <typename IP>
void CachelessResolver::PendingRequest<IP>::process_server_answer(
    Header header, ResponseDecoder &decoder) noexcept {
  result = process_server_answer_impl(header, decoder);
}

bool CachelessResolver::PendingRequestBase::await_ready() noexcept {
  return !result.is_nothing();
}

void CachelessResolver::PendingRequestBase::await_suspend(std::coroutine_handle<> h) noexcept {
  waiter = h;
}

void CachelessResolver::PendingRequestBase::await_resume() noexcept {
}

template struct Resolver<Cache<>>;

} // namespace corosig::dns
