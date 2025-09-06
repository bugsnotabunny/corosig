#include "corosig/io/File.hpp"
#include "boost/outcome/try.hpp"
#include "corosig/ErrorTypes.hpp"
#include "corosig/Result.hpp"
#include "corosig/testing/Signals.hpp"

#include <algorithm>
#include <catch2/catch.hpp>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <span>

using namespace corosig;

namespace {

char const *g_test_file{nullptr};

std::filesystem::path tmp_file() {
  auto tmp_dir = std::filesystem::temp_directory_path();
  char filename[11]{};
  do {
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<char> distr{'a', 'z'};
    auto rand_letter = [&] { return distr(gen); };
    std::ranges::generate_n(filename, std::size(filename) - 1, rand_letter);
  } while (std::filesystem::exists(tmp_dir / filename));
  return tmp_dir / filename;
}

struct TestFixture {
  TestFixture() {
    g_test_file = file.c_str();
  }

  ~TestFixture() {
    std::filesystem::remove(file);
    g_test_file = nullptr;
  }

  std::filesystem::path file = tmp_file();
};

} // namespace

TEST_CASE("Opening a non-existent file without CREATE fails") {
  TestFixture _;
  test_in_sighandler([] {
    auto res = File::open(g_test_file, File::OpenFlags::RDONLY).block_on();
    COROSIG_REQUIRE(res.has_error());
    COROSIG_REQUIRE(res.error().holds<SyscallError>());
  });
}

TEST_CASE("Create and write to a file") {
  TestFixture _;
  constexpr static std::string_view content = "hello world\n";

  test_in_sighandler([] {
    auto foo = []() -> Fut<void, Error<AllocationError, SyscallError>> {
      BOOST_OUTCOME_CO_TRY(
          auto file,
          co_await File::open(g_test_file, File::OpenFlags::CREATE | File::OpenFlags::WRONLY));

      auto wres = co_await file.write(content);
      COROSIG_REQUIRE(wres.has_value());
      COROSIG_REQUIRE(wres.assume_value() == content.size());
      co_return success();
    };

    COROSIG_REQUIRE(foo().block_on().has_value());
  });

  std::ifstream ifs(g_test_file);
  std::string file_contents;
  std::getline(ifs, file_contents);
  file_contents += '\n';
  REQUIRE(file_contents == content);
}

TEST_CASE("Read file contents") {
  TestFixture _;

  {
    std::ofstream ofs(g_test_file);
    ofs << "abc123";
  }

  test_in_sighandler([] {
    auto foo = []() -> Fut<void, Error<AllocationError, SyscallError>> {
      BOOST_OUTCOME_CO_TRY(auto file, co_await File::open(g_test_file, File::OpenFlags::RDONLY));

      char buf[16];
      BOOST_OUTCOME_CO_TRY(size_t rres, co_await file.read(buf));
      COROSIG_REQUIRE(rres == 6u);
      COROSIG_REQUIRE(std::string_view{buf, rres} == "abc123");
      co_return success();
    };
    COROSIG_REQUIRE(foo().block_on().has_value());
  });
}

TEST_CASE("Append mode writes at end of file") {
  TestFixture _;

  {
    std::ofstream ofs(g_test_file);
    ofs << "start";
  }

  test_in_sighandler([] {
    auto foo = []() -> Fut<void, Error<AllocationError, SyscallError>> {
      BOOST_OUTCOME_CO_TRY(
          auto file,
          co_await File::open(g_test_file, File::OpenFlags::APPEND | File::OpenFlags::WRONLY));
      constexpr std::string_view extra = "end";
      BOOST_OUTCOME_CO_TRY(size_t written, co_await file.write(extra));
      COROSIG_REQUIRE(written == extra.size());
      co_return success();
    };
    COROSIG_REQUIRE(foo().block_on().has_value());
  });

  std::ifstream ifs(g_test_file);
  std::string file_contents;
  std::getline(ifs, file_contents);
  REQUIRE(file_contents == "startend");
}

TEST_CASE("Move semantics") {
  TestFixture _;

  test_in_sighandler([] {
    auto foo = []() -> Fut<void, Error<AllocationError, SyscallError>> {
      BOOST_OUTCOME_CO_TRY(File f1, co_await File::open(g_test_file, File::OpenFlags::CREATE |
                                                                         File::OpenFlags::WRONLY));
      int fd_before = f1.underlying_handle();
      File f2 = std::move(f1);
      CHECK(f1.underlying_handle() == -1);
      CHECK(f2.underlying_handle() == fd_before);
      co_return success();
    };
    COROSIG_REQUIRE(foo().block_on().has_value());
  });
}
