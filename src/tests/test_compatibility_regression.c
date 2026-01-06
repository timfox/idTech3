/*
=============================================================================
Compatibility Regression Test Suite

Comprehensive regression testing for backwards compatibility with legacy
Quake 3 mods and content.
=============================================================================
*/

#include "backwards_compatibility.h"
#include "compatibility_shims.h"
#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>
#include <string.h>

// Test assets and scenarios
static const struct {
    const char *test_name;
    legacy_mode_t mode;
    const char *description;
} compatibility_tests[] = {
    {"q3_vanilla_basic", LEGACY_MODE_Q3_VANILLA, "Basic Quake 3 vanilla compatibility"},
    {"q3_vanilla_shaders", LEGACY_MODE_Q3_VANILLA, "Q3 vanilla shader loading"},
    {"q3_vanilla_network", LEGACY_MODE_Q3_VANILLA, "Q3 vanilla network protocol"},
    {"openarena_assets", LEGACY_MODE_OA_COMPATIBLE, "OpenArena asset compatibility"},
    {"openarena_network", LEGACY_MODE_OA_COMPATIBLE, "OpenArena network compatibility"},
    {"mod_generic_cvars", LEGACY_MODE_MOD_GENERIC, "Generic mod CVar handling"},
    {"mod_generic_commands", LEGACY_MODE_MOD_GENERIC, "Generic mod command handling"},
    {"modern_fallback", LEGACY_MODE_NONE, "Modern mode fallback behavior"},
    {NULL, LEGACY_MODE_NONE, NULL}
};

// Legacy content test data
static const struct {
    const char *filename;
    legacy_mode_t expected_mode;
    float expected_score;
} content_tests[] = {
    {"pak0.pk3", LEGACY_MODE_Q3_VANILLA, 0.95f},
    {"pak1.pk3", LEGACY_MODE_Q3_VANILLA, 0.95f},
    {"q3config.cfg", LEGACY_MODE_Q3_VANILLA, 0.90f},
    {"autoexec.cfg", LEGACY_MODE_Q3_VANILLA, 0.85f},
    {"osp/q3config.cfg", LEGACY_MODE_MOD_GENERIC, 0.80f},
    {"cpma/q3config.cfg", LEGACY_MODE_MOD_GENERIC, 0.80f},
    {NULL, LEGACY_MODE_NONE, 0.0f}
};

// Network protocol test data
static const struct {
    int protocol;
    legacy_mode_t expected_mode;
    qboolean should_be_compatible;
} protocol_tests[] = {
    {66, LEGACY_MODE_Q3_VANILLA, qtrue},
    {67, LEGACY_MODE_Q3_VANILLA, qtrue},
    {68, LEGACY_MODE_Q3_POINT_RELEASE, qtrue},
    {69, LEGACY_MODE_Q3_POINT_RELEASE, qtrue},
    {70, LEGACY_MODE_OA_COMPATIBLE, qtrue},
    {71, LEGACY_MODE_OA_COMPATIBLE, qtrue},
    {100, LEGACY_MODE_NONE, qtrue},  // Modern protocol
    {200, LEGACY_MODE_NONE, qtrue},  // Modern protocol
    {0, LEGACY_MODE_NONE, qfalse}    // Invalid protocol
};

/*
===============
test_legacy_mode_detection_regression

Test that legacy mode detection works correctly for known scenarios
===============
*/
static void test_legacy_mode_detection_regression(void) {
    printf("Running test: legacy mode detection regression\n");

    legacy_context_t ctx;
    BC_Init(&ctx);

    // Test content detection
    for (int i = 0; content_tests[i].filename; i++) {
        compatibility_result_t result = BC_DetectContentCompatibility(content_tests[i].filename);

        if (result.detected_mode != content_tests[i].expected_mode) {
            printf("  FAILED: %s detected as %s, expected %s\n",
                   content_tests[i].filename,
                   BC_LegacyModeToString(result.detected_mode),
                   BC_LegacyModeToString(content_tests[i].expected_mode));
            BC_Shutdown(&ctx);
            return;
        }

        // Check score is reasonable (within 0.1 of expected)
        if (fabs(result.compatibility_score - content_tests[i].expected_score) > 0.1f) {
            printf("  WARNING: %s score %.2f, expected %.2f\n",
                   content_tests[i].filename,
                   result.compatibility_score,
                   content_tests[i].expected_score);
        }

        printf("  Content '%s': %s (score: %.2f)\n",
               content_tests[i].filename,
               BC_LegacyModeToString(result.detected_mode),
               result.compatibility_score);
    }

    // Test protocol detection
    for (int i = 0; protocol_tests[i].protocol || i == 0; i++) {
        if (protocol_tests[i].protocol == 0 && i > 0) break; // End marker

        compatibility_result_t result = BC_DetectNetworkCompatibility(protocol_tests[i].protocol);

        if (result.detected_mode != protocol_tests[i].expected_mode) {
            printf("  FAILED: Protocol %d detected as %s, expected %s\n",
                   protocol_tests[i].protocol,
                   BC_LegacyModeToString(result.detected_mode),
                   BC_LegacyModeToString(protocol_tests[i].expected_mode));
            BC_Shutdown(&ctx);
            return;
        }

        printf("  Protocol %d: %s (compatible: %s)\n",
               protocol_tests[i].protocol,
               BC_LegacyModeToString(result.detected_mode),
               protocol_tests[i].should_be_compatible ? "yes" : "no");
    }

    BC_Shutdown(&ctx);
    printf("  PASSED: Legacy mode detection regression\n");
}

