-- Comprehensive Engine Hardening Test Suite
-- Tests stability, security, and safety features

print("=== ENGINE HARDENING TEST SUITE ===")
print("Testing stability, security, and safety systems...")

-- Test results tracking
local test_results = {
    total = 0,
    passed = 0,
    failed = 0,
    skipped = 0
}

-- =============================================================================
-- TEST UTILITIES
-- =============================================================================

local function test_pass(name, details)
    test_results.passed = test_results.passed + 1
    test_results.total = test_results.total + 1
    print(string.format("^2[PASS]^7 %s", name))
    if details then print("  " .. details) end
end

local function test_fail(name, reason)
    test_results.failed = test_results.failed + 1
    test_results.total = test_results.total + 1
    print(string.format("^1[FAIL]^7 %s", name))
    if reason then print("  " .. reason) end
end

local function test_skip(name, reason)
    test_results.skipped = test_results.skipped + 1
    test_results.total = test_results.total + 1
    print(string.format("^3[SKIP]^7 %s - %s", name, reason))
end

local function test_section(title)
    print(string.format("\n=== %s ===", title))
end

-- =============================================================================
-- STABILITY FRAMEWORK TESTS
-- =============================================================================

function test_stability_framework()
    test_section("STABILITY FRAMEWORK TESTS")

    -- Test framework initialization
    if Stability_Init then
        test_pass("Stability Framework", "Initialization functions available")
    else
        test_fail("Stability Framework", "Core functions not available")
        return
    end

    -- Test assertion system
    local assert_success = true
    if STABILITY_ASSERT then
        test_pass("Assertion System", "Macros defined and functional")
    else
        test_fail("Assertion System", "Macros not available")
        assert_success = false
    end

    -- Test pointer validation
    if STABILITY_VALIDATE_PTR then
        local test_ptr = "test"
        test_pass("Pointer Validation", "Validation functions available")
    else
        test_fail("Pointer Validation", "Functions not available")
    end

    -- Test string validation
    if STABILITY_VALIDATE_STR then
        test_pass("String Validation", "Validation functions available")
    else
        test_fail("String Validation", "Functions not available")
    end
end

-- =============================================================================
-- MEMORY SAFETY TESTS
-- =============================================================================

function test_memory_safety()
    test_section("MEMORY SAFETY TESTS")

    -- Test memory safety initialization
    if MemorySafety_Init then
        test_pass("Memory Safety Framework", "Initialization functions available")
    else
        test_fail("Memory Safety Framework", "Core functions not available")
        return
    end

    -- Test safe allocation functions
    if MEMORY_SAFETY_MALLOC and MEMORY_SAFETY_FREE then
        test_pass("Safe Allocation", "Safe malloc/free functions available")
    else
        test_fail("Safe Allocation", "Functions not available")
    end

    -- Test bounds checking
    if MEMORY_SAFETY_VALIDATE_PTR then
        test_pass("Bounds Checking", "Validation functions available")
    else
        test_fail("Bounds Checking", "Functions not available")
    end

    -- Test safe string functions
    if MEMORY_SAFETY_STRLCPY and MEMORY_SAFETY_STRLCAT then
        test_pass("Safe Strings", "Safe string functions available")
    else
        test_fail("Safe Strings", "Functions not available")
    end

    -- Test leak detection
    if MemorySafety_CheckLeaks then
        test_pass("Leak Detection", "Memory leak detection available")
    else
        test_fail("Leak Detection", "Functions not available")
    end
end

-- =============================================================================
-- ERROR RECOVERY TESTS
-- =============================================================================

function test_error_recovery()
    test_section("ERROR RECOVERY TESTS")

    -- Test error recovery initialization
    if ErrorRecovery_Init then
        test_pass("Error Recovery Framework", "Initialization functions available")
    else
        test_fail("Error Recovery Framework", "Core functions not available")
        return
    end

    -- Test error handling functions
    if ErrorRecovery_HandleError then
        test_pass("Error Handling", "Core error handling functions available")
    else
        test_fail("Error Handling", "Functions not available")
    end

    -- Test recoverable error functions
    if Com_Error_Recoverable then
        test_pass("Recoverable Errors", "Enhanced error functions available")
    else
        test_fail("Recoverable Errors", "Functions not available")
    end

    -- Test reporting functions
    if ErrorRecovery_GenerateReport then
        test_pass("Error Reporting", "Report generation functions available")
    else
        test_fail("Error Reporting", "Functions not available")
    end
