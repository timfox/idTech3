/*
===========================================================================
Memory Safety Tests

Tests the enhanced string and buffer operations with bounds checking.
===========================================================================
*/

#include "test_framework.h"
#include "../src/qcommon/q_memory_safety.h"

// Mock Com_Printf and Com_Error for testing
static char g_last_error[1024] = {0};
static int g_error_count = 0;

void Com_Printf(const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, argptr);
    va_end(argptr);
    // Don't print to stdout in tests
}

void Com_Error(errorParm_t level, const char *error, ...) {
    va_list argptr;
    va_start(argptr, error);
    vsnprintf(g_last_error, sizeof(g_last_error), error, argptr);
    va_end(argptr);
    g_error_count++;
}

// Test functions
TEST(q_strncpyz_safe_basic) {
    char dest[32];

    ASSERT_TRUE(Q_strncpyz_safe(dest, "hello", sizeof(dest), "test"));
    ASSERT_STR_EQ(dest, "hello");

    PASS();
}

TEST(q_strncpyz_safe_null_dest) {
    ASSERT_FALSE(Q_strncpyz_safe(NULL, "hello", 32, "test"));
    ASSERT_TRUE(strstr(g_last_error, "NULL dest") != NULL);

    PASS();
}

TEST(q_strncpyz_safe_null_src) {
    char dest[32];

    ASSERT_TRUE(Q_strncpyz_safe(dest, NULL, sizeof(dest), "test"));
    ASSERT_STR_EQ(dest, "");
    ASSERT_TRUE(strstr(g_last_error, "NULL src") != NULL);

    PASS();
}

TEST(q_strncpyz_safe_truncation) {
    char dest[6]; // Room for "hello" + null

    ASSERT_TRUE(Q_strncpyz_safe(dest, "hello world", sizeof(dest), "test"));
    ASSERT_STR_EQ(dest, "hello");
    ASSERT_TRUE(strstr(g_last_error, "truncation") != NULL);

    PASS();
}

TEST(q_strcat_safe_basic) {
    char dest[32];

    strcpy(dest, "hello");
    ASSERT_TRUE(Q_strcat_safe(dest, " world", sizeof(dest), "test"));
    ASSERT_STR_EQ(dest, "hello world");

    PASS();
}

TEST(q_strcat_safe_overflow) {
    char dest[8]; // "hello" + " world" would overflow

    strcpy(dest, "hello");
    ASSERT_TRUE(Q_strcat_safe(dest, " world!!!", sizeof(dest), "test"));
    // Should be truncated
    ASSERT_TRUE(strstr(g_last_error, "truncation") != NULL);

    PASS();
}

TEST(q_snprintf_safe_basic) {
    char dest[32];
    int result;

    result = Q_snprintf_safe(dest, sizeof(dest), "test %d %s", 42, "hello");
    ASSERT_EQ(result, 13); // "test 42 hello"
    ASSERT_STR_EQ(dest, "test 42 hello");

    PASS();
}

TEST(q_snprintf_safe_truncation) {
    char dest[10];

    Q_snprintf_safe(dest, sizeof(dest), "very long string here");
    // Should be truncated and null-terminated
    ASSERT_TRUE(strlen(dest) < sizeof(dest));
    ASSERT_TRUE(strstr(g_last_error, "truncation") != NULL);

    PASS();
}

TEST(q_validate_buffer_macro) {
    char buffer[100];

    // This should pass
    Q_VALIDATE_BUFFER(buffer, 50, "test");

    // Reset error state
    g_last_error[0] = '\0';

    PASS();
}

TEST(q_static_assert_buffer_size) {
    char buffer[100];

    // This should compile without error
    Q_STATIC_ASSERT_BUFFER_SIZE(buffer, 50);

    PASS();
}

// Test main function
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // Run tests
    RUN_TEST(q_strncpyz_safe_basic);
    RUN_TEST(q_strncpyz_safe_null_dest);
    RUN_TEST(q_strncpyz_safe_null_src);
    RUN_TEST(q_strncpyz_safe_truncation);
    RUN_TEST(q_strcat_safe_basic);
    RUN_TEST(q_strcat_safe_overflow);
    RUN_TEST(q_snprintf_safe_basic);
    RUN_TEST(q_snprintf_safe_truncation);
    RUN_TEST(q_validate_buffer_macro);
    RUN_TEST(q_static_assert_buffer_size);

    return 0;
}