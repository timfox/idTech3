/*
===========================================================================
Scalability System Unit Tests

Tests for dynamic limits and hardware-based scaling
===========================================================================
*/

#include "test_framework.h"
#include "../src/common/q_scalability.h"
#include <stdlib.h>

// Mock system info for testing
static int mockPhysicalMemoryMB = 4096; // 4GB default
static int mockNumCPUCores = 4; // 4 cores default

// Mock implementations
static int Sys_GetPhysicalMemoryMB(void) {
	return mockPhysicalMemoryMB;
}

static int Sys_GetNumCPUCores(void) {
	return mockNumCPUCores;
}

TEST(scalability_initialization) {
	// Test basic initialization
	Scalability_Init();

	// Should have reasonable defaults
	int maxModels = Scalability_GetMaxModels();
	int maxShaders = Scalability_GetMaxShaders();
	int maxFonts = Scalability_GetMaxFonts();

	ASSERT_TRUE(maxModels > 0);
	ASSERT_TRUE(maxShaders > 0);
	ASSERT_TRUE(maxFonts > 0);

	Scalability_Shutdown();
}

TEST(scalability_auto_detection_high_end) {
	// Test with high-end hardware
	mockPhysicalMemoryMB = 16384; // 16GB
	mockNumCPUCores = 8; // 8 cores

	Scalability_AutoDetect();

	int maxModels = Scalability_GetMaxModels();
	int maxShaders = Scalability_GetMaxShaders();

	// Should scale up for high-end hardware
	ASSERT_TRUE(maxModels > DEFAULT_MAX_MODELS);
	ASSERT_TRUE(maxShaders > DEFAULT_MAX_SHADERS);

	Scalability_Shutdown();
}

TEST(scalability_auto_detection_low_end) {
	// Test with low-end hardware
	mockPhysicalMemoryMB = 1024; // 1GB
	mockNumCPUCores = 2; // 2 cores

	Scalability_AutoDetect();

	int maxModels = Scalability_GetMaxModels();

	// Should scale down for low-end hardware but maintain minimums
	ASSERT_TRUE(maxModels >= 256); // Minimum should be maintained
	ASSERT_TRUE(maxModels <= DEFAULT_MAX_MODELS); // Should not exceed default

	Scalability_Shutdown();
}

TEST(scalability_custom_limits) {
	Scalability_Init();

	// Test setting custom limits
	Scalability_SetMaxModels(2048);
	Scalability_SetMaxShaders(4096);
	Scalability_SetMaxFonts(32);

	ASSERT_EQ(Scalability_GetMaxModels(), 2048);
	ASSERT_EQ(Scalability_GetMaxShaders(), 4096);
	ASSERT_EQ(Scalability_GetMaxFonts(), 32);

	// Test allocation checks
	ASSERT_TRUE(Scalability_CanAllocateModel());
	ASSERT_TRUE(Scalability_CanAllocateShader());
	ASSERT_TRUE(Scalability_CanAllocateFont());

	Scalability_Shutdown();
}

TEST(scalability_limit_clamping) {
	Scalability_Init();

	// Test that limits are clamped to reasonable ranges
	Scalability_SetMaxModels(MAX_MODELS_LIMIT + 1000); // Too high
	Scalability_SetMaxModels(10); // Too low

	// Should be clamped to valid range
	int maxModels = Scalability_GetMaxModels();
	ASSERT_TRUE(maxModels >= 64 && maxModels <= MAX_MODELS_LIMIT);

	Scalability_Shutdown();
}

// Asset loader tests disabled for now due to extern issues

// Test main function
int main(int argc, char *argv[]) {
	(void)argc; (void)argv;

	RUN_TEST(scalability_initialization);
	RUN_TEST(scalability_auto_detection_high_end);
	RUN_TEST(scalability_auto_detection_low_end);
	RUN_TEST(scalability_custom_limits);
	RUN_TEST(scalability_limit_clamping);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}
