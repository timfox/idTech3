/*
===========================================================================
Scalability System Unit Tests

Tests for dynamic limits and hardware-based scaling
===========================================================================
*/

#include "test_framework.h"
#include "../src/common/q_scalability.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

// Mock system info for testing
static int mockPhysicalMemoryMB = 4096; // 4GB default
static int mockNumCPUCores = 4; // 4 cores default

// Mock function prototypes
int Sys_GetPhysicalMemoryMB(void);
void Com_Error(errorParm_t level, const char *fmt, ...);
void Com_Printf(const char *fmt, ...);
int Sys_GetNumCPUCores(void);
void Cmd_CommandCompletion(void (*callback)(const char *s));
void Cvar_CommandCompletion(void (*callback)(const char *s));
void Sys_RandomBytes(byte *data, int size);
cvar_t *Cvar_Get(const char *name, const char *value, int flags);
void Z_Free(void *ptr);
float Q_atof(const char *str);
qboolean Q_Log_IsEnabled(log_level_t level, log_category_t category);
void Q_Log_Flush(void);
void FS_ForceFlush(qhandle_t f);
void Crash_LogMessage(const char *msg);
qboolean FS_Initialized(void);
qboolean FS_StartupInProgress(void);
void Q_Log_ComPrintf(log_level_t level, const char *fmt, ...);
void CL_ConsolePrint(const char *msg);
void Sys_Print(const char *msg);

// Mock implementations
int Sys_GetPhysicalMemoryMB(void) {
	return mockPhysicalMemoryMB;
}

void Com_Error([[maybe_unused]] errorParm_t level, const char *fmt, ...) {
	// Mock Com_Error - just exit the test
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
	printf("\n");
	exit(1); // Exit with failure
}

void Com_Printf(const char *fmt, ...) {
	// Mock Com_Printf - just print to stdout
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

int Sys_GetNumCPUCores(void) {
	return mockNumCPUCores;
}

// Additional mocks for linking
void Cmd_CommandCompletion(void (*callback)(const char *s)) {
	(void)callback; // Mock - do nothing
}

void Cvar_CommandCompletion(void (*callback)(const char *s)) {
	(void)callback; // Mock - do nothing
}

void Sys_RandomBytes(byte *data, int size) {
	(void)data; (void)size; // Mock - do nothing
}

// Mocks for q_scalability.c dependencies
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
	(void)name; (void)value; (void)flags;
	// Return a mock cvar
	static cvar_t mock_cvar;
	mock_cvar.integer = atoi(value);
	mock_cvar.value = atof(value);
	return &mock_cvar;
}

void Z_Free(void *ptr) {
	free(ptr); // Use standard free for testing
}

// Additional mocks for all dependencies
float Q_atof(const char *str) {
	return atof(str);
}

qboolean Q_Log_IsEnabled(log_level_t level, log_category_t category) {
	(void)level; (void)category;
	return qtrue; // Enable all logging for tests
}

void Q_Log_Flush(void) {
	// Mock - do nothing
}

void FS_ForceFlush(qhandle_t f) {
	(void)f; // Mock - do nothing
}

void Crash_LogMessage(const char *msg) {
	(void)msg; // Mock - do nothing
}

qboolean FS_Initialized(void) {
	return qtrue; // Mock - assume initialized
}

qboolean FS_StartupInProgress(void) {
	return qfalse; // Mock - not in progress
}

void Q_Log_ComPrintf(log_level_t level, const char *fmt, ...) {
	(void)level; (void)fmt; // Mock - do nothing
}

void CL_ConsolePrint(const char *msg) {
	(void)msg; // Mock - do nothing
}

void Sys_Print(const char *msg) {
	(void)msg; // Mock - do nothing
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
