/*
=============================================================================
Automated Packaging System Test

Multi-platform installer and package generation tests.
=============================================================================
*/

#include "q_shared.h"
#include "package_generator.h"
#include <stdio.h>
#include <stdlib.h>

static qboolean test_package_generator_initialization(void) {
    printf("Testing automated packaging system initialization...\n");

    if (!PackageGenerator_Init()) {
        printf("FAILED: Could not initialize automated packaging system\n");
        return qfalse;
    }

    if (!packaging_system.initialized) {
        printf("FAILED: Automated packaging system not marked as initialized\n");
        return qfalse;
    }

    printf("PASSED: Automated packaging system initialized successfully\n");
    return qtrue;
}

static qboolean test_platform_detection(void) {
    printf("Testing platform and architecture detection...\n");

    package_platform_t platform = PackageGenerator_DetectCurrentPlatform();
    package_architecture_t arch = PackageGenerator_DetectCurrentArchitecture();

    if (platform == PACKAGE_PLATFORM_COUNT) {
        printf("FAILED: Could not detect current platform\n");
        return qfalse;
    }

    if (arch == PACKAGE_ARCH_COUNT) {
        printf("FAILED: Could not detect current architecture\n");
        return qfalse;
    }

    printf("PASSED: Platform detection works (Platform: %s, Arch: %s)\n",
           PackageGenerator_GetPlatformName(platform),
           PackageGenerator_GetArchitectureName(arch));
    return qtrue;
}

static qboolean test_tool_detection(void) {
    printf("Testing packaging tool detection...\n");

    if (!PackageGenerator_DetectAvailableTools()) {
        printf("FAILED: Could not detect available tools\n");
        return qfalse;
    }

    // Check that at least some basic tools are detected
    qboolean has_any_tools = packaging_system.has_nsis || packaging_system.has_wix ||
                           packaging_system.has_dpkg_deb || packaging_system.has_rpmbuild ||
                           packaging_system.has_appimagetool || packaging_system.has_hdiutil ||
                           packaging_system.has_pkgbuild;

    if (!has_any_tools) {
        printf("WARNING: No packaging tools detected - this may be expected in test environment\n");
    }

    printf("PASSED: Tool detection completed\n");
    return qtrue;
}

static qboolean test_configuration_management(void) {
    printf("Testing package configuration management...\n");

    package_config_t* config = PackageGenerator_CreateConfig();
    if (!config) {
        printf("FAILED: Could not create package configuration\n");
        return qfalse;
    }

    // Test configuration modification
    Q_strncpyz(config->package_name, "test_package", sizeof(config->package_name));
    Q_strncpyz(config->package_version, "1.0.0", sizeof(config->package_version));

    // Test configuration retrieval
    const package_config_t* retrieved = PackageGenerator_GetDefaultConfig();
    if (!retrieved) {
        printf("FAILED: Could not retrieve default configuration\n");
        PackageGenerator_DestroyConfig(config);
        return qfalse;
    }

    // Test setting default configuration
    PackageGenerator_SetDefaultConfig(config);

    // Test configuration destruction
    PackageGenerator_DestroyConfig(config);

    printf("PASSED: Configuration management works correctly\n");
    return qtrue;
}

static qboolean test_package_type_detection(void) {
    printf("Testing package type and utility functions...\n");

    // Test package type names
    for (int i = 0; i < PACKAGE_TYPE_COUNT; i++) {
        const char* name = PackageGenerator_GetPackageTypeName((package_type_t)i);
        if (!name || strcmp(name, "Unknown") == 0) {
            printf("FAILED: Invalid package type name for type %d\n", i);
            return qfalse;
        }
    }

    // Test platform names
    for (int i = 0; i < PACKAGE_PLATFORM_COUNT; i++) {
        const char* name = PackageGenerator_GetPlatformName((package_platform_t)i);
        if (!name || strcmp(name, "Unknown") == 0) {
            printf("FAILED: Invalid platform name for platform %d\n", i);
            return qfalse;
        }
    }

    // Test architecture names
    for (int i = 0; i < PACKAGE_ARCH_COUNT; i++) {
        const char* name = PackageGenerator_GetArchitectureName((package_architecture_t)i);
        if (!name || strcmp(name, "Unknown") == 0) {
            printf("FAILED: Invalid architecture name for arch %d\n", i);
            return qfalse;
        }
    }

    // Test package extensions
    char extension[16];
    if (!PackageGenerator_GetPackageExtension(PACKAGE_TYPE_ZIP, extension, sizeof(extension))) {
        printf("FAILED: Could not get package extension\n");
        return qfalse;
    }

    if (strcmp(extension, ".zip") != 0) {
        printf("FAILED: Incorrect package extension: %s\n", extension);
        return qfalse;
    }

    printf("PASSED: Package type detection and utility functions work correctly\n");
    return qtrue;
}

