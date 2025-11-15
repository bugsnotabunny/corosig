#include "corosig/io/Pipe.hpp"

#include "corosig/testing/Signals.hpp"

#include <boost/outcome/try.hpp>
#include <catch2/catch_all.hpp>

using namespace corosig;

COROSIG_SIGHANDLER_TEST_CASE("PipePair::make creates valid pair", "[PipePair]") {
  auto pair = PipePair::make();

  COROSIG_REQUIRE(pair.has_value());
  COROSIG_REQUIRE(pair.assume_value().read.underlying_handle() != -1);
  COROSIG_REQUIRE(pair.assume_value().write.underlying_handle() != -1);
}

COROSIG_SIGHANDLER_TEST_CASE("PipePair read/write roundtrip") {
  auto foo = [](Reactor &r) -> Fut<int, Error<AllocationError, SyscallError>> {
    BOOST_OUTCOME_CO_TRY(auto pipes, PipePair::make());

    constexpr std::string_view MSG = "hello world!";
    BOOST_OUTCOME_CO_TRY(size_t written, co_await pipes.write.write(r, MSG));
    COROSIG_REQUIRE(written == MSG.size());

    std::array<char, MSG.size()> buf;
    BOOST_OUTCOME_CO_TRY(size_t read, co_await pipes.read.read(r, buf));
    COROSIG_REQUIRE(std::string_view{buf.begin(), read} == MSG);

    co_return 10;
  };

  auto res = foo(reactor).block_on();
  COROSIG_REQUIRE(res);
  COROSIG_REQUIRE(res.value() == 10);
}

COROSIG_SIGHANDLER_TEST_CASE("PipeRead and PipeWrite move semantics", "[Pipe]") {
  auto pair_result = PipePair::make();
  REQUIRE(pair_result.has_value());
  auto pair = std::move(pair_result).assume_value();

  int original_fd_r = pair.read.underlying_handle();
  int original_fd_w = pair.write.underlying_handle();

  PipeRead moved_read(std::move(pair.read));
  PipeWrite moved_write(std::move(pair.write));

  COROSIG_REQUIRE(pair.read.underlying_handle() == -1);
  COROSIG_REQUIRE(pair.write.underlying_handle() == -1);

  COROSIG_REQUIRE(moved_read.underlying_handle() == original_fd_r);
  COROSIG_REQUIRE(moved_write.underlying_handle() == original_fd_w);
}

COROSIG_SIGHANDLER_TEST_CASE("Pipe close() invalidates descriptor", "[Pipe]") {
  auto pair_result = PipePair::make();
  COROSIG_REQUIRE(pair_result.has_value());
  auto pair = std::move(pair_result).assume_value();

  pair.read.close();
  COROSIG_REQUIRE(pair.read.underlying_handle() == -1);

  pair.write.close();
  COROSIG_REQUIRE(pair.write.underlying_handle() == -1);
}
