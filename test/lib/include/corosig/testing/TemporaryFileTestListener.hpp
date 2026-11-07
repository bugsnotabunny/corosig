#ifndef COROSIG_TESTING_TEMP_FILE_TEST_LISTENER_HPP
#define COROSIG_TESTING_TEMP_FILE_TEST_LISTENER_HPP

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <filesystem>

namespace corosig::testing {
extern char const *g_temp_test_file;

struct TemporaryFileTestListener : Catch::EventListenerBase {
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const &) override;
  void testCaseEnded(Catch::TestCaseStats const &) override;

  std::filesystem::path file;
};

void write_temp_file(std::string_view content);

} // namespace corosig::testing

#endif
