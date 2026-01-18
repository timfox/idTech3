/*
===============================================================================

Modern Unit Testing Framework for id Tech 3

Provides comprehensive testing capabilities with modern C++ features.

===============================================================================
*/

#pragma once

#include "q_shared.h"
#include "error_handling.h"
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <chrono>
#include <source_location>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <sstream>

//===============================================================================
// Test Framework Core
//===============================================================================

// Test result enumeration
enum class TestResult {
    Pass,
    Fail,
    Skip,
    Error
};

// Test case information
struct TestCase {
    std::string name;
    std::string suite;
    std::string description;
    std::function<void()> testFunction;
    std::source_location location;
    std::chrono::microseconds duration{0};
    TestResult result{TestResult::Skip};
    std::string failureMessage;
    std::string errorMessage;

    TestCase(std::string_view n, std::string_view s, std::string_view desc,
             std::function<void()> func, std::source_location loc = std::source_location::current())
        : name(n), suite(s), description(desc), testFunction(std::move(func)), location(loc) {}
};

// Test suite base class
class TestSuite {
public:
    explicit TestSuite(std::string_view name) : m_name(name) {}
    virtual ~TestSuite() = default;

    virtual void setup() {}    // Called before each test
    virtual void teardown() {} // Called after each test

    const std::string& name() const { return m_name; }

    // Register test cases (to be called in constructor)
    void add_test(std::string_view name, std::string_view description, std::function<void()> testFunc) {
        m_testCases.emplace_back(name, m_name, description, std::move(testFunc));
    }

    const std::vector<TestCase>& test_cases() const { return m_testCases; }

private:
    std::string m_name;
    std::vector<TestCase> m_testCases;
};

// Test runner class
class TestRunner {
public:
    static TestRunner& instance();

    // Suite management
    void add_suite(std::unique_ptr<TestSuite> suite);
    void add_suite(std::string_view name);

    // Test execution
    bool run_all_tests();
    bool run_suite(std::string_view suite_name);
    bool run_test(std::string_view suite_name, std::string_view test_name);

    // Results
    struct TestStats {
        size_t totalTests{0};
        size_t passedTests{0};
        size_t failedTests{0};
        size_t skippedTests{0};
        size_t errorTests{0};
        std::chrono::milliseconds totalDuration{0};
    };

    const TestStats& stats() const { return m_stats; }
    const std::vector<TestCase>& results() const { return m_results; }

    // Configuration
    void set_verbose(bool verbose) { m_verbose = verbose; }
    void set_fail_fast(bool failFast) { m_failFast = failFast; }
    void set_filter(std::string_view filter) { m_filter = filter; }

    // Output
    void print_summary() const;
    void print_detailed_results() const;
    void export_results(const std::string& filename) const;

private:
    TestRunner() = default;
    ~TestRunner() = default;

    bool run_test_case(TestCase& testCase);
    bool matches_filter(const TestCase& testCase) const;

    std::vector<std::unique_ptr<TestSuite>> m_suites;
    std::vector<TestCase> m_results;
    TestStats m_stats;
    bool m_verbose{false};
    bool m_failFast{false};
    std::string m_filter;
};

//===============================================================================
// Test Macros and Assertions
//===============================================================================

// Test registration macros
#define TEST_SUITE(name) \
    class name##TestSuite : public TestSuite { \
    public: \
        name##TestSuite() : TestSuite(#name) { register_tests(); } \
        void register_tests(); \
    }; \
    static name##TestSuite* name##_suite_instance = []() { \
        static std::unique_ptr<name##TestSuite> instance = std::make_unique<name##TestSuite>(); \
        TestRunner::instance().add_suite(std::move(instance)); \
        return nullptr; \
    }(); \
    void name##TestSuite::register_tests()