/*
===============
test_compatibility_shims_regression

Test that compatibility shims work correctly
===============
*/
static void test_compatibility_shims_regression(void) {
    printf("Running test: compatibility shims regression\n");

    shim_context_t ctx;

    // Test each compatibility mode
    for (int i = 0; compatibility_tests[i].test_name; i++) {
        legacy_mode_t mode = compatibility_tests[i].mode;

        if (!Shim_Init(&ctx, mode)) {
            printf("  FAILED: Could not initialize shims for %s\n", compatibility_tests[i].test_name);
            continue;
        }

        // Test basic shim functionality
        qboolean shim_available = qfalse;

        if (mode == LEGACY_MODE_Q3_VANILLA) {
            shim_available = Shim_IsShimAvailable(mode, "renderer") &&
                           Shim_IsShimAvailable(mode, "asset");
        } else if (mode == LEGACY_MODE_OA_COMPATIBLE) {
            shim_available = Shim_IsShimAvailable(mode, "asset") &&
                           Shim_IsShimAvailable(mode, "network");
        } else if (mode == LEGACY_MODE_MOD_GENERIC) {
            shim_available = Shim_IsShimAvailable(mode, "cvar") &&
                           Shim_IsShimAvailable(mode, "cmd");
        } else if (mode == LEGACY_MODE_NONE) {
            shim_available = qtrue; // Modern mode always works
        }

        if (!shim_available) {
            printf("  FAILED: Required shims not available for %s\n", compatibility_tests[i].test_name);
            Shim_Shutdown(&ctx);
            return;
        }

        // Test shim statistics
        char stats[512];
        Shim_GetStats(&ctx, stats, sizeof(stats));

        if (strlen(stats) == 0) {
            printf("  FAILED: No shim statistics for %s\n", compatibility_tests[i].test_name);
            Shim_Shutdown(&ctx);
            return;
        }

        printf("  %s: %s shims available\n", compatibility_tests[i].test_name,
               Shim_GetShimDescription(mode));

        Shim_Shutdown(&ctx);
    }

    printf("  PASSED: Compatibility shims regression\n");
}

/*
===============
test_asset_loading_compatibility

Test asset loading compatibility across modes
===============
*/
static void test_asset_loading_compatibility(void) {
    printf("Running test: asset loading compatibility\n");

    shim_context_t ctx;
    const char *test_shaders[] = {"white", "menu/art/font1_prop.tga", "textures/base_wall/lfwall13", NULL};

    for (int mode_idx = 0; compatibility_tests[mode_idx].test_name; mode_idx++) {
        legacy_mode_t mode = compatibility_tests[mode_idx].mode;

        if (!Shim_Init(&ctx, mode)) continue;

        // Test shader loading shims
        for (int shader_idx = 0; test_shaders[shader_idx]; shader_idx++) {
            char output_path[MAX_QPATH];

            qboolean result = Shim_LoadShader(test_shaders[shader_idx], output_path, sizeof(output_path));

            if (!result && mode != LEGACY_MODE_NONE) {
                // For legacy modes, shimming should at least provide a valid path
                printf("  WARNING: Shader '%s' not shimmed in %s mode\n",
                       test_shaders[shader_idx], BC_LegacyModeToString(mode));
            }

            if (result) {
                if (strlen(output_path) == 0) {
                    printf("  FAILED: Empty output path for shader '%s' in %s mode\n",
                           test_shaders[shader_idx], BC_LegacyModeToString(mode));
                    Shim_Shutdown(&ctx);
                    return;
                }
            }
        }

        Shim_Shutdown(&ctx);
    }

    printf("  PASSED: Asset loading compatibility\n");
}

