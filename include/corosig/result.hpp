#pragma once

#include "boost/outcome.hpp"
#include "boost/outcome/success_failure.hpp"
#include "corosig/error_types.hpp"
#include <concepts>

namespace corosig {

// Even simplest coroutines may fail due to allocation error. This is why any error type must be
// able to contain it
template <typename T>
concept AnError = std::constructible_from<T, AllocationError>;

template <typename R, AnError E>
using Result = boost::outcome_v2::result<R, E, boost::outcome_v2::policy::terminate>;

using boost::outcome_v2::failure;
using boost::outcome_v2::success;

} // namespace corosig
