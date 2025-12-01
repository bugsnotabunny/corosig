#ifndef COROSIG_CONCEPT_AN_INSTANCE_OF_HPP
#define COROSIG_CONCEPT_AN_INSTANCE_OF_HPP

#include <type_traits>

namespace corosig {

namespace detail {

template <typename T, template <typename...> typename TEMPLATE>
struct AnInstanceOfImpl : std::false_type {};

template <typename... ARGS, template <typename...> typename TEMPLATE>
struct AnInstanceOfImpl<TEMPLATE<ARGS...>, TEMPLATE> : std::true_type {};

} // namespace detail

/// @brief Tell if T is an instance of TEMPLATE
/// @warning works poorly with alias templates as TEMPLATE argument
template <typename T, template <typename...> typename TEMPLATE>
concept AnInstanceOf = detail::AnInstanceOfImpl<T, TEMPLATE>::value;

} // namespace corosig

#endif
