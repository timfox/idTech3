/*
===============================================================================

Modern Unit Testing Framework Implementation

===============================================================================
*/

#include "unit_test.h"
#include "qcommon.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>

//===============================================================================
// TestRunner Implementation
//===============================================================================

TestRunner& TestRunner::instance()
{
    static TestRunner instance;
    return instance;
}

void TestRunner::add_suite(std::unique_ptr<TestSuite> suite)
{
    m_suites.push_back(std::move(suite));
}

void TestRunner::add_suite(std::string_view name)
{
    // For suites that don't need special setup
    auto suite = std::make_unique<TestSuite>(name);
    m_suites.push_back(std::move(suite));
}

bool TestRunner::run_all_tests()
{
    m_results.clear();
    m_stats = TestStats{};
    auto startTime = std::chrono::high_resolution_clock::now();

    Com_Printf("Running all unit tests...\n");

    bool allPassed = true;

    for (auto& suite : m_suites) {
        if (!run_suite(suite->name())) {
            allPassed = false;
            if (m_failFast) break;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    print_summary();
    return allPassed;
}

bool TestRunner::run_suite(std::string_view suite_name)
{
    auto it = std::find_if(m_suites.begin(), m_suites.end(),
                          [&](const std::unique_ptr<TestSuite>& suite) {
                              return suite->name() == suite_name;
                          });

    if (it == m_suites.end()) {
        Com_Printf("Test suite '%s' not found\n", std::string(suite_name).c_str());
        return false;
    }

    Com_Printf("Running test suite: %s\n", std::string(suite_name).c_str());

    bool suitePassed = true;
    TestSuite* suite = it->get();

    for (auto& testCase : suite->test_cases()) {
        if (!matches_filter(testCase)) continue;

        m_stats.totalTests++;

        if (m_verbose) {
            Com_Printf("  Running: %s::%s\n", testCase.suite.c_str(), testCase.name.c_str());
        }

        try {
            suite->setup();
            bool passed = run_test_case(testCase);
            suite->teardown();

            if (passed) {
                m_stats.passedTests++;
            } else {
                m_stats.failedTests++;
                suitePassed = false;
                if (m_failFast) break;
            }
        } catch (const std::exception& e) {
            testCase.result = TestResult::Error;
            testCase.errorMessage = e.what();
            m_stats.errorTests++;
            suitePassed = false;

            Com_Printf("  ERROR: %s::%s - %s\n",
                      testCase.suite.c_str(), testCase.name.c_str(), e.what());

            if (m_failFast) break;
        }
    }

    return suitePassed;
}

bool TestRunner::run_test(std::string_view suite_name, std::string_view test_name)
{
    auto it = std::find_if(m_suites.begin(), m_suites.end(),
                          [&](const std::unique_ptr<TestSuite>& suite) {
                              return suite->name() == suite_name;
                          });

    if (it == m_suites.end()) return false;

    TestSuite* suite = it->get();

    auto testIt = std::find_if(suite->test_cases().begin(), suite->test_cases().end(),
                              [&](const TestCase& testCase) {
                                  return testCase.name == test_name;
                              });

    if (testIt == suite->test_cases().end()) return false;

    TestCase testCase = *testIt; // Copy for modification
    m_stats.totalTests++;

    try {
        suite->setup();
        bool passed = run_test_case(testCase);
        suite->teardown();

        if (passed) {
            m_stats.passedTests++;
        } else {
            m_stats.failedTests++;
        }

        m_results.push_back(testCase);
        return passed;
    } catch (const std::exception& e) {
        testCase.result = TestResult::Error;
        testCase.errorMessage = e.what();
        m_stats.errorTests++;
        m_results.push_back(testCase);
        return false;
    }
}

bool TestRunner::run_test_case(TestCase& testCase)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        testCase.testFunction();
        testCase.result = TestResult::Pass;
        testCase.failureMessage.clear();
        testCase.errorMessage.clear();
    } catch (const std::runtime_error& e) {
        testCase.result = TestResult::Fail;
        testCase.failureMessage = e.what();
        testCase.errorMessage.clear();
    } catch (const std::exception& e) {
        testCase.result = TestResult::Error;
        testCase.errorMessage = e.what();
        testCase.failureMessage.clear();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    testCase.duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    m_results.push_back(testCase);

    if (testCase.result != TestResult::Pass) {
        Com_Printf("  FAILED: %s::%s", testCase.suite.c_str(), testCase.name.c_str());
        if (!testCase.failureMessage.empty()) {
            Com_Printf(" - %s", testCase.failureMessage.c_str());
        } else if (!testCase.errorMessage.empty()) {
            Com_Printf(" - ERROR: %s", testCase.errorMessage.c_str());
        }
        Com_Printf("\n");
    } else if (m_verbose) {
        Com_Printf("  PASSED: %s::%s (%lld µs)\n",
                  testCase.suite.c_str(), testCase.name.c_str(),
                  testCase.duration.count());
    }

    return testCase.result == TestResult::Pass;
}

bool TestRunner::matches_filter(const TestCase& testCase) const
{
    if (m_filter.empty()) return true;

    std::string fullName = testCase.suite + "." + testCase.name;
    return fullName.find(m_filter) != std::string::npos;
}

void TestRunner::print_summary() const
{
    Com_Printf("\n=== Test Results Summary ===\n");
    Com_Printf("Total tests: %zu\n", m_stats.totalTests);
    Com_Printf("Passed: %zu\n", m_stats.passedTests);
    Com_Printf("Failed: %zu\n", m_stats.failedTests);
    Com_Printf("Errors: %zu\n", m_stats.errorTests);
    Com_Printf("Skipped: %zu\n", m_stats.skippedTests);
    Com_Printf("Total time: %lld ms\n", m_stats.totalDuration.count());

    if (m_stats.totalTests > 0) {
        double passRate = (double)m_stats.passedTests / m_stats.totalTests * 100.0;
        Com_Printf("Pass rate: %.1f%%\n", passRate);
    }

    if (!m_results.empty()) {
        auto avgDuration = std::accumulate(m_results.begin(), m_results.end(),
                                          std::chrono::microseconds(0),
                                          [](auto sum, const auto& test) { return sum + test.duration; })
                          / m_results.size();
        Com_Printf("Average test time: %lld µs\n", avgDuration.count());
    }
}

void TestRunner::print_detailed_results() const
{
    Com_Printf("\n=== Detailed Test Results ===\n");

    for (const auto& result : m_results) {
        const char* status = "UNKNOWN";
        switch (result.result) {
            case TestResult::Pass: status = "PASS"; break;
            case TestResult::Fail: status = "FAIL"; break;
            case TestResult::Skip: status = "SKIP"; break;
            case TestResult::Error: status = "ERROR"; break;
        }

        Com_Printf("[%s] %s::%s (%lld µs)\n",
                  status, result.suite.c_str(), result.name.c_str(),
                  result.duration.count());

        if (!result.failureMessage.empty()) {
            Com_Printf("  Failure: %s\n", result.failureMessage.c_str());
        }

        if (!result.errorMessage.empty()) {
            Com_Printf("  Error: %s\n", result.errorMessage.c_str());
        }

        if (!result.description.empty()) {
            Com_Printf("  Description: %s\n", result.description.c_str());
        }
    }
}

void TestRunner::export_results(const std::string& filename) const
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        Com_Printf("Failed to open file for export: %s\n", filename.c_str());
        return;
    }

    file << "id Tech 3 Unit Test Results\n";
    file << "===========================\n\n";
    file << "Summary:\n";
    file << "- Total tests: " << m_stats.totalTests << "\n";
    file << "- Passed: " << m_stats.passedTests << "\n";
    file << "- Failed: " << m_stats.failedTests << "\n";
    file << "- Errors: " << m_stats.errorTests << "\n";
    file << "- Skipped: " << m_stats.skippedTests << "\n";
    file << "- Total time: " << m_stats.totalDuration.count() << " ms\n\n";

    file << "Detailed Results:\n";
    file << std::left << std::setw(8) << "Status"
         << std::setw(20) << "Suite"
         << std::setw(20) << "Test"
         << std::setw(12) << "Time (µs)"
         << "Details\n";
    file << std::string(70, '-') << "\n";

    for (const auto& result : m_results) {
        const char* status = "UNKNOWN";
        switch (result.result) {
            case TestResult::Pass: status = "PASS"; break;
            case TestResult::Fail: status = "FAIL"; break;
            case TestResult::Skip: status = "SKIP"; break;
            case TestResult::Error: status = "ERROR"; break;
        }

        file << std::left << std::setw(8) << status
             << std::setw(20) << result.suite
             << std::setw(20) << result.name
             << std::setw(12) << result.duration.count();

        if (!result.failureMessage.empty()) {
            file << result.failureMessage;
        } else if (!result.errorMessage.empty()) {
            file << result.errorMessage;
        }

        file << "\n";
    }

    file.close();
    Com_Printf("Test results exported to: %s\n", filename.c_str());
}

