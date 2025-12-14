/*
===========================================================================
Performance Counters Tests
===========================================================================
*/

#include "../src/qcommon/performance_counters.h"
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Forward declarations for functions defined in performance_counters.c
void Com_Printf(const char *fmt, ...);

// Types are defined in performance_counters.h for standalone testing

// Minimal test framework for standalone testing
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define ASSERT_EQ(a, b) \
	do { \
		test_count++; \
		if ((a) != (b)) { \
			printf("FAIL: %s:%d: Expected %d, got %d\n", \
				__func__, __LINE__, (int)(b), (int)(a)); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_TRUE(condition) \
	do { \
		test_count++; \
		if (!(condition)) { \
			printf("FAIL: %s:%d: Expected true\n", \
				__func__, __LINE__); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name) \
	do { \
		printf("Running test: %s\n", #name); \
		test_##name(); \
	} while(0)

#define PRINT_TEST_SUMMARY() \
	do { \
		printf("\n=== Test Summary ===\n"); \
		printf("Total: %d\n", test_count); \
		printf("Passed: %d\n", test_passed); \
		printf("Failed: %d\n", test_failed); \
		if (test_failed == 0) { \
			printf("All tests passed!\n"); \
		} \
	} while(0)

// Types are defined in performance_counters.h for standalone testing

TEST(performance_counters_initialization) {
	Perf_Init();

	// Check initial state
	ASSERT_EQ(perfCounters.frameCount, 0);
	ASSERT_EQ(perfCounters.drawCallsThisFrame, 0);
	ASSERT_TRUE(perfCounters.minFrameTime > 0); // Should be initialized to a large value
}

TEST(performance_counters_frame_tracking) {
	Perf_Init();

	// Simulate a few frames
	Perf_Frame(16);  // 16ms frame
	Perf_Frame(33);  // 33ms frame
	Perf_Frame(8);   // 8ms frame

	// Check frame count
	ASSERT_EQ(perfCounters.frameCount, 3);

	// Check frame time stats
	ASSERT_TRUE(perfCounters.currentFrameTime >= 8.0f); // Last frame time
	ASSERT_EQ(perfCounters.minFrameTime, 8.0f);
	ASSERT_EQ(perfCounters.maxFrameTime, 33.0f);
}

TEST(performance_counters_draw_call_tracking) {
	Perf_Init();
	Perf_ResetFrameCounters();

	// Count some draw calls
	Perf_CountDrawCall();
	Perf_CountDrawCall();
	Perf_CountDrawCall();

	ASSERT_EQ(perfCounters.drawCallsThisFrame, 3);

	// Reset should clear per-frame counters
	Perf_ResetFrameCounters();
	ASSERT_EQ(perfCounters.drawCallsThisFrame, 0);
}

TEST(performance_counters_fps_calculation) {
	Perf_Init();

	// Simulate 60 frames over 1 second (1000ms)
	for (int i = 0; i < 60; i++) {
		Perf_Frame(16); // ~16ms per frame = ~60 FPS
	}

	// Should have calculated FPS
	ASSERT_TRUE(perfCounters.currentFPS >= 55.0f); // Allow some tolerance
	ASSERT_TRUE(perfCounters.currentFPS <= 65.0f);
}

TEST(performance_counters_average_calculation) {
	Perf_Init();

	// Add some frame times to history
	float testTimes[] = {16.0f, 17.0f, 15.0f, 16.5f, 15.5f};
	int numFrames = sizeof(testTimes) / sizeof(testTimes[0]);

	for (int i = 0; i < numFrames; i++) {
		Perf_Frame((int)testTimes[i]);
	}

	// Check that we have history
	ASSERT_TRUE(perfCounters.frameTimeHistoryCount > 0);

	// Calculate expected average
	float expectedAvg = 0.0f;
	for (int i = 0; i < numFrames; i++) {
		expectedAvg += testTimes[i];
	}
	expectedAvg /= numFrames;

	// Should be close to our expected average
	float tolerance = 0.1f;
	ASSERT_TRUE(fabsf(perfCounters.averageFrameTime - expectedAvg) < tolerance);
}

int main(void) {
	Com_Printf("Running performance counters tests...\n\n");

	RUN_TEST(performance_counters_initialization);
	RUN_TEST(performance_counters_frame_tracking);
	RUN_TEST(performance_counters_draw_call_tracking);
	RUN_TEST(performance_counters_fps_calculation);
	RUN_TEST(performance_counters_average_calculation);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}