/*
=============================================================================
Backwards Compatibility Test Suite

Tests backwards compatibility detection and legacy mode management
=============================================================================
*/

#include "backwards_compatibility.h"
#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>
#include <string.h>

// Test data
static const char *test_mod_names[] = {
    "baseq3",
    "osp",
    "cpma",
    "excessive",
    "threewave",
    "freeze",
    "unknown_mod",
    NULL
};

static const char *test_content_paths[] = {
    "pak0.pk3",
    "pak1.pk3",
    "q3config.cfg",
    "autoexec.cfg",
    "unknown.file",
    NULL
};

/*
===============
test_legacy_mode_detection

Test basic legacy mode detection
===============
*/
static void test_legacy_mode_detection(void) {
    printf("Running test: legacy mode detection\n");

    legacy_context_t ctx;
    BC_Init(&ctx);

    // Test mod detection
    for (int i = 0; test_mod_names[i]; i++) {
        compatibility_result_t result = BC_DetectModCompatibility(test_mod_names[i]);

        // Should detect some mods as requiring legacy mode
        if (strcmp(test_mod_names[i], "baseq3") == 0) {
            if (result.detected_mode == LEGACY_MODE_NONE) {
                printf("  FAILED: baseq3 should be detected as compatible\n");
                return;
            }
        } else if (strcmp(test_mod_names[i], "unknown_mod") == 0) {
            if (!result.requires_legacy_mode) {
                printf("  FAILED: unknown_mod should require legacy mode\n");
                return;
            }
        }

        printf("  Mod '%s': %s (score: %.2f)\n",
               test_mod_names[i],
               BC_LegacyModeToString(result.detected_mode),
               result.compatibility_score);
    }

    // Test content detection
    for (int i = 0; test_content_paths[i]; i++) {
        compatibility_result_t result = BC_DetectContentCompatibility(test_content_paths[i]);

        printf("  Content '%s': %s (score: %.2f)\n",
               test_content_paths[i],
               BC_LegacyModeToString(result.detected_mode),
               result.compatibility_score);
    }

    BC_Shutdown(&ctx);
    printf("  PASSED: Legacy mode detection\n");
}

/*
===============
test_protocol_compatibility

Test network protocol compatibility detection
===============
*/
static void test_protocol_compatibility(void) {
    printf("Running test: protocol compatibility\n");

    // Test various protocol versions
    int test_protocols[] = {66, 67, 68, 69, 70, 71, 100, 200};
    legacy_mode_t expected_modes[] = {
        LEGACY_MODE_Q3_VANILLA,
        LEGACY_MODE_Q3_VANILLA,
        LEGACY_MODE_Q3_POINT_RELEASE,
        LEGACY_MODE_Q3_POINT_RELEASE,
        LEGACY_MODE_OA_COMPATIBLE,
        LEGACY_MODE_OA_COMPATIBLE,
        LEGACY_MODE_NONE,  // Modern protocol
        LEGACY_MODE_NONE   // Modern protocol
    };

    for (int i = 0; i < (int)(sizeof(test_protocols)/sizeof(test_protocols[0])); i++) {
        compatibility_result_t result = BC_DetectNetworkCompatibility(test_protocols[i]);

        if (result.detected_mode != expected_modes[i]) {
            printf("  FAILED: Protocol %d detected as %s, expected %s\n",
                   test_protocols[i],
                   BC_LegacyModeToString(result.detected_mode),
                   BC_LegacyModeToString(expected_modes[i]));
            return;
        }

        printf("  Protocol %d: %s (score: %.2f)\n",
               test_protocols[i],
               BC_LegacyModeToString(result.detected_mode),
               result.compatibility_score);
    }

    printf("  PASSED: Protocol compatibility\n");
}

/*
===============
test_legacy_mode_management

Test legacy mode setting and management
===============
*/
static void test_legacy_mode_management(void) {
    printf("Running test: legacy mode management\n");

    legacy_context_t ctx;
    BC_Init(&ctx);

    // Test setting different modes
    legacy_mode_t test_modes[] = {
        LEGACY_MODE_NONE,
        LEGACY_MODE_Q3_VANILLA,
        LEGACY_MODE_Q3_POINT_RELEASE,
        LEGACY_MODE_OA_COMPATIBLE,
        LEGACY_MODE_MOD_GENERIC
    };

    for (int i = 0; i < (int)(sizeof(test_modes)/sizeof(test_modes[0])); i++) {
        if (BC_SetLegacyMode(&ctx, test_modes[i])) {
            legacy_mode_t current = BC_GetCurrentMode(&ctx);
            if (current != test_modes[i]) {
                printf("  FAILED: Set mode %s but got %s\n",
                       BC_LegacyModeToString(test_modes[i]),
                       BC_LegacyModeToString(current));
                BC_Shutdown(&ctx);
                return;
            }

            qboolean is_legacy = BC_IsLegacyModeActive(&ctx);
            qboolean expected_legacy = (test_modes[i] != LEGACY_MODE_NONE);

            if (is_legacy != expected_legacy) {
                printf("  FAILED: Mode %s legacy state incorrect\n",
                       BC_LegacyModeToString(test_modes[i]));
                BC_Shutdown(&ctx);
                return;
            }
        } else {
            printf("  FAILED: Could not set mode %s\n",
                   BC_LegacyModeToString(test_modes[i]));
            BC_Shutdown(&ctx);
            return;
        }
    }

    // Test forced mode override
    legacy_config_t config = {0};
    config.forced_mode = LEGACY_MODE_CUSTOM;
    BC_SetConfig(&ctx, &config);

    legacy_mode_t current = BC_GetCurrentMode(&ctx);
    if (current != LEGACY_MODE_CUSTOM) {
        printf("  FAILED: Forced mode not respected\n");
        BC_Shutdown(&ctx);
        return;
    }

    BC_Shutdown(&ctx);
    printf("  PASSED: Legacy mode management\n");
}