//===============================================================================
// Legacy Integration
//===============================================================================

extern "C" {

void Test_RunUnitTests_f(void)
{
    int argc = Cmd_Argc();
    TestRunner& runner = TestRunner::instance();

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        const char* arg = Cmd_Argv(i);

        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            runner.set_verbose(true);
        } else if (strcmp(arg, "-f") == 0 || strcmp(arg, "--fail-fast") == 0) {
            runner.set_fail_fast(true);
        } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--filter") == 0) {
            if (i + 1 < argc) {
                runner.set_filter(Cmd_Argv(++i));
            }
        }
    }

    bool success = runner.run_all_tests();

    if (!success) {
        Com_Printf("Some tests failed. Use 'test_list' to see available tests.\n");
    }
}

void Test_RunSuite_f(void)
{
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: test_run_suite <suite_name>\n");
        return;
    }

    const char* suiteName = Cmd_Argv(1);
    bool success = TestRunner::instance().run_suite(suiteName);

    if (!success) {
        Com_Printf("Suite '%s' failed or not found.\n", suiteName);
    }
}

void Test_RunTest_f(void)
{
    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: test_run_test <suite_name> <test_name>\n");
        return;
    }

    const char* suiteName = Cmd_Argv(1);
    const char* testName = Cmd_Argv(2);
    bool success = TestRunner::instance().run_test(suiteName, testName);

    if (!success) {
        Com_Printf("Test '%s::%s' failed or not found.\n", suiteName, testName);
    }
}

