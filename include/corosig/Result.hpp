#pragma once

#include <boost/outcome.hpp>
#include <boost/outcome/success_failure.hpp>
#include <concepts>

namespace corosig {

template <typename R, typename E>
using Result = boost::outcome_v2::result<R, E, boost::outcome_v2::policy::terminate>;

using boost::outcome_v2::failure;
using boost::outcome_v2::success;

} // namespace corosig
