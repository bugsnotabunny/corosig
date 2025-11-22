#ifndef COROSIG_META_LAMBDIZE_HPP
#define COROSIG_META_LAMBDIZE_HPP

#define COROSIG_LAMBDIZE(...)                                                                      \
  []<typename... ARGS>(ARGS &&...args) { return __VA_ARGS__(std::forward<ARGS>(args)...); }

#endif
