/*
===========================================================================
Secure Message Functions Test

Tests the enhanced message handling with bounds checking.
===========================================================================
*/

#include "test_framework.h"
#include "../common/msg_secure.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Mock stubs for external dependencies
cvar_t *cl_shownet = NULL;

void Com_Printf( const char *fmt, ... ) {
	va_list argptr;
	va_start( argptr, fmt );
	vprintf( fmt, argptr );
	va_end( argptr );
}

void Com_Error(errorParm_t level, const char *error, ...) {
    (void)level;
    (void)error;
	// This is declared noreturn in the engine; keep that contract.
	abort();
}

// Test buffer
#define TEST_BUFFER_SIZE 1024
static byte test_buffer[TEST_BUFFER_SIZE];
static msg_t test_msg;

// Test functions
TEST(msg_secure_init) {
    MSG_Init(&test_msg, test_buffer, TEST_BUFFER_SIZE);
    ASSERT_TRUE(MSG_ValidateState(&test_msg, "init"));
    ASSERT_EQ(test_msg.maxsize, TEST_BUFFER_SIZE);
    ASSERT_EQ(test_msg.cursize, 0);

    PASS();
}

TEST(msg_secure_write_bits_bounds) {
    MSG_Init(&test_msg, test_buffer, TEST_BUFFER_SIZE);

    // This should work - plenty of space
    MSG_WriteBits_Secure(&test_msg, 42, 8, "test_write");
    ASSERT_EQ(test_msg.cursize, 1);

    // Fill up the message
    while (MSG_HasBits(&test_msg, 8, "fill_test")) {
        MSG_WriteBits_Secure(&test_msg, 0xFF, 8, "fill_test");
    }

    // This should fail - no space left
    int original_size = test_msg.cursize;
    MSG_WriteBits_Secure(&test_msg, 42, 8, "overflow_test");
    ASSERT_EQ(test_msg.cursize, original_size);  // Should not have changed

    PASS();
}

TEST(msg_secure_read_bits_bounds) {
    MSG_Init(&test_msg, test_buffer, TEST_BUFFER_SIZE);

    // Write some data
    MSG_WriteBits_Secure(&test_msg, 42, 8, "write_test");
    MSG_WriteBits_Secure(&test_msg, 123, 8, "write_test");

    // Reset for reading
    MSG_BeginReading(&test_msg);

    // Read the data back
    int val1 = MSG_ReadBits_Secure(&test_msg, 8, "read_test");
    int val2 = MSG_ReadBits_Secure(&test_msg, 8, "read_test");

    ASSERT_EQ(val1, 42);
    ASSERT_EQ(val2, 123);

    // Try to read past end - should return 0
    int val3 = MSG_ReadBits_Secure(&test_msg, 8, "overflow_read");
    ASSERT_EQ(val3, 0);

    PASS();
}

TEST(msg_secure_string_operations) {
    MSG_Init(&test_msg, test_buffer, TEST_BUFFER_SIZE);

    const char *test_string = "Hello, World!";
    char read_buffer[50];

    // Write string
    MSG_WriteString_Secure(&test_msg, test_string, "write_string");

    // Reset for reading
    MSG_BeginReading(&test_msg);

    // Read string
    ASSERT_TRUE(MSG_ReadString_Secure(&test_msg, read_buffer, sizeof(read_buffer), "read_string"));
    ASSERT_STR_EQ(read_buffer, test_string);

    PASS();
}

TEST(msg_secure_data_operations) {
    MSG_Init(&test_msg, test_buffer, TEST_BUFFER_SIZE);

    const byte test_data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    byte read_data[sizeof(test_data)];

    // Write data
    MSG_WriteData_Secure(&test_msg, test_data, sizeof(test_data), "write_data");

    // Reset for reading
    MSG_BeginReading(&test_msg);

    // Read data
    ASSERT_TRUE(MSG_ReadData_Secure(&test_msg, read_data, sizeof(read_data), "read_data"));

    for (size_t i = 0; i < sizeof(test_data); i++) {
        ASSERT_EQ(read_data[i], test_data[i]);
    }

    PASS();
}

TEST(msg_secure_bounds_validation) {
    MSG_Init(&test_msg, test_buffer, TEST_BUFFER_SIZE);

    // Test MSG_HasSpace
    ASSERT_TRUE(MSG_HasSpace(&test_msg, 100, "space_test"));
    ASSERT_FALSE(MSG_HasSpace(&test_msg, TEST_BUFFER_SIZE + 1, "overflow_test"));

    // Test MSG_HasBits
    ASSERT_TRUE(MSG_HasBits(&test_msg, 32, "bits_test"));
    ASSERT_FALSE(MSG_HasBits(&test_msg, 1000000, "invalid_bits"));

    // Test MSG_ValidateState
    ASSERT_TRUE(MSG_ValidateState(&test_msg, "state_test"));

    PASS();
}

TEST(msg_secure_string_truncation) {
    MSG_Init(&test_msg, test_buffer, TEST_BUFFER_SIZE);

    // Create a very long string
    char long_string[2000];
    memset(long_string, 'A', sizeof(long_string) - 1);
    long_string[sizeof(long_string) - 1] = '\0';

    char read_buffer[100];

    // Write the long string (should work)
    MSG_WriteString_Secure(&test_msg, long_string, "long_string");

    // Reset for reading
    MSG_BeginReading(&test_msg);

    // Read with small buffer (should truncate safely)
    ASSERT_TRUE(MSG_ReadString_Secure(&test_msg, read_buffer, sizeof(read_buffer), "truncate_test"));
    ASSERT_EQ(strlen(read_buffer), sizeof(read_buffer) - 1);  // Should be truncated

    PASS();
}

// Test main function
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // Run tests
    RUN_TEST(msg_secure_init);
    RUN_TEST(msg_secure_write_bits_bounds);
    RUN_TEST(msg_secure_read_bits_bounds);
    RUN_TEST(msg_secure_string_operations);
    RUN_TEST(msg_secure_data_operations);
    RUN_TEST(msg_secure_bounds_validation);
    RUN_TEST(msg_secure_string_truncation);

    return 0;
}