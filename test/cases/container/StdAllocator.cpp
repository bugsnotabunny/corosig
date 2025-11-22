#include "corosig/container/StdAllocator.hpp"

#include <catch2/catch_all.hpp>
#include <cstddef>
#include <new>
#include <string>
#include <vector>

using namespace corosig;

struct DummyAlloc {
  size_t allocate_call_count = 0;
  size_t deallocate_call_count = 0;
  size_t alloc_n = 0;

  void *allocate(size_t n, size_t align) noexcept;

  void *allocate(size_t n) noexcept {
    allocate_call_count++;
    alloc_n += n;
    return ::operator new(n * sizeof(std::max_align_t));
  }

  void deallocate(void *p) noexcept {
    deallocate_call_count++;
    ::operator delete(p);
  }
};

TEST_CASE("StdAllocator allocates and deallocates properly", "[StdAllocator]") {
  DummyAlloc da;
  StdAllocator<int, DummyAlloc &> alloc((da));

  int *p = alloc.allocate(1);
  REQUIRE(p != nullptr);

  alloc.deallocate(p, 1);

  REQUIRE(da.allocate_call_count == 1);
  REQUIRE(da.deallocate_call_count == 1);
}

TEST_CASE("StdAllocator works with non-POD/complex types", "[StdAllocator]") {
  DummyAlloc tracker;
  StdAllocator<std::string, DummyAlloc &> alloc(tracker);

  std::string *p = alloc.allocate(3);
  REQUIRE(p != nullptr);
  REQUIRE(tracker.alloc_n == 3);

  alloc.deallocate(p, 3);
}

TEST_CASE("StdAllocator works inside std::vector", "[StdAllocator]") {
  DummyAlloc tracker;
  using StdAllocator = StdAllocator<int, DummyAlloc &>;
  StdAllocator alloc(tracker);

  std::vector<int, StdAllocator> vec(alloc);
  vec.push_back(1);
  vec.push_back(2);
  vec.push_back(3);

  REQUIRE(vec.size() == 3);
}
