#include "corosig/container/StdAllocator.hpp"

#include "corosig/testing/MockAllocator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

namespace {

using namespace corosig;
using namespace corosig::testing;

} // namespace

TEST_CASE("StdAllocator allocates and deallocates properly", "[StdAllocator]") {
  MockAllocator ma;
  StdAllocator<int, MockAllocator &> alloc((ma));

  int *p = alloc.allocate(1);
  REQUIRE(p != nullptr);

  alloc.deallocate(p, 1);

  REQUIRE(ma.allocate_calls == 1);
  REQUIRE(ma.deallocate_calls == 1);
}

TEST_CASE("StdAllocator works with non-POD/complex types", "[StdAllocator]") {
  MockAllocator tracker;
  StdAllocator<std::string, MockAllocator &> alloc(tracker);

  std::string *p = alloc.allocate(3);
  REQUIRE(p != nullptr);
  REQUIRE(tracker.allocated_bytes == 3 * sizeof(std::string));

  alloc.deallocate(p, 3);
}

TEST_CASE("StdAllocator works inside std::vector", "[StdAllocator]") {
  MockAllocator tracker;
  using StdAllocator = StdAllocator<int, MockAllocator &>;
  StdAllocator alloc{tracker};

  std::vector<int, StdAllocator> vec{alloc};
  vec.push_back(1);
  vec.push_back(2);
  vec.push_back(3);

  REQUIRE(vec.size() == 3);
}