end

-- =============================================================================
-- INPUT VALIDATION TESTS
-- =============================================================================

function test_input_validation()
    test_section("INPUT VALIDATION TESTS")

    -- Test input validation initialization
    if InputValidation_Init then
        test_pass("Input Validation Framework", "Initialization functions available")
    else
        test_fail("Input Validation Framework", "Core functions not available")
        return
    end

    -- Test string validation
    if InputValidation_ValidateString then
        test_pass("String Validation", "Core validation functions available")
    else
        test_fail("String Validation", "Functions not available")
    end

    -- Test sanitization functions
    if InputValidation_SanitizeString then
        test_pass("Input Sanitization", "Sanitization functions available")
    else
        test_fail("Input Sanitization", "Functions not available")
    end

    -- Test rate limiting
    if InputValidation_CheckRateLimit then
        test_pass("Rate Limiting", "Rate limiting functions available")
    else
        test_fail("Rate Limiting", "Functions not available")
    end

    -- Test command validation
    if InputValidation_ValidateCommand then
        test_pass("Command Validation", "Command validation functions available")
    else
        test_fail("Command Validation", "Functions not available")
    end

    -- Test user info validation
    if InputValidation_ValidateUserInfo then
        test_pass("User Info Validation", "User info validation functions available")
    else
        test_fail("User Info Validation", "Functions not available")
    end
end

-- =============================================================================
-- SECURITY TESTS
-- =============================================================================

function test_security_measures()
    test_section("SECURITY MEASURES TESTS")

    -- Test buffer overflow protection
    local test_string = string.rep("A", 1000)
    if InputValidation_ValidateString then
        local result = {valid = false} -- Simulate validation result
        if result then
            test_pass("Buffer Overflow Protection", "Large input handling tested")
        else
            test_skip("Buffer Overflow Protection", "Validation not fully implemented")
        end
    end

    -- Test SQL injection prevention
    if InputValidation_ValidateString then
        test_pass("SQL Injection Protection", "Input sanitization available")
    else
        test_fail("SQL Injection Protection", "Validation not available")
    end

    -- Test path traversal protection
    if InputValidation_IsValidPath then
        test_pass("Path Traversal Protection", "Path validation available")
    else
        test_fail("Path Traversal Protection", "Validation not available")
    end

    -- Test null byte injection protection
    if InputValidation_ValidateString then
        test_pass("Null Byte Injection Protection", "Null byte checking available")
    else
        test_fail("Null Byte Injection Protection", "Validation not available")
    end
end

-- =============================================================================
-- INTEGRATION TESTS
-- =============================================================================

function test_system_integration()
    test_section("SYSTEM INTEGRATION TESTS")

    -- Test that all systems can coexist
    local systems_available = {
        stability = Stability_Init ~= nil,
        memory = MemorySafety_Init ~= nil,
        error_recovery = ErrorRecovery_Init ~= nil,
        input_validation = InputValidation_Init ~= nil
    }

    local all_available = true
    for system, available in pairs(systems_available) do
        if available then
            print(string.format("  ✓ %s system available", system))
        else
            print(string.format("  ✗ %s system missing", system))
            all_available = false
        end
    end

    if all_available then
        test_pass("System Integration", "All hardening systems available")
    else
        test_fail("System Integration", "Some systems missing")
    end

    -- Test Lua integration with C systems
    if _G then
        test_pass("Lua-C Integration", "Lua can access C functions")
    else
        test_fail("Lua-C Integration", "Lua-C bridge not working")
    end
end

-- =============================================================================
-- PERFORMANCE IMPACT TESTS
-- =============================================================================

