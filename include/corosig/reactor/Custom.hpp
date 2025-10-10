#ifndef COROSIG_REACTOR_CUSTOM_HPP
#define COROSIG_REACTOR_CUSTOM_HPP

#include "corosig/reactor/CoroList.hpp"
#include "corosig/reactor/PollList.hpp"

#include <concepts>
#include <cstddef>

namespace corosig {

template <typename REACTOR>
concept AReactor = requires(REACTOR &reactor) {
  { reactor.allocate_frame(std::declval<size_t>()) } noexcept -> std::same_as<void *>;
  { reactor.free_frame(std::declval<void *>()) } noexcept -> std::same_as<void>;
  { reactor.yield(std::declval<CoroListNode &>()) } noexcept -> std::same_as<void>;
  { reactor.poll(std::declval<PollListNode &>()) } noexcept -> std::same_as<void>;
  { reactor.do_event_loop_iteration() } noexcept;
  { REACTOR::instance() } noexcept -> std::same_as<REACTOR &>;
};

} // namespace corosig

#endif
