/*
===========================================================================
Minimal Test Framework - No engine dependencies
===========================================================================
*/

#ifndef __TEST_FRAMEWORK_MINIMAL_H__
#define __TEST_FRAMEWORK_MINIMAL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Minimal type definitions
typedef int qboolean;
#define qtrue 1
#define qfalse 0
typedef unsigned char byte;

// Test statistics
static int test_count __attribute__((unused)) = 0;
static int test_passed __attribute__((unused)) = 0;
static int test_failed __attribute__((unused)) = 0;

// Com_Printf declaration (implementation provided by test or impl file)
void Com_Printf(const char *fmt, ...);

// Test macro
#define TEST(name) \
    static void test_##name(void); \
    static void test_##name(void)

// Assertion macros
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

#define ASSERT_STR_EQ(a, b) \
    do { \
        test_count++; \
        if (strcmp((a), (b)) != 0) { \
            printf("FAIL: %s:%d: Expected \"%s\", got \"%s\"\n", \
                __func__, __LINE__, (b), (a)); \
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

#define ASSERT_FALSE(condition) \
    do { \
        test_count++; \
        if (condition) { \
            printf("FAIL: %s:%d: Expected false\n", \
                __func__, __LINE__); \
            test_failed++; \
            return; \
        } \
        test_passed++; \
    } while(0)

#define PASS() do {} while(0)

#define RUN_TEST(name) \
    do { \
        printf("Running test: %s\n", #name); \
        test_##name(); \
    } while(0)

#endif // __TEST_FRAMEWORK_MINIMAL_H__
