#include "corosig/util/Variant.hpp"

#include "corosig/testing/Signals.hpp"

namespace {

using namespace corosig;

static_assert(std::convertible_to<Variant<int, char>, Variant<int, char, double>>);
static_assert(!std::convertible_to<Variant<int, double, char>, Variant<int, double>>);
static_assert(!std::convertible_to<Variant<int, double>, Variant<int, char>>);

} // namespace

COROSIG_SIGHANDLER_TEST_CASE(
    "Variant shuffled-order converting constructor - Basic type shuffling") {
  Variant<int, double, char> v1{42};
  Variant<char, int, double> v2{v1};
  COROSIG_REQUIRE(v2 == 42);
}

COROSIG_SIGHANDLER_TEST_CASE(
    "Variant shuffled-order converting constructor - Different active type after shuffle") {
  Variant<int, double, char> v1{3.14};
  Variant<char, int, double> v2{v1};

  COROSIG_REQUIRE(v2.holds<double>());
  COROSIG_REQUIRE(v2.as<double>() == Catch::Approx(3.14));
}

COROSIG_SIGHANDLER_TEST_CASE(
    "Variant shuffled-order converting constructor - Another active type variation") {
  Variant<int, double, char> v1{'x'};
  Variant<char, int, double> v2{v1};
  COROSIG_REQUIRE(v2 == 'x');
}

// A regular test case since std strings are a priori non sigsafe
TEST_CASE("Variant shuffled conversion with non-trivial types - String type") {
  Variant<int, std::string, double> v1{std::string("Hello")};
  Variant<double, int, std::string> v2{std::move(v1)};
  REQUIRE(v2 == std::string{"Hello"});
}

COROSIG_SIGHANDLER_TEST_CASE(
    "Variant shuffled conversion with non-trivial types - Custom move-only type") {
  struct MoveOnly {
    MoveOnly(int v = 0)
        : value{v} {
    }

    MoveOnly(const MoveOnly &) = delete;
    MoveOnly(MoveOnly &&) noexcept = default;
    MoveOnly &operator=(const MoveOnly &) = delete;
    MoveOnly &operator=(MoveOnly &&) noexcept = default;
    ~MoveOnly() = default;

    constexpr auto operator<=>(MoveOnly const &) const noexcept = default;

    int value;
  };

  Variant<int, MoveOnly, double> v1{MoveOnly{42}};
  Variant<double, int, MoveOnly> v2{std::move(v1)};
  REQUIRE(v2 == MoveOnly{42});
}
