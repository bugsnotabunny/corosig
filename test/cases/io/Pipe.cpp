#include "corosig/io/Pipe.hpp"

#include "corosig/testing/Signals.hpp"

#include <boost/outcome/try.hpp>
#include <catch2/catch.hpp>

using namespace corosig;

TEST_CASE("PipePair::make creates valid pair", "[PipePair]") {
  auto pair = PipePair::make();

  COROSIG_REQUIRE(pair.has_value());
  COROSIG_REQUIRE(pair.assume_value().read.underlying_handle() != -1);
  COROSIG_REQUIRE(pair.assume_value().write.underlying_handle() != -1);
}

TEST_CASE("PipePair read/write roundtrip") {
  test_in_sighandler([] {
    auto foo = []() -> Fut<int, Error<AllocationError, SyscallError>> {
      BOOST_OUTCOME_CO_TRY(auto pipes, PipePair::make());

      constexpr std::string_view msg = "hello world!";
      BOOST_OUTCOME_CO_TRY(size_t written, co_await pipes.write.write(msg));
      COROSIG_REQUIRE(written == msg.size());

      char buf[msg.size()];
      BOOST_OUTCOME_CO_TRY(size_t read, co_await pipes.read.read(buf));
      COROSIG_REQUIRE(std::string_view{buf, read} == msg);

      co_return 10;
    };

    auto res = foo().block_on();
    COROSIG_REQUIRE(res);
    COROSIG_REQUIRE(res.value() == 10);
  });
}

TEST_CASE("PipeRead and PipeWrite move semantics", "[Pipe]") {
  test_in_sighandler([] {
    auto pairResult = PipePair::make();
    REQUIRE(pairResult.has_value());
    auto pair = std::move(pairResult).assume_value();

    int original_fd_r = pair.read.underlying_handle();
    int original_fd_w = pair.write.underlying_handle();

    PipeRead movedRead(std::move(pair.read));
    PipeWrite movedWrite(std::move(pair.write));

    COROSIG_REQUIRE(pair.read.underlying_handle() == -1);
    COROSIG_REQUIRE(pair.write.underlying_handle() == -1);

    COROSIG_REQUIRE(movedRead.underlying_handle() == original_fd_r);
    COROSIG_REQUIRE(movedWrite.underlying_handle() == original_fd_w);
  });
}

TEST_CASE("Pipe close() invalidates descriptor", "[Pipe]") {
  test_in_sighandler([] {
    auto pairResult = PipePair::make();
    COROSIG_REQUIRE(pairResult.has_value());
    auto pair = std::move(pairResult).assume_value();

    pair.read.close();
    COROSIG_REQUIRE(pair.read.underlying_handle() == -1);

    pair.write.close();
    COROSIG_REQUIRE(pair.write.underlying_handle() == -1);
  });
}
