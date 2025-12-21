/*
===========================================================================
String Safety Unit Tests

Tests for safe string operations and bounds checking
===========================================================================
*/

#include "test_framework.h"
#include "../src/common/q_shared.h"
#include "../src/common/qcommon.h"
#include <string.h>
#include <stdlib.h>

// Mock Com_Printf for testing
void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

// Mock Com_Error for testing
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;  // Unused parameter
	va_list argptr;
	va_start(argptr, error);
	vfprintf(stderr, error, argptr);
	va_end(argptr);
	fprintf(stderr, "\n");
	exit(1);
}

// Stub implementation of Q_ValidateFilePath for testing
qboolean Q_ValidateFilePath(const char *path) {
	const char *p;

	if (!path || !*path) {
		return qfalse;
	}

	// Check for directory traversal patterns
	for (p = path; *p; p++) {
		// Check for "../" or "..\" patterns
		if ((p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\\')) ||
		    (p[0] == '.' && p[1] == '.' && p[2] == '\0')) {
			return qfalse;
		}
	}

	return qtrue;
}

// Stub implementation of Q_atof for testing
float Q_atof(const char *str) {
	return atof(str);
}

TEST(Q_strncpyz_basic) {
	char dest[32];

	// Basic copy
	Q_strncpyz(dest, "hello", sizeof(dest));
	ASSERT_STR_EQ(dest, "hello");

	// Copy with null termination
	Q_strncpyz(dest, "world", 4);
	ASSERT_STR_EQ(dest, "wor");

	// Empty string
	Q_strncpyz(dest, "", sizeof(dest));
	ASSERT_STR_EQ(dest, "");
}

TEST(Q_strncpyz_bounds_checking) {
	char dest[10];

	// Test truncation
	Q_strncpyz(dest, "verylongstring", sizeof(dest));
	ASSERT_STR_EQ(dest, "verylongs");

	// Test with exact size
	Q_strncpyz(dest, "exactly9ch", sizeof(dest));
	ASSERT_STR_EQ(dest, "exactly9ch");
}

TEST(Q_strncpyz_null_handling) {
	char dest[32];

	// NULL source
	Q_strncpyz(dest, NULL, sizeof(dest));
	// Should set dest to empty string (implementation dependent)

	// NULL destination (should not crash)
	Q_strncpyz(NULL, "test", 0);
}

TEST(Com_sprintf_basic) {
	char dest[64];
	int len;

	// Basic formatting
	len = Com_sprintf(dest, sizeof(dest), "hello %s", "world");
	ASSERT_EQ(len, 11);
	ASSERT_STR_EQ(dest, "hello world");

	// Integer formatting
	len = Com_sprintf(dest, sizeof(dest), "value: %d", 42);
	ASSERT_EQ(len, 9);
	ASSERT_STR_EQ(dest, "value: 42");
}

TEST(Com_sprintf_bounds_checking) {
	char dest[10];
	int len;

	// Test truncation
	len = Com_sprintf(dest, sizeof(dest), "verylongstring");
	ASSERT_EQ(len, 9); // Should be truncated
	ASSERT_STR_EQ(dest, "verylongs");
}

TEST(Com_sprintf_overflow_protection) {
	char dest[10];
	int len;

	// Test that overflow is detected
	len = Com_sprintf(dest, sizeof(dest), "%s%s%s", "aaa", "bbb", "ccc");
	// Should truncate safely
	ASSERT_TRUE(len >= 0);
	ASSERT_TRUE(strlen(dest) < sizeof(dest));
}

TEST(Q_SafeAtoi_basic) {
	int result;
	qboolean error;

	// Valid integers
	result = Q_SafeAtoi("123", 0, &error);
	ASSERT_EQ(result, 123);
	ASSERT_FALSE(error);

	result = Q_SafeAtoi("-456", 0, &error);
	ASSERT_EQ(result, -456);
	ASSERT_FALSE(error);
}

TEST(Q_SafeAtoi_edge_cases) {
	int result;
	qboolean error;

	// Empty string
	result = Q_SafeAtoi("", 42, &error);
	ASSERT_EQ(result, 42);
	ASSERT_TRUE(error);

	// Invalid characters
	result = Q_SafeAtoi("123abc", 0, &error);
	ASSERT_EQ(result, 0);
	ASSERT_TRUE(error);

	// Overflow protection (simplified test)
	result = Q_SafeAtoi("999999999999", 0, &error);
	// Should handle gracefully
	ASSERT_TRUE(error || result != 0);
}

TEST(Q_ValidateFilePath_basic) {
	// Valid paths
	ASSERT_TRUE(Q_ValidateFilePath("models/player.md3"));
	ASSERT_TRUE(Q_ValidateFilePath("textures/base/wall01.tga"));

	// Invalid paths with traversal
	ASSERT_FALSE(Q_ValidateFilePath("../escape"));
	ASSERT_FALSE(Q_ValidateFilePath("../../../etc/passwd"));
	ASSERT_FALSE(Q_ValidateFilePath("folder/../../../root"));
}

TEST(Q_ValidateFilePath_edge_cases) {
	// NULL input
	ASSERT_FALSE(Q_ValidateFilePath(NULL));

	// Empty string
	ASSERT_TRUE(Q_ValidateFilePath(""));

	// Absolute paths
	ASSERT_FALSE(Q_ValidateFilePath("/etc/passwd"));
	ASSERT_FALSE(Q_ValidateFilePath("C:\\Windows\\system32"));
}

TEST(string_safety_integration) {
	char buffer[32];
	int result;
	qboolean error;

	// Test integrated string operations
	Q_strncpyz(buffer, "42", sizeof(buffer));
	result = Q_SafeAtoi(buffer, 0, &error);
	ASSERT_FALSE(error); // Should not error on valid string
	ASSERT_EQ(result, 42); // Should parse correctly

	// Should work correctly
	ASSERT_TRUE(Q_ValidateFilePath("safe/path.tga"));
	ASSERT_FALSE(Q_ValidateFilePath("../unsafe"));
}

// Test runner
int main(int argc, char **argv) {
	(void)argc; (void)argv;

	RUN_TEST(Q_strncpyz_basic);
	RUN_TEST(Q_strncpyz_bounds_checking);
	RUN_TEST(Q_strncpyz_null_handling);
	RUN_TEST(Com_sprintf_basic);
	RUN_TEST(Com_sprintf_bounds_checking);
	RUN_TEST(Com_sprintf_overflow_protection);
	RUN_TEST(Q_SafeAtoi_basic);
	RUN_TEST(Q_SafeAtoi_edge_cases);
	RUN_TEST(Q_ValidateFilePath_basic);
	RUN_TEST(Q_ValidateFilePath_edge_cases);
	RUN_TEST(string_safety_integration);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}
