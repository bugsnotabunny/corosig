#include "corosig/testing/TemporaryFileTestListener.hpp"

#include "corosig/testing/FileHelpers.hpp"


namespace corosig::testing {

void write_temp_file(std::string_view content) {
  return write_file(g_temp_test_file, content);
}

void TemporaryFileTestListener::testCaseStarting(Catch::TestCaseInfo const &) {
  file = tmp_file();
  g_temp_test_file = file.c_str();
}

void TemporaryFileTestListener::testCaseEnded(Catch::TestCaseStats const &) {
  std::filesystem::remove(file);
  g_temp_test_file = nullptr;
}

} // namespace corosig::testing