void Test_ListSuites_f(void)
{
    const auto& suites = TestRunner::instance();
    // Note: This is a simplified version. In practice, we'd need to expose
    // the suites vector through a getter method.

    Com_Printf("Available test suites:\n");
    Com_Printf("(Note: Use test_run_unit_tests to run all tests)\n");
    Com_Printf("Example: test_run_suite Common\n");
}

void Test_PrintResults_f(void)
{
    TestRunner::instance().print_detailed_results();
}

} // extern "C"

//===============================================================================
// Example Test Suite Implementation
//===============================================================================

/*
// This would be in a separate file, but shown here for completeness

TEST_SUITE(Common)
{
    TEST(math_clamp, "Test math clamping functions")
    {
        ASSERT_EQ(5, Com_Clamp(0, 10, 5));
        ASSERT_EQ(0, Com_Clamp(0, 10, -5));
        ASSERT_EQ(10, Com_Clamp(0, 10, 15));
    }

    TEST(string_operations, "Test string manipulation")
    {
        char buffer[64];
        Com_sprintf(buffer, sizeof(buffer), "test %d", 42);
        ASSERT_STREQ("test 42", buffer);
    }

    TEST(performance_critical, "Performance test for critical path")
    {
        PerformanceTest::assert_performance([]() {
            // Simulate critical path operation
            volatile int sum = 0;
            for (int i = 0; i < 1000000; ++i) {
                sum += i;
            }
        }, std::chrono::milliseconds(100), "million iteration loop");
    }
};

REGISTER_TEST_SUITE(Common);
*/