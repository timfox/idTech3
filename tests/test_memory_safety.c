/*
===========================================================================
Memory Safety Tests - Standalone version
===========================================================================
*/

#include "test_framework_minimal.h"
#include "q_memory_safety_test.h"

// Track last error for assertions
static char g_last_error[1024] = {0};

// Tests
TEST(q_strncpyz_safe_basic) {
    char dest[32];

    ASSERT_TRUE(Q_strncpyz_safe(dest, "hello", sizeof(dest), "test"));
    ASSERT_STR_EQ(dest, "hello");

    PASS();
}

TEST(q_strncpyz_safe_null_dest) {
    ASSERT_FALSE(Q_strncpyz_safe(NULL, "hello", 32, "test"));
    PASS();
}

TEST(q_strncpyz_safe_null_src) {
    char dest[32];

    ASSERT_TRUE(Q_strncpyz_safe(dest, NULL, sizeof(dest), "test"));
    ASSERT_STR_EQ(dest, "");
    PASS();
}

TEST(q_strncpyz_safe_truncation) {
    char dest[6]; // Room for "hello" + null

    ASSERT_TRUE(Q_strncpyz_safe(dest, "hello world", sizeof(dest), "test"));
    ASSERT_STR_EQ(dest, "hello");
    PASS();
}

TEST(q_strcat_safe_basic) {
    char dest[32];

    strcpy(dest, "hello");
    ASSERT_TRUE(Q_strcat_safe(dest, " world", sizeof(dest), "test"));
    ASSERT_STR_EQ(dest, "hello world");

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

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    RUN_TEST(q_strncpyz_safe_basic);
    RUN_TEST(q_strncpyz_safe_null_dest);
    RUN_TEST(q_strncpyz_safe_null_src);
    RUN_TEST(q_strncpyz_safe_truncation);
    RUN_TEST(q_strcat_safe_basic);
    RUN_TEST(q_snprintf_safe_basic);

    printf("\nTests: %d passed, %d failed\n", test_passed, test_failed);
    return test_failed > 0 ? 1 : 0;
}
