#ifndef COROSIG_IO_DNS_PROTOCOL_HPP
#define COROSIG_IO_DNS_PROTOCOL_HPP

#include "corosig/Result.hpp"
#include "corosig/container/Vector.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/meta/AnAllocator.hpp"
#include "corosig/util/Variant.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace corosig::dns {

struct QuestionEncodeError {
  enum Value : uint8_t {
    TOO_LONG_DOMAIN_NAME,
    TOO_LONG_DOMAIN_NAME_LABEL,
    EMPTY_LABEL,
  } value;

  constexpr QuestionEncodeError(Value v) noexcept
      : value{v} {
  }

  constexpr auto operator<=>(const QuestionEncodeError &) const noexcept = default;

  [[nodiscard]] std::string_view description() const noexcept;
};

struct ResponseDecodeError {
  enum Value : uint8_t {
    WRONG_HEADER_BYTES_COUNT,
    COMPRESSION_PTR_OOB,
    COMPRESSION_PTR_INFINITE_LOOP,
    LABEL_NO_LEN,
    LABEL_LEN_OOB,
    LABEL_LEN_TOO_BIG,
    NAME_LEN_TOO_BIG,
    NO_ROOT_LABEL,
    NO_QUESTION_TYPE,
    NO_QUESTION_CLASS,
    NO_RESOURCE_RECORD_TYPE,
    NO_RESOURCE_RECORD_CLASS,
    NO_RESOURCE_RECORD_TTL,
    NO_RESOURCE_RECORD_DATA_LEN,
    RESOURCE_RECORD_DATA_LEN_OOB,
    RESOURCE_RECORD_DATA_BAD_A,
    RESOURCE_RECORD_DATA_BAD_AAAA,
    RESOURCE_RECORD_DATA_BAD_NS,
    RESOURCE_RECORD_DATA_BAD_CNAME,
    RESOURCE_RECORD_DATA_BAD_SOA,
    RESOURCE_RECORD_DATA_BAD_MB,
    RESOURCE_RECORD_DATA_BAD_MG,
    RESOURCE_RECORD_DATA_BAD_MR,
    RESOURCE_RECORD_DATA_BAD_PTR,
    RESOURCE_RECORD_DATA_BAD_HINFO,
    RESOURCE_RECORD_DATA_BAD_MINFO,
  } value;

  constexpr ResponseDecodeError(Value v) noexcept
      : value{v} {
  }

  constexpr auto operator<=>(const ResponseDecodeError &) const noexcept = default;

  [[nodiscard]] std::string_view description() const noexcept;
};

struct ServerResponseCode {
  enum Value : uint8_t {
    NOERROR = 0,
    FORMAT_ERROR = 1,
    SERVER_FAILURE = 2,
    NAME_ERROR = 3,
    NOT_IMPLEMENTED = 4,
    REFUSED = 5,
  } value;

  constexpr ServerResponseCode(Value v) noexcept
      : value{v} {
  }

  constexpr auto operator<=>(const ServerResponseCode &) const noexcept = default;

  [[nodiscard]] std::string_view description() const noexcept;
};

enum class QueryType : uint8_t {
  /// @brief Requests an IPv4 address
  A = 1,

  /// @brief Requests an IPv6 address
  AAAA = 28,

  /// @brief Requests authoritative name servers
  NS = 2,

  /// @brief Obsolete. Use MX
  MD = 3,

  /// @brief Obsolete. Use MX
  MF = 4,

  /// @brief Requests the canonical name for an alias
  CNAME = 5,

  /// @brief Requests the start of a zone of authority
  SOA = 6,

  /// @brief Requests mailbox domain name. Experimental in protocol
  MB = 7,

  /// @brief Requests mail group domain name. Experimental in protocol
  MG = 8,

  /// @brief Requests main rename domain name. Experimental in protocol
  MR = 9,

  /// @brief Requests null resource record. Experimental in protocol
  TNULL = 10,

  /// @brief Requests well known service description
  WKS = 11,

  /// @brief Requests a domain name pointer (used for reverse lookups)
  PTR = 12,

  /// @brief Requests host information
  HINFO = 13,

  /// @brief Requests mailbox or mail list information
  MINFO = 14,

  /// @brief Requests mail exchange records
  MX = 15,

  /// @brief Requests text strings
  TXT = 16,

  /// @brief Requests a transfer of the entire zone (all records in a domain). This is used for
  ///        replicating DNS data between primary and secondary servers.
  AXFR = 252,

  /// @brief Requests all mailbox-related records (MB, MG, MR)
  MAILB = 253,

  /// @brief Requests mail agent records (now obsolete, replaced by MX)
  MAILA = 254,

  /// @brief Requests all records, regardless of type
  ANY = 255,
};

enum class QueryClass : uint8_t {
  /// @brief Internet
  IN = 1,

