#ifndef COROSIG_TESTING_TEMP_FILE_TEST_LISTENER_HPP
#define COROSIG_TESTING_TEMP_FILE_TEST_LISTENER_HPP

#include <catch2/catch_all.hpp>
#include <filesystem>

namespace corosig::testing {

inline char const *g_temp_test_file = nullptr;

struct TemporaryFileTestListener : Catch::EventListenerBase {
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const &) override;
  void testCaseEnded(Catch::TestCaseStats const &) override;

  std::filesystem::path file;
};

void write_temp_file(std::string_view content);

} // namespace corosig::testing

#endif
