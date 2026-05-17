#include "corosig/io/dns/HostsFileCache.hpp"

#include "corosig/Clock.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/io/File.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/io/dns/Protocol.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace {

using namespace corosig;

static_assert(dns::ACache<dns::HostsFileCache>);

struct HostsFileParser {
  template <typename IP>
  static Fut<size_t, Error<AllocationError, SyscallError>>
  parse_and_collect(Reactor &r,
                    char const *path,
                    std::string_view ascii_name,
                    std::span<dns::ResolvedAddress<IP>> out) noexcept {
    assert(dns::detail::debug_is_ascii(ascii_name));
    if (ascii_name.size() > dns::detail::FQDN_MAX_OCTET_LEN) {
      co_return 0;
    }

    std::array<char, dns::detail::FQDN_MAX_OCTET_LEN> ascii_name_lowercase_buf;
    char const *end =
        std::ranges::transform(ascii_name, ascii_name_lowercase_buf.begin(), dns::detail::to_lower)
            .out;
    std::string_view ascii_name_lowercase = {ascii_name_lowercase_buf.begin(), end};

    COROSIG_CO_TRY(auto file, co_await File::open(r, path));
    HostsFileParser self{std::move(file)};
    COROSIG_CO_TRYV(co_await self.read_to_free_buffer(r));

    size_t returned = 0;
    std::optional<IP> addr;
    while (!out.empty() && self.m_used_buffer != 0) {
      self.shift_buffer_to_left();
      COROSIG_CO_TRYV(co_await self.read_to_free_buffer(r));
      using enum State;
      switch (self.m_state) {
      case AT_IP: {
        addr = std::nullopt;
        self.skip_ws();
        auto *it = self.find_next_ws();
        std::span used_buf = self.get_used_buffer();
        self.adwance(it - used_buf.begin().base());
        if (it == used_buf.end().base() || *it == '#' || *it == '\n') {
          self.m_state = SKIPPING_LINE;
          continue;
        }

        addr = IP::parse(std::string_view{used_buf.data(), it});
        self.m_state = addr ? AT_NAME : SKIPPING_LINE;
        break;
      }

      case AT_NAME: {
        self.skip_ws();
        auto *it = self.find_next_ws();
        std::span used_buf = self.get_used_buffer();
        self.adwance(it - used_buf.begin().base());

        if (it == used_buf.end().base() || *it == '#') {
          self.m_state = SKIPPING_LINE;
          continue;
        }

        for (auto *jt = used_buf.begin().base(); jt != it; ++jt) {
          *jt = dns::detail::to_lower(*jt);
        }

        std::string_view domain_name{used_buf.begin().base(), it};

        if (std::ranges::equal(domain_name, ascii_name_lowercase)) {
          dns::ResolvedAddress<IP> &result = out.front();
          assert(addr && "Addr shall have been set before going into NAME state");
          result.address = *addr; // NOLINT (bugprone-unchecked-optional-access)
          // shall only be used for current transaction
          result.expires_at = SteadyClock::time_point{SteadyClock::duration{0}};
          out = out.subspan(1);
          ++returned;
        }

        if (*it == '\n') {
          self.m_state = SKIPPING_LINE;
          continue;
        }

        break;
      }

      case SKIPPING_LINE:
        self.skip_line();
        break;
      }
    }

    co_return returned;
  }

private:
  enum class State : uint8_t {
    AT_IP,
    AT_NAME,
    SKIPPING_LINE,
  };

  explicit HostsFileParser(File input) noexcept
      : m_input{std::move(input)} {
  }

  Fut<void, Error<AllocationError, SyscallError>> read_to_free_buffer(Reactor &r) noexcept {
    COROSIG_CO_TRY(size_t bytes_read, co_await m_input.read(r, get_free_buffer()));
    m_used_buffer += bytes_read;
    co_return Ok{};
  }

  void skip_ws() noexcept {
    size_t adwance_amount = 0;
    for (char c : get_used_buffer()) {
      if (c != ' ' && c != '\t') {
        break;
      }
      ++adwance_amount;
    }
    adwance(adwance_amount);
  }

  char *find_next_ws() noexcept {
    std::span used_buf = get_used_buffer();
    auto *it = std::ranges::find_first_of(used_buf, " \t#\n").base();
    return it;
  }

  void skip_line() noexcept {
    using enum State;
    std::span used_buf = get_used_buffer();
    auto it = std::ranges::find(used_buf, '\n');
    if (it != used_buf.end()) {
      m_state = AT_IP;
    }
    adwance(std::min(m_buffer.size(), size_t(it.base() - m_buffer.begin() + 1)));
  }

  std::span<char> get_free_buffer() noexcept {
    return std::span{m_buffer}.subspan(m_unused_front_buffer + m_used_buffer);
  }

  std::span<char> get_used_buffer() noexcept {
    return std::span{m_buffer}.subspan(m_unused_front_buffer, m_used_buffer);
  }

  void shift_buffer_to_left() noexcept {
    std::span used = get_used_buffer();
    std::memmove(m_buffer.begin(), used.data(), used.size());
    m_unused_front_buffer = 0;
  }

  void adwance(size_t n) noexcept {
    assert(n <= m_used_buffer);
    m_used_buffer -= n;
    m_unused_front_buffer += n;
  }

  File m_input;
  uint16_t m_unused_front_buffer = 0;
  uint16_t m_used_buffer = 0;
  State m_state = State::AT_IP;
  std::array<char, 511> m_buffer;
};

} // namespace

namespace corosig::dns {

HostsFileCache::HostsFileCache(Reactor &r, char const *path) noexcept
    : m_reactor{r},
      m_path{path} {
}

Fut<size_t, Error<AllocationError, SyscallError>>
HostsFileCache::pull(std::string_view ascii_name,
                     std::span<ResolvedAddress<Ipv4Addr>> out) const noexcept {
  return HostsFileParser::parse_and_collect(m_reactor, m_path, ascii_name, out);
}

Fut<size_t, Error<AllocationError, SyscallError>>
HostsFileCache::pull(std::string_view ascii_name,
                     std::span<ResolvedAddress<Ipv6Addr>> out) const noexcept {
  return HostsFileParser::parse_and_collect(m_reactor, m_path, ascii_name, out);
}

Fut<size_t, Error<AllocationError, SyscallError>>
HostsFileCache::pull(std::string_view ascii_name,
                     std::span<ResolvedAddress<IpvNAddr>> out) const noexcept {
  return HostsFileParser::parse_and_collect(m_reactor, m_path, ascii_name, out);
}

Reactor &HostsFileCache::underlying_reactor() const noexcept {
  return m_reactor;
}

} // namespace corosig::dns
