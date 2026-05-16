#ifndef COROSIG_IO_DNS_MEMORY_CACHE_HPP
#define COROSIG_IO_DNS_MEMORY_CACHE_HPP

#include "corosig/Clock.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/container/UniquePtr.hpp"
#include "corosig/container/Vector.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/meta/AlwaysOkResult.hpp"
#include "corosig/meta/AnAllocator.hpp"

#include <algorithm>
#include <boost/intrusive/avl_set.hpp>
#include <boost/intrusive/avl_set_hook.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace corosig::dns {

namespace detail {

struct NameStorageHeader;

struct AddrStorageHeader
    : boost::intrusive::slist_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>,
      boost::intrusive::avl_set_base_hook<
          boost::intrusive::link_mode<boost::intrusive::auto_unlink>,
          boost::intrusive::optimize_size<true>> {

  AddrStorageHeader(const AddrStorageHeader &) = delete;
  AddrStorageHeader(AddrStorageHeader &&) = delete;
  AddrStorageHeader &operator=(const AddrStorageHeader &) = delete;
  AddrStorageHeader &operator=(AddrStorageHeader &&) = delete;

  template <AnAllocator ALLOC, typename IPV4S, typename IPV6S>
    requires(std::ranges::sized_range<IPV4S> &&
             std::same_as<std::ranges::range_value_t<IPV4S>, Ipv4Addr> &&
             std::ranges::sized_range<IPV6S> &&
             std::same_as<std::ranges::range_value_t<IPV6S>, Ipv6Addr>)
  static UniquePtr<AddrStorageHeader, ALLOC> make(ALLOC &&alloc,
                                                  NameStorageHeader &parent,
                                                  SteadyClock::time_point expires_at,
                                                  IPV4S &&ipv4s,
                                                  IPV6S &&ipv6s) noexcept {
    size_t allocation_size = sizeof(AddrStorageHeader);
    allocation_size += ipv4s.size() * sizeof(Ipv4Addr);
    allocation_size += ipv6s.size() * sizeof(Ipv6Addr);

    void *mem = alloc.allocate(allocation_size, alignof(AddrStorageHeader));
    if (mem == nullptr) {
      return UniquePtr<AddrStorageHeader, ALLOC>{nullptr, std::forward<ALLOC>(alloc)};
    }

    auto *header = new (mem) AddrStorageHeader{parent, expires_at, ipv4s, ipv6s};
    return UniquePtr<AddrStorageHeader, ALLOC>{header, std::forward<ALLOC>(alloc)};
  }

  template <AnAllocator ALLOC, typename IPV4S>
    requires(std::ranges::sized_range<IPV4S> &&
             std::same_as<std::ranges::range_value_t<IPV4S>, Ipv4Addr>)
  static UniquePtr<AddrStorageHeader, ALLOC> make(ALLOC &&alloc,
                                                  NameStorageHeader &parent,
                                                  SteadyClock::time_point expires_at,
                                                  IPV4S &&addrs) noexcept {
    return make(std::forward<ALLOC>(alloc),
                parent,
                expires_at,
                std::forward<IPV4S>(addrs),
                std::span<Ipv6Addr>{});
  }

  template <AnAllocator ALLOC, typename IPV6S>
    requires(std::ranges::sized_range<IPV6S> &&
             std::same_as<std::ranges::range_value_t<IPV6S>, Ipv6Addr>)
  static UniquePtr<AddrStorageHeader, ALLOC> make(ALLOC &&alloc,
                                                  NameStorageHeader &parent,
                                                  SteadyClock::time_point expires_at,
                                                  IPV6S &&addrs) noexcept {
    return make(std::forward<ALLOC>(alloc),
                parent,
                expires_at,
                std::span<Ipv4Addr>{},
                std::forward<IPV6S>(addrs));
  }

  std::span<Ipv4Addr const> ipv4s() const noexcept;
  std::span<Ipv4Addr> ipv4s() noexcept;

  std::span<Ipv6Addr const> ipv6s() const noexcept;
  std::span<Ipv6Addr> ipv6s() noexcept;

  SteadyClock::time_point expires_at() const noexcept;

  NameStorageHeader &parent() const noexcept;

  auto operator<=>(AddrStorageHeader const &rhs) const noexcept {
    return expires_at() <=> rhs.expires_at();
  }

private:
  template <typename IPV4S, typename IPV6S>
    requires(std::ranges::sized_range<IPV4S> &&
             std::same_as<std::ranges::range_value_t<IPV4S>, Ipv4Addr> &&
             std::ranges::sized_range<IPV6S> &&
             std::same_as<std::ranges::range_value_t<IPV6S>, Ipv6Addr>)
  AddrStorageHeader(NameStorageHeader &parent,
                    SteadyClock::time_point expires_at,
                    IPV4S &&ipv4s_list,
                    IPV6S &&ipv6s_list) noexcept;

  Ipv4Addr *ipv4s_begin() noexcept;

  Ipv4Addr const *ipv4s_begin() const noexcept;

  std::reference_wrapper<NameStorageHeader> m_parent;
  SteadyClock::time_point m_expires_at;
  uint32_t m_ipv4s_count;
  uint32_t m_ipv6s_count;
};

static_assert(alignof(AddrStorageHeader) >= alignof(Ipv4Addr));
static_assert(alignof(Ipv4Addr) >= alignof(Ipv6Addr));

struct NameStorageHeader : boost::intrusive::avl_set_base_hook<
                               boost::intrusive::link_mode<boost::intrusive::auto_unlink>,
                               boost::intrusive::optimize_size<true>> {
  template <AnAllocator ALLOC>
  static UniquePtr<NameStorageHeader, ALLOC> make(ALLOC &&alloc, std::string_view name) noexcept {
    size_t allocation_size = sizeof(NameStorageHeader) + name.size();

    void *mem = alloc.allocate(allocation_size, alignof(NameStorageHeader));
    if (mem == nullptr) {
      return UniquePtr<NameStorageHeader, ALLOC>{nullptr, std::forward<ALLOC>(alloc)};
    }

    auto *header = new (mem) NameStorageHeader{name};
    return UniquePtr<NameStorageHeader, ALLOC>{header, std::forward<ALLOC>(alloc)};
  }

  NameStorageHeader(const NameStorageHeader &) = delete;
  NameStorageHeader(NameStorageHeader &&) = delete;
  NameStorageHeader &operator=(const NameStorageHeader &) = delete;
  NameStorageHeader &operator=(NameStorageHeader &&) = delete;

  ~NameStorageHeader() = default;

  std::string_view name() const noexcept;

  auto operator<=>(std::string_view other) const noexcept {
    return name() <=> other;
  }

  auto operator<=>(NameStorageHeader const &other) const noexcept {
    return *this <=> other.name();
  }

  using slist_type =
      boost::intrusive::slist<AddrStorageHeader, boost::intrusive::constant_time_size<false>>;

  slist_type ips;

private:
  NameStorageHeader(std::string_view name_) noexcept;

  size_t m_name_size;
};

template <typename IPV4S, typename IPV6S>
  requires(std::ranges::sized_range<IPV4S> &&
           std::same_as<std::ranges::range_value_t<IPV4S>, Ipv4Addr> &&
           std::ranges::sized_range<IPV6S> &&
           std::same_as<std::ranges::range_value_t<IPV6S>, Ipv6Addr>)
inline AddrStorageHeader::AddrStorageHeader(NameStorageHeader &parent,
                                            SteadyClock::time_point expires_at,
                                            IPV4S &&ipv4s_list,
                                            IPV6S &&ipv6s_list) noexcept
    : m_parent{parent},
      m_expires_at{expires_at},
      m_ipv4s_count{uint32_t(ipv4s_list.size())},
      m_ipv6s_count{uint32_t(ipv6s_list.size())} {
  assert(ipv4s_list.size() <= std::numeric_limits<uint32_t>::max());
  assert(ipv6s_list.size() <= std::numeric_limits<uint32_t>::max());
  std::ranges::copy(ipv4s_list, ipv4s().begin());
  std::ranges::copy(ipv6s_list, ipv6s().begin());
  parent.ips.push_front(*this);
}

using memory_cache_names = boost::intrusive::avl_set<detail::NameStorageHeader,
                                                     boost::intrusive::constant_time_size<false>>;

template <typename IP, typename IP2>
AlwaysOkResult<size_t> memory_cache_pull_impl(
    std::add_pointer_t<std::span<IP2 const>(AddrStorageHeader const &)> ip_source,
    memory_cache_names const &names,
    std::string_view ascii_name,
    std::span<ResolvedAddress<IP>> out) noexcept;

} // namespace detail

