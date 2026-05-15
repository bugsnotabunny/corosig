#ifndef COROSIG_IO_DNS_MEMORY_CACHE_HPP
#define COROSIG_IO_DNS_MEMORY_CACHE_HPP

#include "corosig/Clock.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/container/UniquePtr.hpp"
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
#include <cstddef>
#include <cstdint>
#include <functional>
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

  template <AnAllocator ALLOC>
  static UniquePtr<AddrStorageHeader, ALLOC> make(ALLOC &&alloc,
                                                  NameStorageHeader &parent,
                                                  SteadyClock::time_point expires_at,
                                                  std::span<Ipv4Addr const> ipv4s,
                                                  std::span<Ipv6Addr const> ipv6s) noexcept {
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
  AddrStorageHeader(NameStorageHeader &parent,
                    SteadyClock::time_point expires_at,
                    std::span<Ipv4Addr const> ipv4s_list,
                    std::span<Ipv6Addr const> ipv6s_list) noexcept;

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

  AlwaysOkResult<size_t> pull(std::string_view ascii_name,
                              std::span<ResolvedAddress<IpvNAddr>> out) const noexcept {
    auto res1 = detail::memory_cache_pull_impl<IpvNAddr, Ipv4Addr>(
        [](detail::AddrStorageHeader const &addr_storage) { return addr_storage.ipv4s(); },
        m_names,
        ascii_name,
        out);
    out = out.subspan(res1.value());
    auto res2 = detail::memory_cache_pull_impl<IpvNAddr, Ipv6Addr>(
        [](detail::AddrStorageHeader const &addr_storage) { return addr_storage.ipv6s(); },
        m_names,
        ascii_name,
        out);
    return res1.value() + res2.value();
  }

  Result<void, AllocationError> push(CachePushTransaction transaction) noexcept {
    auto it = m_names.lower_bound(transaction.name, std::less<>{});

    UniquePtr<detail::NameStorageHeader, ptr_allocator_ref_type> new_name_store{
        nullptr,
        ptr_allocator_ref_type{m_alloc},
    };

    if (it == m_names.end() || !std::ranges::equal(it->name(), transaction.name)) {
      new_name_store =
          detail::NameStorageHeader::make(ptr_allocator_ref_type{m_alloc}, transaction.name);

      if (new_name_store == nullptr) {
        return Failure{AllocationError{}};
      }

      it = m_names.insert_before(it, *new_name_store);
    }

    UniquePtr addrs_store = detail::AddrStorageHeader::make(ptr_allocator_ref_type{m_alloc},
                                                            *it,
                                                            transaction.expire_at,
                                                            transaction.ipv4s,
                                                            transaction.ipv6s);

    if (addrs_store == nullptr) {
      return Failure{AllocationError{}};
    }

    m_addrs_by_expire_time.insert(*addrs_store);

    (void)new_name_store.release();
    (void)addrs_store.release();
    return Ok{};
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
