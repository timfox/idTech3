/*
=============================================================================
Asset Validation System Test

Automated asset correctness and optimization checking tests.
=============================================================================
*/

#include "q_shared.h"
#include "asset_validation.h"
#include <stdio.h>
#include <stdlib.h>

static qboolean test_asset_validation_initialization(void) {
    printf("Testing asset validation system initialization...\n");

    if (!AssetValidation_Init()) {
        printf("FAILED: Could not initialize asset validation system\n");
        return qfalse;
    }

    if (!asset_validation.initialized) {
        printf("FAILED: Asset validation system not marked as initialized\n");
        return qfalse;
    }

    printf("PASSED: Asset validation system initialized successfully\n");
    return qtrue;
}

static qboolean test_asset_type_detection(void) {
    printf("Testing asset type detection...\n");

    // Test various file extensions
    const struct {
        const char* filename;
        const char* expected_type;
        qboolean should_detect;
    } test_cases[] = {
        {"texture.png", "texture", qtrue},
        {"model.obj", "model", qtrue},
        {"sound.wav", "sound", qtrue},
        {"shader.glsl", "shader", qtrue},
        {"material.mat", "material", qtrue},
        {"invalid.xyz", NULL, qfalse},
        {NULL, NULL, qfalse}
    };

    for (int i = 0; test_cases[i].filename; i++) {
        char detected_type[32];
        qboolean result = AssetValidation_GetAssetTypeFromPath(test_cases[i].filename,
                                                              detected_type, sizeof(detected_type));

        if (result != test_cases[i].should_detect) {
            printf("FAILED: Detection mismatch for %s (expected %s, got %s)\n",
                   test_cases[i].filename,
                   test_cases[i].should_detect ? "detected" : "not detected",
                   result ? "detected" : "not detected");
            return qfalse;
        }

        if (result && test_cases[i].expected_type &&
            Q_stricmp(detected_type, test_cases[i].expected_type) != 0) {
            printf("FAILED: Type mismatch for %s (expected %s, got %s)\n",
                   test_cases[i].filename, test_cases[i].expected_type, detected_type);
            return qfalse;
        }
    }

    printf("PASSED: Asset type detection works correctly\n");
    return qtrue;
}

static qboolean test_validation_issue_management(void) {
    printf("Testing validation issue management...\n");

    // Create a dummy result
    asset_validation_result_t result;
    memset(&result, 0, sizeof(result));
    Q_strncpyz(result.asset_path, "test_asset.txt", sizeof(result.asset_path));
    result.max_issues = 10;
    result.issues = (validation_issue_t*)malloc(sizeof(validation_issue_t) * result.max_issues);

    if (!result.issues) {
        printf("FAILED: Could not allocate memory for test issues\n");
        return qfalse;
    }

    // Test adding issues
    if (!AssetValidation_AddIssue(&result,
                                "Test error message",
                                "Test recommendation",
                                ISSUE_ERROR,
                                CHECK_CORRECTNESS,
                                "test_asset.txt",
                                42,
                                qfalse)) {
        free(result.issues);
        printf("FAILED: Could not add validation issue\n");
        return qfalse;
    }

    if (result.issue_count != 1) {
        free(result.issues);
        printf("FAILED: Issue count not updated correctly\n");
        return qfalse;
    }

    if (strcmp(result.issues[0].description, "Test error message") != 0) {
        free(result.issues);
        printf("FAILED: Issue description not stored correctly\n");
        return qfalse;
    }

    free(result.issues);
    printf("PASSED: Validation issue management works correctly\n");
    return qtrue;
}

static qboolean test_texture_validation(void) {
    printf("Testing texture validation...\n");

    // Test with a non-existent file
    asset_validation_result_t* result = AssetValidation_ValidateAsset("nonexistent_texture.png", "texture");

    if (!result) {
        printf("FAILED: Could not create validation result for nonexistent texture\n");
        return qfalse;
    }

    if (result->overall_result != VALIDATION_CRITICAL) {
        printf("FAILED: Nonexistent texture should result in critical validation error\n");
        return qfalse;
    }

    if (result->issue_count == 0) {
        printf("FAILED: Nonexistent texture should generate validation issues\n");
        return qfalse;
    }

    printf("PASSED: Texture validation handles nonexistent files correctly\n");
    return qtrue;
}

static qboolean test_model_validation(void) {
    printf("Testing model validation...\n");

    // Test with a non-existent file
    asset_validation_result_t* result = AssetValidation_ValidateAsset("nonexistent_model.obj", "model");

    if (!result) {
        printf("FAILED: Could not create validation result for nonexistent model\n");
        return qfalse;
    }

    if (result->overall_result != VALIDATION_CRITICAL) {
        printf("FAILED: Nonexistent model should result in critical validation error\n");
        return qfalse;
    }

    printf("PASSED: Model validation handles nonexistent files correctly\n");
    return qtrue;
}