template <AnAllocator ALLOCATOR = AllocatorRef<Allocator>>
struct MemoryCache {
  MemoryCache(ALLOCATOR alloc) noexcept
      : m_alloc{std::forward<ALLOCATOR>(alloc)} {
  }

  MemoryCache(const MemoryCache &) noexcept = delete;
  MemoryCache(MemoryCache &&) noexcept = default;
  MemoryCache &operator=(const MemoryCache &) noexcept = delete;
  MemoryCache &operator=(MemoryCache &&) noexcept = default;

  ~MemoryCache() {
    auto deleter = [&](auto *p) noexcept { m_alloc.deallocate(p); };
    m_addrs_by_expire_time.clear_and_dispose(deleter);
    m_names.clear_and_dispose(deleter);
  }

  AlwaysOkResult<size_t> pull(std::string_view ascii_name,
                              std::span<ResolvedAddress<Ipv6Addr>> out) const noexcept {
    return detail::memory_cache_pull_impl<Ipv6Addr, Ipv6Addr>(
        [](detail::AddrStorageHeader const &addr_storage) { return addr_storage.ipv6s(); },
        m_names,
        ascii_name,
        out);
  }

  AlwaysOkResult<size_t> pull(std::string_view ascii_name,
                              std::span<ResolvedAddress<Ipv4Addr>> out) const noexcept {
    return detail::memory_cache_pull_impl<Ipv4Addr, Ipv4Addr>(
        [](detail::AddrStorageHeader const &addr_storage) { return addr_storage.ipv4s(); },
        m_names,
        ascii_name,
        out);
  }