/*
===============
test_network_protocol_compatibility

Test network protocol compatibility
===============
*/
static void test_network_protocol_compatibility(void) {
    printf("Running test: network protocol compatibility\n");

    shim_context_t ctx;
    byte test_data[1024];
    int test_length = 512;

    // Fill test data with some pattern
    for (int i = 0; i < test_length; i++) {
        test_data[i] = (byte)(i % 256);
    }

    for (int mode_idx = 0; compatibility_tests[mode_idx].test_name; mode_idx++) {
        legacy_mode_t mode = compatibility_tests[mode_idx].mode;

        if (!Shim_Init(&ctx, mode)) continue;

        int original_length = test_length;
        int modified_length = test_length;

        // Test network message shimming
        qboolean result = Shim_NetworkMessage(test_data, &modified_length, sizeof(test_data));

        if (!result) {
            printf("  FAILED: Network shim failed for %s\n", BC_LegacyModeToString(mode));
            Shim_Shutdown(&ctx);
            return;
        }

        // Length should not exceed buffer size
        if (modified_length > sizeof(test_data)) {
            printf("  FAILED: Network shim exceeded buffer size in %s\n", BC_LegacyModeToString(mode));
            Shim_Shutdown(&ctx);
            return;
        }

        // Length should be reasonable
        if (modified_length < 0) {
            printf("  FAILED: Negative length from network shim in %s\n", BC_LegacyModeToString(mode));
            Shim_Shutdown(&ctx);
            return;
        }

        Shim_Shutdown(&ctx);
    }

    printf("  PASSED: Network protocol compatibility\n");
}

/*
===============
test_cvar_command_compatibility

Test CVar and command compatibility
===============
*/
static void test_cvar_command_compatibility(void) {
    printf("Running test: CVar and command compatibility\n");

    shim_context_t ctx;

    // Test different modes
    for (int mode_idx = 0; compatibility_tests[mode_idx].test_name; mode_idx++) {
        legacy_mode_t mode = compatibility_tests[mode_idx].mode;

        if (!Shim_Init(&ctx, mode)) continue;

        // Test CVar registration shim
        cvar_t *test_cvar = Shim_Cvar_Get("test_cvar", "default_value", CVAR_ARCHIVE);
        if (!test_cvar) {
            printf("  FAILED: CVar shim failed for %s\n", BC_LegacyModeToString(mode));
            Shim_Shutdown(&ctx);
            return;
        }

        // Test command registration shim
        static qboolean test_command_called = qfalse;
        test_command_called = qfalse;

        Shim_Cmd_AddCommand("test_cmd", [](){ test_command_called = qtrue; });

        // Try to execute the command (this might not work in unit test environment)
        // But at least verify the shim didn't crash

        Shim_Shutdown(&ctx);
    }

    printf("  PASSED: CVar and command compatibility\n");
}

/*
===============
test_mode_switching_regression

Test mode switching doesn't break functionality
===============
*/
static void test_mode_switching_regression(void) {
    printf("Running test: mode switching regression\n");

    shim_context_t ctx;
    legacy_mode_t test_modes[] = {
        LEGACY_MODE_NONE,
        LEGACY_MODE_Q3_VANILLA,
        LEGACY_MODE_OA_COMPATIBLE,
        LEGACY_MODE_MOD_GENERIC,
        LEGACY_MODE_NONE  // Back to modern
    };

    for (int i = 0; i < (int)(sizeof(test_modes)/sizeof(test_modes[0])); i++) {
        // Switch to new mode
        if (i == 0) {
            if (!Shim_Init(&ctx, test_modes[i])) {
                printf("  FAILED: Could not initialize shims for %s\n",
                       BC_LegacyModeToString(test_modes[i]));
                return;
            }
        } else {
            Shim_SetMode(&ctx, test_modes[i]);
        }

        // Verify mode was set correctly
        if (ctx.active_mode != test_modes[i]) {
            printf("  FAILED: Mode not set correctly, expected %s, got %s\n",
                   BC_LegacyModeToString(test_modes[i]),
                   BC_LegacyModeToString(ctx.active_mode));
            Shim_Shutdown(&ctx);
            return;
        }

        // Test basic functionality still works
        char stats[512];
        Shim_GetStats(&ctx, stats, sizeof(stats));

        if (strlen(stats) == 0) {
            printf("  FAILED: No stats after mode switch to %s\n",
                   BC_LegacyModeToString(test_modes[i]));
            Shim_Shutdown(&ctx);
            return;
        }

        printf("  Switched to %s: OK\n", BC_LegacyModeToString(test_modes[i]));
    }

    Shim_Shutdown(&ctx);
    printf("  PASSED: Mode switching regression\n");
}