  /// @brief Obsolete by protocol. Don't use it
  CSNET = 2,

  /// @brief Chaos. Sometimes used internally by some DNS server software for querying server
  ///        statistics and version information
  CH = 3,

  /// @brief HESIOD class. Used by the Hesiod project at MIT for distributing information (like user
  ///        accounts)
  HS = 4,

  /// @brief Requests all records, regardless of class
  ANY = 255,
};

enum class QueryOpcode : uint8_t {
  STANDARD = 0,
  /// OBSOLETE
  INVERSE = 1,
  STATUS = 2,
};

struct Header {
  struct Flags {
    [[nodiscard]] bool is_response() const noexcept;
    Flags &set_response(bool) noexcept;

    Flags &set_opcode(QueryOpcode) noexcept;
    [[nodiscard]] QueryOpcode opcode() const noexcept;

    Flags &set_authoritative_answer(bool) noexcept;
    [[nodiscard]] bool authoritative_answer() const noexcept;

    Flags &set_truncated(bool) noexcept;
    [[nodiscard]] bool truncated() const noexcept;

    Flags &set_recursion_desired(bool) noexcept;
    [[nodiscard]] bool recursion_desired() const noexcept;

    Flags &set_recursion_available(bool) noexcept;
    [[nodiscard]] bool recursion_available() const noexcept;

    Flags &set_rcode(ServerResponseCode) noexcept;
    [[nodiscard]] ServerResponseCode get_rcode() const noexcept;

    constexpr auto operator<=>(Flags const &rhs) const noexcept = default;

    uint16_t value = 0;
  };

  constexpr auto operator<=>(Header const &rhs) const noexcept = default;

  constexpr static uint16_t THE_ONLY_VALID_QDCOUNT = 1; // RFC 9619 restricts this field's value

  uint16_t id = 0;
  Flags flags = Flags{};
  uint16_t qdcount = 0;
  uint16_t ancount = 0;
  uint16_t nscount = 0;
  uint16_t arcount = 0;
};

struct Question {
  uint16_t id;
  QueryOpcode opcode = QueryOpcode::STANDARD;
  bool recursion_desired = true;
  std::string_view name;
  QueryType qtype;
  QueryClass qclass = QueryClass::IN;
};

struct DomainName : std::ranges::view_interface<DomainName> {
  struct LabelsIterator {
    using difference_type = std::ptrdiff_t;
    using value_type = std::span<uint8_t const>;
    using reference = value_type &;
    using pointer = value_type *;
    using iterator_category = std::input_iterator_tag;

    explicit LabelsIterator(DomainName domain_name) noexcept;

    value_type operator*() const noexcept;

    LabelsIterator &operator++() noexcept;
    LabelsIterator operator++(int) noexcept;

    bool operator==(std::default_sentinel_t) const noexcept;

  private:
    void adwance() noexcept;

    uint8_t const *m_original_message_start;
    uint8_t const *m_label_start;
  };

  DomainName(uint8_t const *original_message_start, uint8_t const *self_start) noexcept;

  [[nodiscard]] auto begin() const noexcept {
    return LabelsIterator{*this};
  }

  static auto end() noexcept {
    return std::default_sentinel;
  }

  [[nodiscard]] std::ranges::range auto labels_svs() const noexcept {
    return *this | std::views::transform([](std::span<uint8_t const> &&octets) {
      return std::string_view{reinterpret_cast<char const *>(octets.data()),
                              octets.size() * sizeof(uint8_t)};
    });
  }

private:
  uint8_t const *m_original_message_start;
  uint8_t const *m_self_start;
};

inline std::ranges::range auto split_into_labels(std::string_view sv) noexcept {
  if (sv.ends_with('.')) {
    sv.remove_suffix(1);
  }

  return sv | std::views::split('.') |
         std::views::transform([](auto &&r) { return std::string_view{r.begin(), r.size()}; });
}

struct CharacterString {
  std::span<uint8_t const> data;
};

using seconds = std::chrono::duration<uint32_t>;

struct RDataCanonicalName {
  DomainName cname;
};

struct RDataHardwareInfo {
  CharacterString cpu;
  CharacterString os;
};

struct RDataMailbox {
  DomainName madname;
};

struct RDataMailGroup {
  DomainName madname;
};

struct RDataMailInfo {
  DomainName rmailbx;
  DomainName emailbx;
};

struct RDataMailboxRename {
  DomainName newname;
};

struct RDataMailExchange {
  DomainName exchange;
  uint16_t preference;
};

struct RDataNull {
  std::span<uint8_t const> anything;
};

struct RDataNameServer {
  DomainName nsdname;
};

struct RDataPtr {
  DomainName ptrdname;
};

