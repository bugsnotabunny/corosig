#ifndef COROSIG_IO_DNS_PROTOCOL_HPP
#define COROSIG_IO_DNS_PROTOCOL_HPP

#include "corosig/Result.hpp"
#include "corosig/container/Vector.hpp"
#include "corosig/io/dns/ProtocolErrors.hpp"
#include "corosig/meta/AnAllocator.hpp"
#include "corosig/util/Endianness.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace corosig::dns {

static_assert(sizeof(uint8_t) * CHAR_BIT == 8, "Platforms with wide uint8_t are not supported");
static_assert(sizeof(uint16_t) * CHAR_BIT == 16, "Platforms with wide uint16_t are not supported");

enum class QueryType : uint8_t {
  /// @brief Requests an IPv4 address
  A = 1,

  /// @brief Requests an IPv6 address
  AAAA = 28,

  /// @brief Requests mail exchange records
  MX = 15,

  /// @brief Requests the canonical name for an alias
  CNAME = 5,

  /// @brief Requests authoritative name servers
  NS = 2,

  /// @brief Requests a domain name pointer (used for reverse lookups)
  PTR = 12,

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
  C_CSNET = 2,

  /// @brief Chaos. Sometimes used internally by some DNS server software for querying server
  ///        statistics and version information
  CH = 3,

  /// @brief HESIOD class. Used by the Hesiod project at MIT for distributing information (like user
  ///        accounts)
  HS = 4,

  /// @brief None class
  C_NONE = 254,

  /// @brief Requests all records, regardless of class
  ANY = 255,
};

enum class QueryOpcode : uint8_t {
  STANDARD = 0,
  INVERSE = 1,
  STATUS = 2,
};

namespace detail {

struct Header {
  struct Flags {

    [[nodiscard]] bool is_response() const {
      return (flags1 & 0x80) == 0x80;
    }

    Flags &set_query() {
      flags1 &= ~0x80;
      return *this;
    }

    Flags &set_response() {
      flags1 |= 0x80;
      return *this;
    }

    Flags &set_opcode(QueryOpcode opcode) {
      flags1 = (flags1 & 0x87) | ((std::underlying_type_t<QueryOpcode>(opcode) & 0x0F) << 3);
      return *this;
    }

    [[nodiscard]] uint8_t opcode() const {
      return (flags1 >> 3) & 0x0F;
    }

    Flags &set_authoritative_answer(bool value) {
      if (value) {
        flags1 |= 0x04;
      } else {
        flags1 &= ~0x04;
      }
      return *this;
    }

    [[nodiscard]] bool authoritative_answer() const {
      return (flags1 & 0x04) == 0x04;
    }

    Flags &set_truncated(bool value) {
      if (value) {
        flags1 |= 0x02;
      } else {
        flags1 &= ~0x02;
      }
      return *this;
    }

    [[nodiscard]] bool truncated() const {
      return (flags1 & 0x02) == 0x02;
    }

    Flags &set_recursion_desired(bool value) {
      if (value) {
        flags1 |= 0x01;
      } else {
        flags1 &= ~0x01;
      }
      return *this;
    }

    [[nodiscard]] bool recursion_desired() const {
      return (flags1 & 0x01) == 0x01;
    }

    Flags &set_recursion_available(bool value) {
      if (value) {
        flags2 |= 0x80;
      } else {
        flags2 &= ~0x80;
      }
      return *this;
    }

    [[nodiscard]] bool recursion_available() const {
      return (flags2 & 0x80) == 0x80;
    }

    Flags &set_rcode(uint8_t rcode) {
      flags2 = (flags2 & 0xF0) | (rcode & 0x0F);
      return *this;
    }

    [[nodiscard]] uint8_t get_rcode() const {
      return flags2 & 0x0F;
    }

    Flags &set_standard_query(bool recursion_desired = true) {
      set_query();
      set_opcode(QueryOpcode::STANDARD);
      set_recursion_desired(recursion_desired);
      return *this;
    }

    Flags &set_standard_response() {
      set_response();
      set_recursion_available(false);
      return *this;
    }

    constexpr auto operator<=>(Flags const &rhs) const noexcept = default;

    uint8_t flags1 = 0; // QR(1) + Opcode(4) + AA(1) + TC(1) + RD(1)
    uint8_t flags2 = 0; // RA(1) + Z(3) + RCODE(4)
  };

  constexpr auto operator<=>(Header const &rhs) const noexcept = default;