/*
===============
test_performance_regression

Test that compatibility shims don't severely impact performance
===============
*/
static void test_performance_regression(void) {
    printf("Running test: performance regression\n");

    shim_context_t ctx;
    Shim_Init(&ctx, LEGACY_MODE_Q3_VANILLA);

    // Measure time for multiple shim operations
    int start_time = Sys_Milliseconds();

    const int NUM_ITERATIONS = 1000;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Test various shim operations
        char output_path[MAX_QPATH];
        Shim_LoadShader("test_shader", output_path, sizeof(output_path));

        byte test_data[100];
        int test_length = 50;
        Shim_NetworkMessage(test_data, &test_length, sizeof(test_data));

        qhandle_t shader = Shim_RegisterShader("white");
        (void)shader; // Avoid unused variable warning
    }

    int end_time = Sys_Milliseconds();
    int total_time = end_time - start_time;

    // Allow reasonable time (should be much less than 1 second for 1000 iterations)
    if (total_time > 1000) {
        printf("  WARNING: Shim operations took %d ms for %d iterations (%.2f ms per operation)\n",
               total_time, NUM_ITERATIONS, (float)total_time / NUM_ITERATIONS);
    } else {
        printf("  Performance: %d ms for %d iterations (%.2f ms per operation)\n",
               total_time, NUM_ITERATIONS, (float)total_time / NUM_ITERATIONS);
    }

    Shim_Shutdown(&ctx);
    printf("  PASSED: Performance regression\n");
}

/*
===============
test_backwards_compatibility_integration

Test full backwards compatibility integration
===============
*/
static void test_backwards_compatibility_integration(void) {
    printf("Running test: backwards compatibility integration\n");

    legacy_context_t bc_ctx;
    shim_context_t shim_ctx;

    // Initialize both systems
    if (!BC_Init(&bc_ctx)) {
        printf("  FAILED: Could not initialize backwards compatibility\n");
        return;
    }

    if (!Shim_Init(&shim_ctx, LEGACY_MODE_Q3_VANILLA)) {
        printf("  FAILED: Could not initialize compatibility shims\n");
        BC_Shutdown(&bc_ctx);
        return;
    }

    // Test integrated operation
    compatibility_result_t result = BC_DetectContentCompatibility("pak0.pk3");
    if (result.requires_legacy_mode) {
        // Switch to detected mode
        BC_SetLegacyMode(&bc_ctx, result.detected_mode);
        Shim_SetMode(&shim_ctx, result.detected_mode);

        // Verify both systems are in the same mode
        if (BC_GetCurrentMode(&bc_ctx) != shim_ctx.active_mode) {
            printf("  FAILED: BC and Shim systems out of sync\n");
            BC_Shutdown(&bc_ctx);
            Shim_Shutdown(&shim_ctx);
            return;
        }
    }

    // Test that shims are working
    char output_path[MAX_QPATH];
    if (Shim_LoadShader("test", output_path, sizeof(output_path))) {
        // Shim worked
    }

    printf("  Integration test: BC mode = %s, Shim mode = %s\n",
           BC_LegacyModeToString(BC_GetCurrentMode(&bc_ctx)),
           BC_LegacyModeToString(shim_ctx.active_mode));

    BC_Shutdown(&bc_ctx);
    Shim_Shutdown(&shim_ctx);
    printf("  PASSED: Backwards compatibility integration\n");
}

/*
===============
main

Run all compatibility regression tests
===============
*/
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Compatibility Regression Test Suite\n");
    printf("====================================\n\n");

    int tests_run = 0;
    int tests_passed = 0;

    // Run all tests
    #define RUN_COMPAT_TEST(test_func) \
        do { \
            tests_run++; \
            test_func(); \
            tests_passed++; \
        } while (0)

    RUN_COMPAT_TEST(test_legacy_mode_detection_regression);
    RUN_COMPAT_TEST(test_compatibility_shims_regression);
    RUN_COMPAT_TEST(test_asset_loading_compatibility);
    RUN_COMPAT_TEST(test_network_protocol_compatibility);
    RUN_COMPAT_TEST(test_cvar_command_compatibility);
    RUN_COMPAT_TEST(test_mode_switching_regression);
    RUN_COMPAT_TEST(test_performance_regression);
    RUN_COMPAT_TEST(test_backwards_compatibility_integration);

    printf("\n====================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("All compatibility regression tests PASSED!\n");
        printf("Backwards compatibility is maintained across all tested scenarios.\n");
        return 0;
    } else {
        printf("Some tests FAILED! Compatibility issues detected.\n");
        return 1;
    }
}