/*
===============
test_string_conversions

Test legacy mode string conversions
===============
*/
static void test_string_conversions(void) {
    printf("Running test: string conversions\n");

    // Test string to mode conversion
    struct {
        const char *str;
        legacy_mode_t expected;
    } conversions[] = {
        {"none", LEGACY_MODE_NONE},
        {"modern", LEGACY_MODE_NONE},
        {"vanilla", LEGACY_MODE_Q3_VANILLA},
        {"q3", LEGACY_MODE_Q3_VANILLA},
        {"point", LEGACY_MODE_Q3_POINT_RELEASE},
        {"pr", LEGACY_MODE_Q3_POINT_RELEASE},
        {"oa", LEGACY_MODE_OA_COMPATIBLE},
        {"openarena", LEGACY_MODE_OA_COMPATIBLE},
        {"mod", LEGACY_MODE_MOD_GENERIC},
        {"generic", LEGACY_MODE_MOD_GENERIC},
        {"custom", LEGACY_MODE_CUSTOM},
        {"invalid", LEGACY_MODE_NONE}
    };

    for (int i = 0; i < (int)(sizeof(conversions)/sizeof(conversions[0])); i++) {
        legacy_mode_t result = BC_StringToLegacyMode(conversions[i].str);
        if (result != conversions[i].expected) {
            printf("  FAILED: String '%s' converted to %s, expected %s\n",
                   conversions[i].str,
                   BC_LegacyModeToString(result),
                   BC_LegacyModeToString(conversions[i].expected));
            return;
        }
    }

    // Test mode to string conversion
    for (int i = 0; i < (int)(sizeof(conversions)/sizeof(conversions[0])); i++) {
        if (conversions[i].expected != LEGACY_MODE_NONE) { // Skip duplicates
            const char *str = BC_LegacyModeToString(conversions[i].expected);
            if (!str || strlen(str) == 0) {
                printf("  FAILED: Mode %d has no string representation\n",
                       conversions[i].expected);
                return;
            }
        }
    }

    printf("  PASSED: String conversions\n");
}

/*
===============
test_compatibility_scoring

Test compatibility score calculations
===============
*/
static void test_compatibility_scoring(void) {
    printf("Running test: compatibility scoring\n");

    // Test score calculation with different results
    compatibility_result_t results[] = {
        {LEGACY_MODE_NONE, qfalse, "", "Perfect compatibility", 1.0f},
        {LEGACY_MODE_Q3_VANILLA, qtrue, "", "Good compatibility", 0.9f},
        {LEGACY_MODE_MOD_GENERIC, qtrue, "", "Moderate compatibility", 0.7f},
        {LEGACY_MODE_NONE, qfalse, "", "Invalid score", 2.0f}, // Should be clamped
        {LEGACY_MODE_NONE, qfalse, "", "Negative score", -1.0f} // Should be clamped
    };

    for (int i = 0; i < (int)(sizeof(results)/sizeof(results[0])); i++) {
        float score = BC_CalculateCompatibilityScore(&results[i]);

        // Check bounds
        if (score < 0.0f || score > 1.0f) {
            printf("  FAILED: Score %.2f out of bounds [0,1]\n", score);
            return;
        }

        // Check basic expectations
        if (results[i].detected_mode == LEGACY_MODE_NONE && results[i].compatibility_score == 1.0f) {
            if (score != 1.0f) {
                printf("  FAILED: Perfect compatibility should score 1.0, got %.2f\n", score);
                return;
            }
        }

        printf("  Mode %s: score %.2f -> %.2f\n",
               BC_LegacyModeToString(results[i].detected_mode),
               results[i].compatibility_score,
               score);
    }

    printf("  PASSED: Compatibility scoring\n");
}

/*
===============
test_context_management

Test backwards compatibility context management
===============
*/
static void test_context_management(void) {
    printf("Running test: context management\n");

    legacy_context_t ctx;

    // Test initialization
    if (!BC_Init(&ctx)) {
        printf("  FAILED: Context initialization failed\n");
        return;
    }

    // Test configuration
    legacy_config_t config;
    BC_GetConfig(&ctx, &config);

    if (!config.enable_legacy_detection) {
        printf("  FAILED: Legacy detection not enabled by default\n");
        BC_Shutdown(&ctx);
        return;
    }

    // Test statistics
    char stats[1024];
    BC_GetStats(&ctx, stats, sizeof(stats));

    if (strlen(stats) == 0) {
        printf("  FAILED: No statistics generated\n");
        BC_Shutdown(&ctx);
        return;
    }

    // Test shutdown
    BC_Shutdown(&ctx);

    printf("  PASSED: Context management\n");
}

/*
===============
main

Run all backwards compatibility tests
===============
*/
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Backwards Compatibility Test Suite\n");
    printf("===================================\n\n");

    int tests_run = 0;
    int tests_passed = 0;

    // Run all tests
    #define RUN_BC_TEST(test_func) \
        do { \
            tests_run++; \
            test_func(); \
            tests_passed++; \
        } while (0)

    RUN_BC_TEST(test_legacy_mode_detection);
    RUN_BC_TEST(test_protocol_compatibility);
    RUN_BC_TEST(test_legacy_mode_management);
    RUN_BC_TEST(test_string_conversions);
    RUN_BC_TEST(test_compatibility_scoring);
    RUN_BC_TEST(test_context_management);

    printf("\n===================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("All backwards compatibility tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}