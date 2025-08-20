#include <algorithm>
#include <catch2/catch_message.hpp>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <corosig/static_buf_allocator.hpp>
#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

using namespace corosig;

std::byte *test_allocate(auto &alloc, size_t size, size_t align) {
  CAPTURE(alloc.view_mem(), size, align);
  std::byte *ptr = alloc.allocate(size, align);
  REQUIRE(ptr != nullptr);
  REQUIRE(size_t(ptr) % align == 0);
  std::memset(ptr, 255, size);
  return ptr;
}

void test_allocate_fail(auto &alloc, size_t size, size_t align) {
  CAPTURE(alloc.view_mem(), size, align);
  std::byte *ptr = alloc.allocate(size, align);
  REQUIRE(ptr == nullptr);
}

// TEST_CASE("Allocate and deallocate a small block") {
//   auto alloc = static_buf_allocator<256>::zeroed();
//   auto ptr = test_allocate(alloc, 16, 16);
//   alloc.deallocate(ptr);

//   // Verify we can allocate again
//   ptr = test_allocate(alloc, 16, 16);
//   alloc.deallocate(ptr);
// }

TEST_CASE("allocator") {
  auto alloc = static_buf_allocator<256>::zeroed();

  test_allocate(alloc, 32, 1);
  test_allocate(alloc, 16, 8);
  test_allocate(alloc, 40, 4);
  test_allocate(alloc, 12, 16);

  test_allocate_fail(alloc, 1234, 1);

  CAPTURE(alloc.view_mem());
  REQUIRE(1 == 2);
}

int main(int argc, char *argv[]) {
  return Catch::Session().run(argc, argv);
}