static qboolean test_package_generation_attempt(void) {
    printf("Testing package generation (basic functionality)...\n");

    // Create a basic configuration
    package_config_t config = packaging_system.default_config;
    Q_strncpyz(config.package_name, "test_package", sizeof(config.package_name));
    Q_strncpyz(config.package_version, "1.0.0", sizeof(config.package_version));
    Q_strncpyz(config.source_directory, ".", sizeof(config.source_directory)); // Use current directory
    Q_strncpyz(config.output_directory, "./test_packages", sizeof(config.output_directory));

    // Try to generate a simple ZIP package
    config.preferred_types[0] = PACKAGE_TYPE_ZIP;
    config.num_preferred_types = 1;
    config.target_platform = PACKAGE_PLATFORM_UNIVERSAL;

    package_generation_result_t* result = PackageGenerator_GeneratePackage(&config);

    if (!result) {
        printf("FAILED: Package generation returned null result\n");
        return qfalse;
    }

    // Check basic result structure
    if (strcmp(result->package_name, "test_package") != 0) {
        printf("FAILED: Package name not set correctly\n");
        return qfalse;
    }

    if (result->generation_time_ms == 0) {
        printf("FAILED: Generation time not recorded\n");
        return qfalse;
    }

    printf("PASSED: Package generation attempt completed (Result: %s)\n",
           PackageGenerator_GetResultString(result->result));
    return qtrue;
}

static qboolean test_reporting_system(void) {
    printf("Testing reporting system...\n");

    if (!PackageGenerator_GenerateReport("test_package_report.txt", "text")) {
        printf("FAILED: Could not generate text report\n");
        return qfalse;
    }

    if (!PackageGenerator_GenerateReport("test_package_report.json", "json")) {
        printf("FAILED: Could not generate JSON report\n");
        return qfalse;
    }

    printf("PASSED: Reporting system works correctly\n");
    return qtrue;
}

static qboolean test_statistics_tracking(void) {
    printf("Testing statistics tracking...\n");

    // Get initial counts
    uint32_t initial_total = packaging_system.total_packages_generated;

    // Generate another package to test statistics
    package_config_t config = packaging_system.default_config;
    Q_strncpyz(config.package_name, "stats_test", sizeof(config.package_name));
    PackageGenerator_GeneratePackage(&config);

    // Check that statistics were updated
    if (packaging_system.total_packages_generated <= initial_total) {
        printf("FAILED: Package generation statistics not updated\n");
        return qfalse;
    }

    printf("PASSED: Statistics tracking works correctly (%u total packages)\n",
           packaging_system.total_packages_generated);
    return qtrue;
}

static qboolean test_result_management(void) {
    printf("Testing result management...\n");

    package_generation_result_t* results;
    uint32_t count = PackageGenerator_GetResults(&results);

    if (count == 0) {
        printf("FAILED: No package generation results available\n");
        return qfalse;
    }

    // Test getting result by name
    package_generation_result_t* result = PackageGenerator_GetResult("test_package");
    if (!result) {
        printf("FAILED: Could not retrieve result by name\n");
        return qfalse;
    }

    printf("PASSED: Result management works correctly (%u results available)\n", count);
    return qtrue;
}

int main(int argc, char* argv[]) {
    printf("=== Automated Packaging System Tests ===\n\n");

    int tests_passed = 0;
    int total_tests = 9;

    if (test_package_generator_initialization()) tests_passed++;
    if (test_platform_detection()) tests_passed++;
    if (test_tool_detection()) tests_passed++;
    if (test_configuration_management()) tests_passed++;
    if (test_package_type_detection()) tests_passed++;
    if (test_package_generation_attempt()) tests_passed++;
    if (test_reporting_system()) tests_passed++;
    if (test_statistics_tracking()) tests_passed++;
    if (test_result_management()) tests_passed++;

    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);

    if (tests_passed == total_tests) {
        printf("ALL TESTS PASSED!\n");
    } else {
        printf("SOME TESTS FAILED!\n");
    }

    // Cleanup
    PackageGenerator_Shutdown();

    return (tests_passed == total_tests) ? 0 : 1;
}