  Result<void, AllocationError> push(std::string_view ascii_name,
                                     std::span<ResolvedAddress<Ipv4Addr> const> addrs) noexcept {
    return push_impl(ascii_name, addrs);
  }

  Result<void, AllocationError> push(std::string_view ascii_name,
                                     std::span<ResolvedAddress<Ipv6Addr> const> addrs) noexcept {
    return push_impl(ascii_name, addrs);
  }

  void prune() noexcept {
    auto now = SteadyClock::now();

    auto it = m_addrs_by_expire_time.begin();
    while (it != m_addrs_by_expire_time.end() && it->expires_at() <= now) {

      detail::NameStorageHeader &parent = it->parent();

      it = m_addrs_by_expire_time.erase_and_dispose(it, AllocatorBoundDeleter<ALLOCATOR>{m_alloc});

      if (parent.ips.empty()) {
        parent.~NameStorageHeader();
        m_alloc.deallocate(&parent);
      }
    }
  }

  [[nodiscard]] ALLOCATOR &underlying_allocator() noexcept {
    return m_alloc;
  }

private:
  template <typename IP>
  Result<void, AllocationError> push_impl(std::string_view ascii_name,
                                          std::span<ResolvedAddress<IP> const> addrs) noexcept {
    auto name_it = m_names.lower_bound(ascii_name, std::less<>{});

    UniquePtr<detail::NameStorageHeader, ptr_allocator_ref_type> new_name_store{
        nullptr,
        ptr_allocator_ref_type{m_alloc},
    };

    if (name_it == m_names.end() || !std::ranges::equal(name_it->name(), ascii_name)) {
      new_name_store = detail::NameStorageHeader::make(ptr_allocator_ref_type{m_alloc}, ascii_name);

      if (new_name_store == nullptr) {
        return Failure{AllocationError{}};
      }

      name_it = m_names.insert_before(name_it, *new_name_store);
    }

    Vector<ResolvedAddress<IP>, ptr_allocator_ref_type> sorted_addrs{m_alloc};

    static_assert(std::is_trivially_destructible_v<ResolvedAddress<IP>>);
    COROSIG_TRYV(sorted_addrs.resize_uninitialized(addrs.size()));

    auto now = SteadyClock::now();
    auto new_end = std::ranges::copy_if(addrs, sorted_addrs.begin(), [&](ResolvedAddress<IP> addr) {
                     return addr.expires_at > now;
                   }).out;
    (void)sorted_addrs.resize_uninitialized(new_end - sorted_addrs.begin());

    std::ranges::sort(sorted_addrs, [](ResolvedAddress<IP> lhs, ResolvedAddress<IP> rhs) {
      return lhs.expires_at < rhs.expires_at;
    });

    struct MultisetGuard {
      MultisetGuard(ptr_allocator_ref_type alloc)
          : alloc{alloc} {
      }

      ~MultisetGuard() {
        set.clear_and_dispose(AllocatorBoundDeleter<ptr_allocator_ref_type>{alloc});
      }

      addrs_by_expire_time set;
      ptr_allocator_ref_type alloc;
    } addrs_by_expire_time_buf{m_alloc};

    auto it = sorted_addrs.begin();
    while (it != sorted_addrs.end()) {
      auto it2 = std::find_if(it, sorted_addrs.end(), [&](ResolvedAddress<IP> addr) {
        return it->expires_at < addr.expires_at;
      });

      auto r =
          std::ranges::subrange{it, it2} | std::views::transform(&ResolvedAddress<IP>::address);
      UniquePtr addrs_store = detail::AddrStorageHeader::make(
          ptr_allocator_ref_type{m_alloc}, *name_it, it->expires_at, r);

      if (addrs_store == nullptr) {
        return Failure{AllocationError{}};
      }

      addrs_by_expire_time_buf.set.insert(*addrs_store);
      (void)addrs_store.release();

      it = it2;
    }

    addrs_by_expire_time_buf.set.clear_and_dispose(
        [&](auto *addr) { m_addrs_by_expire_time.insert(*addr); });

    (void)new_name_store.release();
    return Ok{};
  }

  using ptr_allocator_ref_type = AllocatorRef<ALLOCATOR>;

  using addrs_by_expire_time =
      boost::intrusive::avl_multiset<detail::AddrStorageHeader,
                                     boost::intrusive::constant_time_size<false>>;

  ALLOCATOR m_alloc;
  detail::memory_cache_names m_names;
  addrs_by_expire_time m_addrs_by_expire_time;
};

extern template struct MemoryCache<AllocatorRef<Allocator>>;

} // namespace corosig::dns

#endif
