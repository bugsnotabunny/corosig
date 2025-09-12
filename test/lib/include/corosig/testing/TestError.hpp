#pragma once

#include <catch2/catch.hpp>
#include <charconv>
#include <concepts>
#include <csignal>
#include <cstddef>
#include <optional>
#include <source_location>
#include <unistd.h>
#include <utility>

namespace corosig {

struct TestError {};

} // namespace corosig
