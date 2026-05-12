#ifndef COROSIG_IO_DNS_HOSTS_FILE_CACHE_HPP
#define COROSIG_IO_DNS_HOSTS_FILE_CACHE_HPP

#include "corosig/Coro.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <span>
#include <string_view>

namespace corosig::dns {

struct HostsFileCache {
  HostsFileCache(Reactor &, char const *path) noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>> pull(std::string_view ascii_name,
                                                         std::span<Ipv6Addr> out) const noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>> pull(std::string_view ascii_name,
                                                         std::span<Ipv4Addr> out) const noexcept;

  [[nodiscard]] Reactor &underlying_reactor() const noexcept;

private:
  std::reference_wrapper<Reactor> m_reactor;
  char const *m_path;
};

static_assert(ACache<HostsFileCache>);

} // namespace corosig::dns

#endif