#define TEST(name, description) \
    add_test(#name, description, [this]() { this->test_##name(); }); \
    void test_##name()

#define TEST_F(suite, name, description) \
    add_test(#name, description, [this]() { this->test_##name(); }); \
    void test_##name()

// Assertion macros
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error(std::string("ASSERT_TRUE failed: ") + #condition + \
                                   " at " + std::string(std::source_location::current().file_name()) + ":" + \
                                   std::to_string(std::source_location::current().line())); \
        } \
    } while (0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::ostringstream oss; \
            oss << "ASSERT_EQ failed: expected " << (expected) << ", got " << (actual) \
                << " at " << std::source_location::current().file_name() << ":" \
                << std::source_location::current().line(); \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_NE(expected, actual) ASSERT_TRUE((expected) != (actual))

#define ASSERT_LT(left, right) ASSERT_TRUE((left) < (right))
#define ASSERT_LE(left, right) ASSERT_TRUE((left) <= (right))
#define ASSERT_GT(left, right) ASSERT_TRUE((left) > (right))
#define ASSERT_GE(left, right) ASSERT_TRUE((left) >= (right))

#define ASSERT_STREQ(expected, actual) \
    do { \
        if (std::strcmp((expected), (actual)) != 0) { \
            std::ostringstream oss; \
            oss << "ASSERT_STREQ failed: expected \"" << (expected) \
                << "\", got \"" << (actual) << "\""; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_STRNE(expected, actual) ASSERT_TRUE(std::strcmp((expected), (actual)) != 0)

#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == nullptr)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != nullptr)

#define ASSERT_THROW(expression, exception_type) \
    do { \
        bool threw = false; \
        try { \
            expression; \
        } catch (const exception_type&) { \
            threw = true; \
        } catch (...) { \
            throw std::runtime_error("ASSERT_THROW failed: wrong exception type thrown"); \
        } \
        if (!threw) { \
            throw std::runtime_error("ASSERT_THROW failed: no exception thrown"); \
        } \
    } while (0)

#define ASSERT_NO_THROW(expression) \
    do { \
        try { \
            expression; \
        } catch (const std::exception& e) { \
            std::ostringstream oss; \
            oss << "ASSERT_NO_THROW failed: " << e.what(); \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        auto diff = std::abs((expected) - (actual)); \
        if (diff > (tolerance)) { \
            std::ostringstream oss; \
            oss << "ASSERT_NEAR failed: expected " << (expected) << ", got " << (actual) \
                << " (difference: " << diff << ", tolerance: " << (tolerance) << ")"; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

//===============================================================================
// Test Fixtures and Utilities
//===============================================================================

// Base test fixture with setup/teardown
class TestFixture {
public:
    virtual ~TestFixture() = default;

    virtual void SetUp() {}    // Called before each test
    virtual void TearDown() {} // Called after each test

protected:
    // Utility functions for tests
    static std::string temp_file_path(const std::string& filename) {
        return std::string("/tmp/idtech3_test_") + filename;
    }

    static bool create_temp_file(const std::string& filename, const std::string& content) {
        FILE* file = std::fopen(temp_file_path(filename).c_str(), "w");
        if (!file) return false;
        std::fwrite(content.c_str(), 1, content.size(), file);
        std::fclose(file);
        return true;
    }

    static bool remove_temp_file(const std::string& filename) {
        return std::remove(temp_file_path(filename).c_str()) == 0;
    }

    static std::string read_temp_file(const std::string& filename) {
        FILE* file = std::fopen(temp_file_path(filename).c_str(), "r");
        if (!file) return "";

        std::string content;
        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
            content.append(buffer, bytesRead);
        }
        std::fclose(file);
        return content;
    }
};

// Performance test utilities
class PerformanceTest {
public:
    static std::chrono::microseconds measure_execution_time(std::function<void()> func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    }

    static void assert_performance(std::function<void()> func,
                                 std::chrono::microseconds maxTime,
                                 std::string_view operation = "") {
        auto duration = measure_execution_time(func);
        if (duration > maxTime) {
            std::ostringstream oss;
            oss << "Performance test failed: " << operation
                << " took " << duration.count() << "µs (max allowed: " << maxTime.count() << "µs)";
            throw std::runtime_error(oss.str());
        }
    }
};

// Mock objects for testing
template<typename T>
class Mock {
public:
    Mock() = default;
    virtual ~Mock() = default;

    // Call tracking
    struct CallInfo {
        std::string methodName;
        std::vector<std::string> args;
        std::chrono::system_clock::time_point timestamp;
    };

    void record_call(std::string_view method, std::vector<std::string> args = {}) {
        m_calls.push_back({std::string(method), std::move(args), std::chrono::system_clock::now()});
    }

    const std::vector<CallInfo>& calls() const { return m_calls; }

    void clear_calls() { m_calls.clear(); }

    size_t call_count(std::string_view method) const {
        return std::count_if(m_calls.begin(), m_calls.end(),
                           [&](const CallInfo& call) { return call.methodName == method; });
    }

    bool was_called(std::string_view method) const {
        return call_count(method) > 0;
    }

private:
    std::vector<CallInfo> m_calls;
};

//===============================================================================
// Integration with Legacy Code
//===============================================================================

// Console command for running tests
extern "C" {
void Test_RunUnitTests_f(void);
void Test_RunSuite_f(void);
void Test_RunTest_f(void);
void Test_ListSuites_f(void);
void Test_PrintResults_f(void);
}

// Automatic test registration
#define REGISTER_TEST_SUITE(suite) \
    static int suite##_register_dummy = []() { \
        TestRunner::instance().add_suite(std::make_unique<suite##TestSuite>()); \
        return 0; \
    }()

//===============================================================================
// Example Test Suite
//===============================================================================

/*
// Example of how to create a test suite:

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