  uint16_t id = 0;
  Flags flags = Flags{};
  uint16_t qdcount = 0;
  uint16_t ancount = 0;
  uint16_t nscount = 0;
  uint16_t arcount = 0;
};

template <std::output_iterator<char> OUT>
OUT write_hton(OUT out, uint16_t value) {
  *out++ = char((value >> 8) & 0xFF);
  *out++ = char(value & 0xFF);
  return out;
};

template <std::output_iterator<char> OUT>
OUT encode_header(OUT out, Header header) {
  out = write_hton(out, header.id);
  // flags are always in network order
  *out++ = header.flags.flags1;
  *out++ = header.flags.flags2;
  out = write_hton(out, header.qdcount);
  out = write_hton(out, header.ancount);
  out = write_hton(out, header.nscount);
  out = write_hton(out, header.arcount);
  return out;
}

Result<Header, ResponseDecodeError> decode_header(std::span<uint8_t const> input) noexcept;

// RFC 1035 section 4.1.4
constexpr auto COMPRESSION_PTR_MAX_VALUE = 0x3FFF;

template <std::output_iterator<char> OUT>
OUT write_compression_ptr(OUT out, uint16_t offset) {
  assert(offset <= COMPRESSION_PTR_MAX_VALUE &&
         "Compression pointer overflow. Only 14 bits are allowed");
  uint16_t compression_ptr = 0xC000 | offset;
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
  CompressionMap(ALLOCATOR2 &&alloc) noexcept
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

  explicit CountingOutputIterator(OUT_IT it, size_t &shared_count) noexcept
      : m_shared_count{shared_count},
        m_it{it} {
  }

  template <typename T>
    requires(!std::same_as<CountingOutputIterator, std::decay_t<T>>)
  CountingOutputIterator &operator=(T &&value) {
    *m_it = char(std::forward<T>(value));
    ++m_shared_count.get();
    return *this;
  }

  CountingOutputIterator &operator*() noexcept {
    return *this;
  }

  CountingOutputIterator &operator++() noexcept {
    ++m_it;
    return *this;
  }

  CountingOutputIterator operator++(int) noexcept {
    auto it = *this;
    ++(*this);
    return it;
  }

  [[nodiscard]] size_t written() const noexcept {
    return m_shared_count.get();
  }

  [[nodiscard]] OUT_IT underlying_iter() const noexcept {
    return m_it;
  }

private:
  std::reference_wrapper<size_t> m_shared_count;
  OUT_IT m_it;
};

static_assert(
    std::output_iterator<CountingOutputIterator<std::back_insert_iterator<std::vector<char>>>,
                         char>);

template <std::output_iterator<char> OUT>
struct WriteLabelSuccess {
  CountingOutputIterator<OUT> out;
  bool all_compressed;
};

template <std::output_iterator<char> OUT, AnAllocator ALLOCATOR>
Result<WriteLabelSuccess<OUT>, QuestionEncodeError>
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
        .out = detail::write_compression_ptr(out, it->offset),
        .all_compressed = true,
    };
  }

  if (out.written() <= COMPRESSION_PTR_MAX_VALUE) {
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
Result<CountingOutputIterator<OUT>, QuestionEncodeError>
write_name(CountingOutputIterator<OUT> out,
           std::string_view name,
           CompressionMap<ALLOCATOR> &compression_map) {
  if (name.size() > 255) {
    return Failure{QuestionEncodeError::TOO_LONG_DOMAIN_NAME};
  }

  for (auto &&label : name | std::views::split('.')) {
    std::string_view remaining_name{label.begin(), name.end()};
    std::string_view labelsv{label.begin(), label.size()};
    COROSIG_TRY(auto write_label_res,
                detail::write_label(out, labelsv, remaining_name, compression_map));
    out = write_label_res.out;
    if (write_label_res.all_compressed) {
      return out;
    }
  }
  *out++ = 0;
  return out;
}

} // namespace detail

struct QuestionParams {
  uint16_t id;
  QueryOpcode opcode = QueryOpcode::STANDARD;
  bool recursion_desired = true;
};

struct QuestionQueryEntry {
  std::string_view name;
  QueryType qtype;
  QueryClass qclass = QueryClass::IN;
};

template <std::output_iterator<char> OUT, std::ranges::sized_range ENTRIES, AnAllocator ALLOCATOR>
Result<OUT, QuestionEncodeError>
encode_question(OUT out, QuestionParams params, ENTRIES const &entries, ALLOCATOR &&alloc) {
  if (entries.size() >= std::numeric_limits<uint16_t>::max()) {
    return Failure{QuestionEncodeError::TOO_MANY_QUESTION_ENTRIES};
  }

  size_t written_count = 0;
  detail::CountingOutputIterator counting_out{out, written_count};

  counting_out =
      detail::encode_header(counting_out,
                            detail::Header{
                                .id = params.id,
                                .flags = detail::Header::Flags{}
                                             .set_opcode(params.opcode)
                                             .set_recursion_desired(params.recursion_desired),
                                .qdcount = uint16_t(entries.size()),
                            });

  detail::CompressionMap compression_map{std::forward<ALLOCATOR>(alloc)};

  for (QuestionQueryEntry const &entry : entries) {
    COROSIG_TRY(counting_out, detail::write_name(counting_out, entry.name, compression_map));
    counting_out = detail::write_hton(counting_out, std::underlying_type_t<QueryType>(entry.qtype));
    counting_out =
        detail::write_hton(counting_out, std::underlying_type_t<QueryClass>(entry.qclass));
  }

  return counting_out.underlying_iter();
}

template <std::output_iterator<char> OUT, AnAllocator ALLOCATOR>
Result<OUT, QuestionEncodeError> encode_question(OUT out,
                                                 QuestionParams params,
                                                 QuestionQueryEntry const &entry,
                                                 ALLOCATOR &&alloc) {
  return encode_question(out, params, std::views::single(entry), std::forward<ALLOCATOR>(alloc));
}

template <std::output_iterator<char> OUT, AnAllocator ALLOCATOR>
Result<OUT, QuestionEncodeError>
encode_question(OUT out,
                QuestionParams params,
                std::initializer_list<QuestionQueryEntry> const &entries,
                ALLOCATOR &&alloc) {
  return encode_question(out, params, std::span{entries}, std::forward<ALLOCATOR>(alloc));
}

} // namespace corosig::dns

#endif
