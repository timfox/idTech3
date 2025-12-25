/*
=============================================================================
Binary Analysis System Test

Automated security scanning and optimization analysis tests.
=============================================================================
*/

#include "q_shared.h"
#include "binary_analysis.h"
#include <stdio.h>
#include <stdlib.h>

static qboolean test_binary_analysis_initialization(void) {
    printf("Testing binary analysis system initialization...\n");

    if (!BinaryAnalysis_Init()) {
        printf("FAILED: Could not initialize binary analysis system\n");
        return qfalse;
    }

    if (!binary_analysis.initialized) {
        printf("FAILED: Binary analysis system not marked as initialized\n");
        return qfalse;
    }

    printf("PASSED: Binary analysis system initialized successfully\n");
    return qtrue;
}

static qboolean test_binary_file_detection(void) {
    printf("Testing binary file detection...\n");

    // Test with a text file (should not be detected as binary)
    FILE* text_file = fopen("test_text.txt", "w");
    if (text_file) {
        fprintf(text_file, "This is a text file, not a binary.\n");
        fclose(text_file);

        if (BinaryAnalysis_IsBinaryFile("test_text.txt")) {
            printf("FAILED: Text file incorrectly detected as binary\n");
            unlink("test_text.txt");
            return qfalse;
        }

        unlink("test_text.txt");
    }

    // Test with current executable (should be detected as binary)
    if (!BinaryAnalysis_IsBinaryFile("./idtech3")) {
        // Try with different names if needed
        qboolean found_binary = qfalse;
        const char* binary_names[] = {"./ioquake3", "./quake3", NULL};

        for (int i = 0; binary_names[i] && !found_binary; i++) {
            if (BinaryAnalysis_IsBinaryFile(binary_names[i])) {
                found_binary = qtrue;
            }
        }

        if (!found_binary) {
            printf("FAILED: Could not detect any binary file\n");
            return qfalse;
        }
    }

    printf("PASSED: Binary file detection works correctly\n");
    return qtrue;
}

static qboolean test_binary_analysis_execution(void) {
    printf("Testing binary analysis execution...\n");

    // Try to analyze a binary file
    const char* test_binary = "./idtech3";

    // If the main binary doesn't exist, try alternatives
    if (!BinaryAnalysis_IsBinaryFile(test_binary)) {
        const char* alternatives[] = {"./ioquake3", "./quake3", "./test_performance_regression", NULL};

        for (int i = 0; alternatives[i]; i++) {
            if (BinaryAnalysis_IsBinaryFile(alternatives[i])) {
                test_binary = alternatives[i];
                break;
            }
        }
    }

    if (!BinaryAnalysis_IsBinaryFile(test_binary)) {
        printf("SKIPPED: No suitable binary file found for testing\n");
        return qtrue; // Skip this test if no binary is available
    }

    binary_analysis_result_t* result = BinaryAnalysis_AnalyzeBinary(test_binary);
    if (!result) {
        printf("FAILED: Could not analyze binary file: %s\n", test_binary);
        return qfalse;
    }

    // Verify basic result structure
    if (strcmp(result->binary_path, test_binary) != 0) {
        printf("FAILED: Binary path not stored correctly\n");
        return qfalse;
    }

    if (result->binary_size_bytes == 0) {
        printf("FAILED: Binary size not detected\n");
        return qfalse;
    }

    if (result->analysis_time_ms == 0) {
        printf("FAILED: Analysis time not recorded\n");
        return qfalse;
    }

    printf("PASSED: Binary analysis execution works (size: %.2f MB, time: %llu ms)\n",
           result->binary_size_bytes / (1024.0 * 1024.0),
           (unsigned long long)result->analysis_time_ms);
    return qtrue;
}

