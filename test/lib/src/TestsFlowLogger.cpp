#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <iostream>

namespace {

struct TestsFlowLogger : Catch::EventListenerBase {
  using EventListenerBase::EventListenerBase;

  void testCasePartialStarting(Catch::TestCaseInfo const &test_info,
                               uint64_t part_number) override {
    std::cout << test_info.name << ". Section #" << part_number << " started\n";
    std::cout.flush();
  }

  void testCasePartialEnded(Catch::TestCaseStats const &test_case_stats,
                            uint64_t part_number) override {
    std::cout << test_case_stats.testInfo->name << ". Section #" << part_number << " ended\n";
    std::cout.flush();
  }
};

CATCH_REGISTER_LISTENER(TestsFlowLogger)

} // namespace
