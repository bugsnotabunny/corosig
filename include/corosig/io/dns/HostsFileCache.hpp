#ifndef COROSIG_IO_DNS_HOSTS_FILE_CACHE_HPP
#define COROSIG_IO_DNS_HOSTS_FILE_CACHE_HPP

#include "corosig/Coro.hpp"
#include "corosig/io/Sockaddr.hpp"
#include "corosig/io/dns/ACache.hpp"
#include "corosig/reactor/Reactor.hpp"

#include <span>
#include <string_view>

namespace corosig::dns {

constexpr static char const *HOSTS_FILE_PATH = "/etc/hosts";

struct HostsFileCache {
  HostsFileCache(Reactor &, char const *path) noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>>
  pull(std::string_view ascii_name, std::span<ResolvedAddress<IpvNAddr>> out) const noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>>
  pull(std::string_view ascii_name, std::span<ResolvedAddress<Ipv6Addr>> out) const noexcept;

  Fut<size_t, Error<AllocationError, SyscallError>>
  pull(std::string_view ascii_name, std::span<ResolvedAddress<Ipv4Addr>> out) const noexcept;

  [[nodiscard]] Reactor &underlying_reactor() const noexcept;

private:
  std::reference_wrapper<Reactor> m_reactor;
  char const *m_path;
};

} // namespace corosig::dns

#endif
