#include "corosig/io/dns/Cache.hpp"

#include "corosig/io/dns/ACache.hpp"

namespace corosig::dns {

static_assert(AMutableCache<Cache<>>);
template struct Cache<>;

} // namespace corosig::dns