struct RDataStartOfAuthority {
  DomainName mname;
  DomainName rname;
  uint32_t serial;
  seconds refresh_after;
  seconds retry_after;
  seconds expire_after;
  seconds minimum_ttl;
};

struct RDataIpv4 {
  Ipv4Addr addr;
};

struct RDataIpv6 {
  Ipv6Addr addr;
};

struct RDataObsolete {
  uint16_t type;
  std::span<uint8_t const> raw;
};

struct RDataNotYetSupported {
  uint16_t type;
  std::span<uint8_t const> raw;
};

struct RData : Variant<RDataCanonicalName,
                       RDataHardwareInfo,
                       RDataMailbox,
                       RDataMailGroup,
                       RDataMailInfo,
                       RDataMailboxRename,
                       RDataMailExchange,
                       RDataNull,
                       RDataNameServer,
                       RDataPtr,
                       RDataStartOfAuthority,
                       RDataIpv4,
                       RDataIpv6,
                       RDataObsolete,
                       RDataNotYetSupported> {
  using Variant::Variant;
};

struct ResourceRecord {
  DomainName name;
  [[no_unique_address]] RData rdata;
  seconds ttl;
};

namespace detail {

constexpr auto FQDN_MAX_OCTET_LEN = 255;

template <std::output_iterator<char> OUT>
constexpr OUT write_hton(OUT out, uint16_t value) {
  *out++ = char((value >> 8) & 0xFF);
  *out++ = char(value & 0xFF);
  return out;
};

template <std::output_iterator<char> OUT>
constexpr OUT encode_header(OUT out, Header header) {
  out = write_hton(out, header.id);
  out = write_hton(out, header.flags.value);
  out = write_hton(out, header.qdcount);
  out = write_hton(out, header.ancount);
  out = write_hton(out, header.nscount);
  out = write_hton(out, header.arcount);
  return out;
}

// RFC 1035 section 4.1.4
struct CompressionPointer {
  constexpr static uint16_t MARKER_BITS = 0xC000;
  constexpr static uint16_t MAX_VALUE = 0x3FFF;

  constexpr auto operator<=>(const CompressionPointer &) const noexcept = default;

  uint16_t offset = 0;
};

template <std::output_iterator<char> OUT>
constexpr OUT encode_compression_ptr(OUT out, CompressionPointer ptr) {
  assert(ptr.offset <= CompressionPointer::MAX_VALUE &&
         "Compression pointer overflow. Only 14 bits are allowed");
  uint16_t compression_ptr = CompressionPointer::MARKER_BITS | ptr.offset;
  return write_hton(out, compression_ptr);
}

template <AnAllocator ALLOCATOR>
struct CompressionMap {
  struct Node {
    explicit Node(std::string_view s, uint16_t offset) noexcept
        : hash{uint16_t(std::hash<std::string_view>{}(s))},
          offset{offset},
          strlen{uint32_t(s.size())},
          str{s.data()} {
    }

    constexpr bool operator==(Node const &rhs) const noexcept {
      return std::tuple(hash, std::string_view{str, strlen}) ==
             std::tuple(rhs.hash, std::string_view{rhs.str, rhs.strlen});
    }

    constexpr auto operator<=>(Node const &rhs) const noexcept {
      return std::tuple(hash, std::string_view{str, strlen}) <=>
             std::tuple(rhs.hash, std::string_view{rhs.str, rhs.strlen});
    }

    uint16_t hash = 0;
    uint16_t offset = 0;
    uint32_t strlen = 0;
    char const *str = nullptr;
  };

  template <std::same_as<ALLOCATOR> ALLOCATOR2>
  constexpr CompressionMap(ALLOCATOR2 &&alloc) noexcept
      : buckets{std::forward<ALLOCATOR2>(alloc)} {
  }

  auto insert(Node node) noexcept {
    auto it = std::upper_bound(buckets.begin(), buckets.end(), node);
    return buckets.insert(it, std::move(node));
  }

  auto find(Node node) noexcept {
    auto it = std::lower_bound(buckets.begin(), buckets.end(), node);
    return (it != buckets.end() && *it == node) ? it : buckets.end();
  }

  Vector<CompressionMap::Node, ALLOCATOR> buckets;
};

template <typename ALLOCATOR>
CompressionMap(ALLOCATOR &&) -> CompressionMap<ALLOCATOR>;

template <typename OUT_IT>
struct CountingOutputIterator {
  using difference_type = std::iter_difference_t<OUT_IT>;

  explicit constexpr CountingOutputIterator(OUT_IT it, size_t &shared_count) noexcept
      : m_shared_count{shared_count},
        m_it{it} {
  }

  template <typename T>
    requires(!std::same_as<CountingOutputIterator, std::decay_t<T>>)
  constexpr CountingOutputIterator &operator=(T &&value) {
    *m_it = char(std::forward<T>(value));
    ++m_shared_count.get();
    return *this;
  }