static qboolean test_analysis_finding_management(void) {
    printf("Testing analysis finding management...\n");

    // Create a test result
    binary_analysis_result_t* result = BinaryAnalysis_AnalyzeBinary("./nonexistent_binary");
    if (!result) {
        printf("FAILED: Could not create analysis result\n");
        return qfalse;
    }

    // Add a test finding
    if (!BinaryAnalysis_AddFinding(result,
                                 "Test security vulnerability",
                                 "Fix this security issue",
                                 ANALYSIS_SECURITY, 7, qtrue, qfalse,
                                 "test_file.c", 42, "test_function", 0x1000,
                                 "Additional technical details")) {
        printf("FAILED: Could not add finding\n");
        return qfalse;
    }

    if (result->finding_count != 1) {
        printf("FAILED: Finding count not updated\n");
        return qfalse;
    }

    // Check finding content
    analysis_finding_t* finding = &result->findings[0];
    if (strcmp(finding->description, "Test security vulnerability") != 0) {
        printf("FAILED: Finding description not stored correctly\n");
        return qfalse;
    }

    if (finding->severity_level != 7) {
        printf("FAILED: Finding severity not stored correctly\n");
        return qfalse;
    }

    printf("PASSED: Analysis finding management works correctly\n");
    return qtrue;
}

static qboolean test_analysis_statistics(void) {
    printf("Testing analysis statistics...\n");

    // Analyze a few binaries to generate statistics
    BinaryAnalysis_AnalyzeBinary("./test_binary_analysis");

    if (binary_analysis.total_binaries_analyzed == 0) {
        printf("FAILED: No binaries recorded in statistics\n");
        return qfalse;
    }

    printf("PASSED: Analysis statistics tracking works (%u binaries analyzed)\n",
           binary_analysis.total_binaries_analyzed);
    return qtrue;
}

static qboolean test_analysis_reporting(void) {
    printf("Testing analysis reporting...\n");

    if (!BinaryAnalysis_GenerateReport("test_analysis_report.txt", "text")) {
        printf("FAILED: Could not generate text report\n");
        return qfalse;
    }

    if (!BinaryAnalysis_GenerateReport("test_analysis_report.json", "json")) {
        printf("FAILED: Could not generate JSON report\n");
        return qfalse;
    }

    printf("PASSED: Analysis report generation works correctly\n");
    return qtrue;
}

static qboolean test_ci_integration(void) {
    printf("Testing CI integration...\n");

    // Test CI security gate checking
    qboolean ci_passed = BinaryAnalysis_CheckCISecurityGates();

    // Test security status retrieval
    char status[32];
    if (!BinaryAnalysis_GetSecurityStatus(status, sizeof(status))) {
        printf("FAILED: Could not get security status\n");
        return qfalse;
    }

    printf("PASSED: CI integration works (gates: %s, status: %s)\n",
           ci_passed ? "PASSED" : "FAILED", status);
    return qtrue;
}

static qboolean test_analysis_utility_functions(void) {
    printf("Testing analysis utility functions...\n");

    // Test result string conversion
    if (strcmp(BinaryAnalysis_GetResultString(ANALYSIS_PASS), "PASS") != 0) {
        printf("FAILED: Result string conversion incorrect\n");
        return qfalse;
    }

    // Test category string conversion
    if (strcmp(BinaryAnalysis_GetCategoryString(ANALYSIS_SECURITY), "Security") != 0) {
        printf("FAILED: Category string conversion incorrect\n");
        return qfalse;
    }

    // Test vulnerability string conversion
    if (strcmp(BinaryAnalysis_GetVulnerabilityString(VULN_BUFFER_OVERFLOW), "Buffer Overflow") != 0) {
        printf("FAILED: Vulnerability string conversion incorrect\n");
        return qfalse;
    }

    printf("PASSED: Analysis utility functions work correctly\n");
    return qtrue;
}

int main(int argc, char* argv[]) {
    printf("=== Binary Analysis System Tests ===\n\n");

    int tests_passed = 0;
    int total_tests = 8;

    if (test_binary_analysis_initialization()) tests_passed++;
    if (test_binary_file_detection()) tests_passed++;
    if (test_binary_analysis_execution()) tests_passed++;
    if (test_analysis_finding_management()) tests_passed++;
    if (test_analysis_statistics()) tests_passed++;
    if (test_analysis_reporting()) tests_passed++;
    if (test_ci_integration()) tests_passed++;
    if (test_analysis_utility_functions()) tests_passed++;

    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);

    if (tests_passed == total_tests) {
        printf("ALL TESTS PASSED!\n");
    } else {
        printf("SOME TESTS FAILED!\n");
    }

    // Cleanup
    BinaryAnalysis_Shutdown();

    return (tests_passed == total_tests) ? 0 : 1;
}