static qboolean test_sound_validation(void) {
    printf("Testing sound validation...\n");

    // Test with a non-existent file
    asset_validation_result_t* result = AssetValidation_ValidateAsset("nonexistent_sound.wav", "sound");

    if (!result) {
        printf("FAILED: Could not create validation result for nonexistent sound\n");
        return qfalse;
    }

    if (result->overall_result != VALIDATION_CRITICAL) {
        printf("FAILED: Nonexistent sound should result in critical validation error\n");
        return qfalse;
    }

    printf("PASSED: Sound validation handles nonexistent files correctly\n");
    return qtrue;
}

static qboolean test_shader_validation(void) {
    printf("Testing shader validation...\n");

    // Test with a non-existent file
    asset_validation_result_t* result = AssetValidation_ValidateAsset("nonexistent_shader.glsl", "shader");

    if (!result) {
        printf("FAILED: Could not create validation result for nonexistent shader\n");
        return qfalse;
    }

    if (result->overall_result != VALIDATION_CRITICAL) {
        printf("FAILED: Nonexistent shader should result in critical validation error\n");
        return qfalse;
    }

    printf("PASSED: Shader validation handles nonexistent files correctly\n");
    return qtrue;
}

static qboolean test_validation_statistics(void) {
    printf("Testing validation statistics...\n");

    // Validate a few assets to generate statistics
    AssetValidation_ValidateAsset("test_texture.png", "texture");
    AssetValidation_ValidateAsset("test_model.obj", "model");

    if (asset_validation.total_assets_validated != 2) {
        printf("FAILED: Total assets validated count incorrect\n");
        return qfalse;
    }

    if (asset_validation.assets_critical != 2) {
        printf("FAILED: Critical assets count incorrect\n");
        return qfalse;
    }

    printf("PASSED: Validation statistics tracking works correctly\n");
    return qtrue;
}

static qboolean test_validation_reporting(void) {
    printf("Testing validation reporting...\n");

    if (!AssetValidation_GenerateReport("test_validation_report.txt", "text")) {
        printf("FAILED: Could not generate text validation report\n");
        return qfalse;
    }

    if (!AssetValidation_GenerateReport("test_validation_report.json", "json")) {
        printf("FAILED: Could not generate JSON validation report\n");
        return qfalse;
    }

    printf("PASSED: Validation report generation works correctly\n");
    return qtrue;
}

static qboolean test_validation_utility_functions(void) {
    printf("Testing validation utility functions...\n");

    // Test result string conversion
    if (strcmp(AssetValidation_GetResultString(VALIDATION_PASS), "PASS") != 0) {
        printf("FAILED: Result string conversion incorrect\n");
        return qfalse;
    }

    // Test severity string conversion
    if (strcmp(AssetValidation_GetSeverityString(ISSUE_ERROR), "ERROR") != 0) {
        printf("FAILED: Severity string conversion incorrect\n");
        return qfalse;
    }

    // Test check string conversion
    if (strcmp(AssetValidation_GetCheckString(CHECK_CORRECTNESS), "Correctness") != 0) {
        printf("FAILED: Check string conversion incorrect\n");
        return qfalse;
    }

    // Test asset type support
    if (!AssetValidation_IsAssetTypeSupported("texture")) {
        printf("FAILED: Texture should be supported asset type\n");
        return qfalse;
    }

    if (AssetValidation_IsAssetTypeSupported("invalid")) {
        printf("FAILED: Invalid should not be supported asset type\n");
        return qfalse;
    }

    printf("PASSED: Validation utility functions work correctly\n");
    return qtrue;
}

int main(int argc, char* argv[]) {
    printf("=== Asset Validation System Tests ===\n\n");

    int tests_passed = 0;
    int total_tests = 10;

    if (test_asset_validation_initialization()) tests_passed++;
    if (test_asset_type_detection()) tests_passed++;
    if (test_validation_issue_management()) tests_passed++;
    if (test_texture_validation()) tests_passed++;
    if (test_model_validation()) tests_passed++;
    if (test_sound_validation()) tests_passed++;
    if (test_shader_validation()) tests_passed++;
    if (test_validation_statistics()) tests_passed++;
    if (test_validation_reporting()) tests_passed++;
    if (test_validation_utility_functions()) tests_passed++;

    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);

    if (tests_passed == total_tests) {
        printf("ALL TESTS PASSED!\n");
    } else {
        printf("SOME TESTS FAILED!\n");
    }

    // Cleanup
    AssetValidation_Shutdown();

    return (tests_passed == total_tests) ? 0 : 1;
}
