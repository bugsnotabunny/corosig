#ifndef COROSIG_IO_DNS_CACHE_HPP
#define COROSIG_IO_DNS_CACHE_HPP

#include "corosig/Coro.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/container/Allocator.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/io/dns/HostsFileCache.hpp"
#include "corosig/io/dns/MemoryCache.hpp"
#include "corosig/meta/AnAllocator.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace corosig::dns {

template <AnAllocator ALLOCATOR = AllocatorRef<Allocator>>
struct Cache {
  Cache(Reactor &reactor, char const *hosts_path, ALLOCATOR alloc) noexcept
      : m_hosts_cache{reactor, hosts_path},
        m_mem_cache{alloc} {
  }

  Fut<size_t, Error<AllocationError, SyscallError>> pull(std::string_view ascii_name,
                                                         std::span<Ipv6Addr> out) const noexcept {
    return pull_impl(m_hosts_cache.underlying_reactor(), ascii_name, out);
  }

  Fut<size_t, Error<AllocationError, SyscallError>> pull(std::string_view ascii_name,
                                                         std::span<Ipv4Addr> out) const noexcept {
    return pull_impl(m_hosts_cache.underlying_reactor(), ascii_name, out);
  }

  Result<void, Error<AllocationError>> push(CachePushTransaction transaction) noexcept {
    return m_mem_cache.push(transaction);
  }

  void prune() noexcept {
    return m_mem_cache.prune();
  }

private:
  template <typename IP>
  Fut<size_t, Error<AllocationError, SyscallError>>
  pull_impl(Reactor &, std::string_view ascii_name, std::span<IP> out) const noexcept {
    Result res = co_await m_hosts_cache.pull(ascii_name, out);
    if (res.is_ok() && res.value() != 0) {
      co_return res;
    } else {
      co_return Result<size_t, Error<AllocationError, SyscallError>>{
          m_mem_cache.pull(ascii_name, out),
      };
    }
  }

  HostsFileCache m_hosts_cache;
  MemoryCache<ALLOCATOR> m_mem_cache;
};

extern template struct Cache<AllocatorRef<Allocator>>;

} // namespace corosig::dns

#endif
