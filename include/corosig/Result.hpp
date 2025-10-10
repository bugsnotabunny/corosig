#pragma once

#include <boost/outcome.hpp>
#include <boost/outcome/success_failure.hpp>

namespace corosig {

template <typename R, typename E>
struct Result : boost::outcome_v2::result<R, E, boost::outcome_v2::policy::terminate> {
  using boost::outcome_v2::result<R, E, boost::outcome_v2::policy::terminate>::result;
};

using boost::outcome_v2::failure;
using boost::outcome_v2::success;

} // namespace corosig
