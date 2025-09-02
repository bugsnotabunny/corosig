#pragma once

#include "boost/outcome.hpp"
#include "corosig/error_types.hpp"
#include <concepts>

namespace corosig {

// Even simplest coroutines may fail due to allocation error. This is why any error type must be
// able to contain it
template <typename T>
concept AnError = std::constructible_from<T, AllocationError>;

template <typename R, AnError E>
using Result = boost::outcome_v2::result<R, E, boost::outcome_v2::policy::terminate>;

} // namespace corosig