  constexpr CountingOutputIterator &operator*() noexcept {
    return *this;
  }

  constexpr CountingOutputIterator &operator++() noexcept {
    ++m_it;
    return *this;
  }

  constexpr CountingOutputIterator operator++(int) noexcept {
    auto it = *this;
    ++(*this);
    return it;
  }

  [[nodiscard]] constexpr size_t written() const noexcept {
    return m_shared_count.get();
  }

  [[nodiscard]] constexpr OUT_IT underlying_iter() const noexcept {
    return m_it;
  }

private:
  std::reference_wrapper<size_t> m_shared_count;
  OUT_IT m_it;
};

template <std::output_iterator<char> OUT>
struct WriteLabelSuccess {
  CountingOutputIterator<OUT> out;
  bool all_compressed;
};

template <std::output_iterator<char> OUT, AnAllocator ALLOCATOR>
constexpr Result<WriteLabelSuccess<OUT>, QuestionEncodeError>
write_label(CountingOutputIterator<OUT> out,
            std::string_view label,
            std::string_view remaining_name,
            CompressionMap<ALLOCATOR> &compression_map) {
  if (label.size() == 0) {
    return Failure{QuestionEncodeError::EMPTY_LABEL};
  }
  if (label.size() > 63) {
    return Failure{QuestionEncodeError::TOO_LONG_DOMAIN_NAME_LABEL};
  }

  typename CompressionMap<ALLOCATOR>::Node node{
      remaining_name,
      uint16_t(out.written()), // overflow handled a bit later
  };

  auto it = compression_map.find(node);
  if (it != compression_map.buckets.end()) {

    return WriteLabelSuccess{
        .out = detail::encode_compression_ptr(out, CompressionPointer{.offset = it->offset}),
        .all_compressed = true,
    };
  }

  if (out.written() <= CompressionPointer::MAX_VALUE) {
    // success does not matter. if there is an allocation error it is
    // still possible to encode question properly
    (void)compression_map.insert(node);
  }

  *out++ = char(label.size());
  return WriteLabelSuccess{
      .out = std::ranges::copy(label, out).out,
      .all_compressed = false,
  };
};

template <std::output_iterator<char> OUT, AnAllocator ALLOCATOR>
constexpr Result<CountingOutputIterator<OUT>, QuestionEncodeError>
encode_domain_name(CountingOutputIterator<OUT> out,
                   std::string_view name,
                   CompressionMap<ALLOCATOR> &compression_map) {
  if (name.size() > detail::FQDN_MAX_OCTET_LEN) {
    return Failure{QuestionEncodeError::TOO_LONG_DOMAIN_NAME};
  }

  for (std::string_view label : split_into_labels(name)) {
    std::string_view remaining_name{label.begin(), name.end()};
    COROSIG_TRY(auto write_label_res,
                detail::write_label(out, label, remaining_name, compression_map));
    out = write_label_res.out;
    if (write_label_res.all_compressed) {
      return out;
    }
  }
  *out++ = 0;
  return out;
}

} // namespace detail

struct DecodeQuestionEntrySuccess {

  DomainName name;
  QueryType qtype;
  QueryClass qclass = QueryClass::IN;
};

struct ResponseDecoder {
  ResponseDecoder(std::span<uint8_t const> response) noexcept;

  Result<Header, ResponseDecodeError> consume_header() noexcept;
  Result<DecodeQuestionEntrySuccess, ResponseDecodeError> consume_question_entry() noexcept;
  Result<ResourceRecord, ResponseDecodeError> consume_resource_record() noexcept;

private:
  void invalidate() noexcept;

  std::span<uint8_t const> m_original_message;
  std::span<uint8_t const> m_remaining_message = m_original_message;
};

template <std::output_iterator<char> OUT, AnAllocator ALLOCATOR>
constexpr Result<OUT, QuestionEncodeError>
encode_question(OUT out, Question question, ALLOCATOR &&alloc) {
  size_t written_count = 0;
  detail::CountingOutputIterator counting_out{out, written_count};

  Header header{
      .id = question.id,
      .flags = Header::Flags{}
                   .set_opcode(question.opcode)
                   .set_recursion_desired(question.recursion_desired),
      .qdcount = 1,
  };

  counting_out = detail::encode_header(counting_out, header);

  detail::CompressionMap compression_map{std::forward<ALLOCATOR>(alloc)};

  COROSIG_TRY(counting_out,
              detail::encode_domain_name(counting_out, question.name, compression_map));
  counting_out = detail::write_hton(counting_out, uint16_t(question.qtype));
  counting_out = detail::write_hton(counting_out, uint16_t(question.qclass));

  return counting_out.underlying_iter();
}

} // namespace corosig::dns

#endif
