#include "corosig/io/File.hpp"

#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/reactor/Reactor.hpp"
#include "corosig/testing/Signals.hpp"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <span>
#include <string_view>

using namespace corosig;

namespace {

char const *g_test_file{nullptr};

std::filesystem::path tmp_file() {
  auto tmp_dir = std::filesystem::temp_directory_path();
  std::array<char, 11> filename{};
  do {
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<char> distr{'a', 'z'};
    auto rand_letter = [&] { return distr(gen); };
    std::ranges::generate_n(filename.begin(), std::size(filename) - 1, rand_letter);
  } while (std::filesystem::exists(tmp_dir / std::string_view{filename.data(), filename.size()}));
  return tmp_dir / std::string_view{filename.data(), filename.size()};
}

struct TestListener : Catch::EventListenerBase {
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const &) override {
    file = tmp_file();
    g_test_file = file.c_str();
  }
  void testCaseEnded(Catch::TestCaseStats const &) override {
    std::filesystem::remove(file);
    g_test_file = nullptr;
  }

  std::filesystem::path file;
};

CATCH_REGISTER_LISTENER(TestListener);

} // namespace

COROSIG_SIGHANDLER_TEST_CASE("Opening a non-existent file without CREATE fails") {
  auto res = File::open(reactor, g_test_file, File::OpenFlags::RDONLY).block_on();
  COROSIG_REQUIRE(!res.is_ok());
  COROSIG_REQUIRE(res.error().holds<SyscallError>());
}

TEST_CASE("Create and write to a file") {
  constexpr static std::string_view CONTENT = "hello world\n";

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      COROSIG_CO_TRY(
          auto file,
          co_await File::open(r, g_test_file, File::OpenFlags::CREATE | File::OpenFlags::WRONLY));

      auto wres = co_await file.write(r, CONTENT);
      COROSIG_REQUIRE(wres.is_ok());
      COROSIG_REQUIRE(wres.value() == CONTENT.size());
      co_return success();
    };

    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });

  std::ifstream ifs(g_test_file);
  std::string file_contents;
  std::getline(ifs, file_contents);
  file_contents += '\n';
  REQUIRE(file_contents == CONTENT);
}

TEST_CASE("Read file contents") {
  {
    std::ofstream ofs(g_test_file);
    ofs << "abc123";
  }

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      COROSIG_CO_TRY(auto file, co_await File::open(r, g_test_file, File::OpenFlags::RDONLY));

      std::array<char, 16> buf;
      COROSIG_CO_TRY(size_t read, co_await file.read(r, buf));
      COROSIG_REQUIRE(read == 6u);
      COROSIG_REQUIRE(std::string_view{buf.begin(), read} == "abc123");
      co_return success();
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });
}

TEST_CASE("Append mode writes at end of file") {
  {
    std::ofstream ofs(g_test_file);
    ofs << "start";
  }

  run_in_sighandler([](Reactor &reactor) {
    auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
      COROSIG_CO_TRY(
          auto file,
          co_await File::open(r, g_test_file, File::OpenFlags::APPEND | File::OpenFlags::WRONLY));
      constexpr std::string_view EXTRA = "end";
      COROSIG_CO_TRY(size_t written, co_await file.write(r, EXTRA));
      COROSIG_REQUIRE(written == EXTRA.size());
      co_return success();
    };
    COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
  });

  std::ifstream ifs(g_test_file);
  std::string file_contents;
  std::getline(ifs, file_contents);
  REQUIRE(file_contents == "startend");
}

COROSIG_SIGHANDLER_TEST_CASE("Move semantics") {
  auto foo = [](Reactor &r) -> Fut<void, Error<AllocationError, SyscallError>> {
    COROSIG_CO_TRY(File f1, co_await File::open(r, g_test_file,
                                                File::OpenFlags::CREATE | File::OpenFlags::WRONLY));
    int fd_before = f1.underlying_handle();
    File f2 = std::move(f1);
    CHECK(f1.underlying_handle() == -1);
    CHECK(f2.underlying_handle() == fd_before);
    co_return success();
  };
  COROSIG_REQUIRE(foo(reactor).block_on().is_ok());
}