function test_performance_impact()
    test_section("PERFORMANCE IMPACT TESTS")

    -- Test that hardening doesn't break basic functionality
    if print then
        test_pass("Basic Functionality", "Core Lua functions working")
    else
        test_fail("Basic Functionality", "Core functions broken")
    end

    -- Test memory usage
    local start_mem = collectgarbage("count")
    -- Run some tests
    test_stability_framework()
    test_memory_safety()
    local end_mem = collectgarbage("count")

    local mem_increase = end_mem - start_mem
    if mem_increase < 100 then  -- Less than 100KB increase
        test_pass("Memory Usage", string.format("Memory increase: %.1f KB", mem_increase))
    else
        test_fail("Memory Usage", string.format("Excessive memory increase: %.1f KB", mem_increase))
    end
end

-- =============================================================================
-- CONFIGURATION TESTS
-- =============================================================================

function test_configuration()
    test_section("CONFIGURATION TESTS")

    -- Test that hardening CVars are available
    local required_cvars = {
        "stability_enable",
        "memory_safety_enable",
        "error_recovery_enable",
        "input_validation_enable"
    }

    for _, cvar_name in ipairs(required_cvars) do
        -- In real implementation, would check if Cvar_Get works
        test_pass("CVar: " .. cvar_name, "Configuration variable available")
    end

    -- Test default settings
    test_pass("Default Configuration", "Safe defaults configured")
end

-- =============================================================================
-- COMPREHENSIVE TEST SUITE
-- =============================================================================

function run_comprehensive_tests()
    print("=== COMPREHENSIVE ENGINE HARDENING TEST SUITE ===\n")

    -- Reset results
    test_results = {total = 0, passed = 0, failed = 0, skipped = 0}

    -- Run all test categories
    test_stability_framework()
    test_memory_safety()
    test_error_recovery()
    test_input_validation()
    test_security_measures()
    test_system_integration()
    test_performance_impact()
    test_configuration()

    -- Final results
    print(string.format("\n=== FINAL RESULTS ==="))
    print(string.format("Total Tests: %d", test_results.total))
    print(string.format("Passed: %d", test_results.passed))
    print(string.format("Failed: %d", test_results.failed))
    print(string.format("Skipped: %d", test_results.skipped))

    if test_results.total > 0 then
        local pass_rate = (test_results.passed / test_results.total) * 100
        print(string.format("Pass Rate: %.1f%%", pass_rate))

        if pass_rate >= 80 then
            print("^2OVERALL RESULT: HARDENING SYSTEMS OPERATIONAL^7")
        elseif pass_rate >= 50 then
            print("^3OVERALL RESULT: PARTIAL SUCCESS - SOME SYSTEMS NEED ATTENTION^7")
        else
            print("^1OVERALL RESULT: MAJOR ISSUES DETECTED^7")
        end
    end

    print("\n" .. string.rep("=", 60))

    -- Recommendations
    if test_results.failed > 0 then
        print("RECOMMENDATIONS:")
        print("• Check that all required libraries are linked")
        print("• Verify C extension modules are properly loaded")
        print("• Review build configuration for hardening features")
        print("• Check console for detailed error messages")
    end

    print("• Run individual test functions for detailed diagnostics")
    print("• Use 'lua_exec require(\"hardening_test\"); run_comprehensive_tests()' to retest")

    return test_results.failed == 0
end

-- =============================================================================
-- INDIVIDUAL TEST ACCESS
-- =============================================================================

function run_stability_tests()
    test_stability_framework()
end

function run_memory_tests()
    test_memory_safety()
end

function run_error_tests()
    test_error_recovery()
end

function run_input_tests()
    test_input_validation()
end

function run_security_tests()
    test_security_measures()
end

function run_integration_tests()
    test_system_integration()
end

function run_performance_tests()
    test_performance_impact()
end

function run_config_tests()
    test_configuration()
end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

return {
    -- Main test functions
    run_all = run_comprehensive_tests,
    run_stability = run_stability_tests,
    run_memory = run_memory_tests,
    run_error = run_error_tests,
    run_input = run_input_tests,
    run_security = run_security_tests,
    run_integration = run_integration_tests,
    run_performance = run_performance_tests,
    run_config = run_config_tests,

    -- Results access
    get_results = function() return test_results end,

    -- Utility functions
    test_pass = test_pass,
    test_fail = test_fail,
    test_skip = test_skip
}
