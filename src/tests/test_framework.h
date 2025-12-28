/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef __TEST_FRAMEWORK_H__
#define __TEST_FRAMEWORK_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/q_shared.h"

// Test statistics
static int test_count __attribute__((unused)) = 0;
static int test_passed __attribute__((unused)) = 0;
static int test_failed __attribute__((unused)) = 0;
static const char *current_test_name __attribute__((unused)) = NULL;

// Test macro
#define TEST(name) \
	static void test_##name(void); \
	static void test_##name(void)

// Assertion macros
#define ASSERT_EQ(a, b) \
	do { \
		test_count++; \
		if ((a) != (b)) { \
		Com_Printf("FAIL: %s:%d: Expected %d, got %d\n", \
			__func__, __LINE__, (int)(b), (int)(a)); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_NE(a, b) \
	do { \
		test_count++; \
		if ((a) == (b)) { \
		Com_Printf("FAIL: %s:%d: Expected not equal, got %d\n", \
			__func__, __LINE__, (int)(a)); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_STR_EQ(a, b) \
	do { \
		test_count++; \
		if (strcmp((a), (b)) != 0) { \
		Com_Printf("FAIL: %s:%d: Expected \"%s\", got \"%s\"\n", \
			__func__, __LINE__, (b), (a)); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_NOT_NULL(ptr) \
	do { \
		test_count++; \
		if ((ptr) == NULL) { \
			Com_Printf("FAIL: %s:%d: Expected non-NULL pointer\n", \
				__func__, __LINE__); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_NULL(ptr) \
	do { \
		test_count++; \
		if ((ptr) != NULL) { \
			Com_Printf("FAIL: %s:%d: Expected NULL pointer\n", \
				__func__, __LINE__); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_FLOAT_EQ(a, b, tolerance) \
	do { \
		test_count++; \
		if (fabsf((a) - (b)) > (tolerance)) { \
			Com_Printf("FAIL: %s:%d: Expected %.9f, got %.9f (diff: %.9f)\n", \
				__func__, __LINE__, (float)(b), (float)(a), fabsf((a) - (b))); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_TRUE(condition) \
	do { \
		test_count++; \
		if (!(condition)) { \
			Com_Printf("FAIL: %s:%d: Expected true\n", \
				__func__, __LINE__); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_FALSE(condition) \
	do { \
		test_count++; \
		if (condition) { \
			Com_Printf("FAIL: %s:%d: Expected false\n", \
				__func__, __LINE__); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

// Test runner
#define RUN_TEST(name) \
	do { \
		current_test_name = #name; \
		Com_Printf("Running test: %s\n", #name); \
		test_##name(); \
	} while(0)

// Pass macro (no-op, just for explicit test completion)
#define PASS() do {} while(0)

// Print test summary
#define PRINT_TEST_SUMMARY() \
	do { \
		Com_Printf("\n=== Test Summary ===\n"); \
		Com_Printf("Total: %d\n", test_count); \
		Com_Printf("Passed: %d\n", test_passed); \
		Com_Printf("Failed: %d\n", test_failed); \
		if (test_failed == 0) { \
			Com_Printf("All tests passed!\n"); \
		} \
	} while(0)

#endif // __TEST_FRAMEWORK_H__

