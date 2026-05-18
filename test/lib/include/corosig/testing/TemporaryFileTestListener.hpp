#ifndef COROSIG_TESTING_TEMP_FILE_TEST_LISTENER_HPP
#define COROSIG_TESTING_TEMP_FILE_TEST_LISTENER_HPP

#include "corosig/testing/FileHelpers.hpp"

#include <catch2/catch_all.hpp>
#include <filesystem>

namespace corosig::testing {

inline char const *g_temp_test_file = nullptr;

struct TemporaryFileTestListener : Catch::EventListenerBase {
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const &) override {
    file = tmp_file();
    g_temp_test_file = file.c_str();
  }

  void testCaseEnded(Catch::TestCaseStats const &) override {
    std::filesystem::remove(file);
    g_temp_test_file = nullptr;
  }

  std::filesystem::path file;
};

inline void write_temp_file(std::string_view content) {
  return write_file(g_temp_test_file, content);
}

} // namespace corosig::testing

#endif
