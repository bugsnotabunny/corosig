#include "corosig/io/dns/Cache.hpp"

namespace corosig::dns {

template struct Cache<AllocatorRef<Allocator>>;

} // namespace corosig::dns
