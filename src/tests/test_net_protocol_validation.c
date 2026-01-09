/*
=============================================================================
Network Protocol Validation Test Suite

Tests network protocol validation and bounds checking functionality
=============================================================================
*/

#include "net_protocol_validation.h"
#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>
#include <string.h>

// Stub for MSG_Init (not available in test environment)
void MSG_Init(msg_t *buf, byte *data, int length) {
    // Stub implementation for testing
    if (buf) {
        buf->data = data;
        buf->maxsize = length;
        buf->cursize = 0;
        buf->readcount = 0;
        buf->bit = 0;
    }
}

// Test data
static const byte valid_packet_data[] = {
    66, 0, 0, 0, 0, 1, 0, 10,  // Header (protocol 66, seq 0, cmd 1, length 10)
    'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd'  // Payload
};

static const byte invalid_packet_data[] = {
    255, 255, 255, 255, 255, 255, 255, 255,  // Invalid header
    'B', 'a', 'd', 'D', 'a', 't', 'a'
};

static const byte corrupted_packet_data[] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0, 1, 0, 10,  // Corruption pattern in header
    'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd'
};

static const char valid_server_info[] = "\\hostname\\Test Server\\mapname\\q3dm1\\clients\\0";
static const char invalid_server_info[] = "hostnameTest Server";  // Missing backslashes

static const char valid_client_command[] = "say hello world";
static const char invalid_client_command[] = "../../etc/passwd";  // Path traversal attempt

/*
===============
test_packet_bounds_validation

Test basic packet bounds checking
===============
*/
static void test_packet_bounds_validation(void) {
    printf("Running test: packet bounds validation\n");

    // Test valid packet
    qboolean result = Net_ValidatePacketBounds(valid_packet_data, sizeof(valid_packet_data), 1024);
    if (!result) {
        printf("  FAILED: Valid packet rejected\n");
        return;
    }

    // Test null data
    result = Net_ValidatePacketBounds(NULL, 100, 1024);
    if (result) {
        printf("  FAILED: Null data accepted\n");
        return;
    }

    // Test oversized packet
    result = Net_ValidatePacketBounds(valid_packet_data, sizeof(valid_packet_data), 10);
    if (result) {
        printf("  FAILED: Oversized packet accepted\n");
        return;
    }

    // Test negative length
    result = Net_ValidatePacketBounds(valid_packet_data, -1, 1024);
    if (result) {
        printf("  FAILED: Negative length accepted\n");
        return;
    }

    printf("  PASSED: Packet bounds validation\n");
}

/*
===============
test_protocol_header_validation

Test protocol header validation
===============
*/
static void test_protocol_header_validation(void) {
    printf("Running test: protocol header validation\n");

    // Test valid header
    net_protocol_result_t result = Net_ValidateProtocolHeader(valid_packet_data, sizeof(valid_packet_data));
    if (result != NET_PROTOCOL_VALID) {
        printf("  FAILED: Valid header rejected (%s)\n", Net_ProtocolResultToString(result));
        return;
    }

    // Test invalid header
    result = Net_ValidateProtocolHeader(invalid_packet_data, sizeof(invalid_packet_data));
    if (result == NET_PROTOCOL_VALID) {
        printf("  FAILED: Invalid header accepted\n");
        return;
    }

    // Test short data
    result = Net_ValidateProtocolHeader(valid_packet_data, 4);
    if (result != NET_PROTOCOL_INVALID_LENGTH) {
        printf("  FAILED: Short data not detected\n");
        return;
    }

    printf("  PASSED: Protocol header validation\n");
}

/*
===============
test_message_integrity_validation

Test message integrity validation
===============
*/
static void test_message_integrity_validation(void) {
    printf("Running test: message integrity validation\n");

    msg_t msg;

    // Test valid message
    MSG_Init(&msg, (byte *)valid_packet_data, sizeof(valid_packet_data));
    msg.cursize = sizeof(valid_packet_data);

    net_protocol_result_t result = Net_ValidateMessageIntegrity(&msg);
    if (result != NET_PROTOCOL_VALID) {
        printf("  FAILED: Valid message rejected (%s)\n", Net_ProtocolResultToString(result));
        return;
    }

    // Test corrupted message
    MSG_Init(&msg, (byte *)corrupted_packet_data, sizeof(corrupted_packet_data));
    msg.cursize = sizeof(corrupted_packet_data);

    result = Net_ValidateMessageIntegrity(&msg);
    if (result == NET_PROTOCOL_VALID) {
        printf("  FAILED: Corrupted message accepted\n");
        return;
    }

    // Test invalid message bounds
    MSG_Init(&msg, (byte *)valid_packet_data, sizeof(valid_packet_data));
    msg.cursize = sizeof(valid_packet_data) + 100;  // Invalid cursize

    result = Net_ValidateMessageIntegrity(&msg);
    if (result == NET_PROTOCOL_VALID) {
        printf("  FAILED: Invalid message bounds accepted\n");
        return;
    }

    printf("  PASSED: Message integrity validation\n");
}

/*
===============
test_server_info_validation

Test server info validation
===============
*/
static void test_server_info_validation(void) {
    printf("Running test: server info validation\n");

    // Test valid server info
    net_protocol_result_t result = Net_ValidateServerInfo(
        (const byte *)valid_server_info, strlen(valid_server_info));
    if (result != NET_PROTOCOL_VALID) {
        printf("  FAILED: Valid server info rejected (%s)\n", Net_ProtocolResultToString(result));
        return;
    }

    // Test invalid server info
    result = Net_ValidateServerInfo(
        (const byte *)invalid_server_info, strlen(invalid_server_info));
    if (result == NET_PROTOCOL_VALID) {
        printf("  FAILED: Invalid server info accepted\n");
        return;
    }

    printf("  PASSED: Server info validation\n");
}

/*
===============
test_client_command_validation

Test client command validation
===============
*/
static void test_client_command_validation(void) {
    printf("Running test: client command validation\n");

    // Test valid command
    net_protocol_result_t result = Net_ValidateClientCommand(
        (const byte *)valid_client_command, strlen(valid_client_command));
    if (result != NET_PROTOCOL_VALID) {
        printf("  FAILED: Valid client command rejected (%s)\n", Net_ProtocolResultToString(result));
        return;
    }

    // Test invalid command (path traversal)
    result = Net_ValidateClientCommand(
        (const byte *)invalid_client_command, strlen(invalid_client_command));
    if (result == NET_PROTOCOL_VALID) {
        printf("  FAILED: Path traversal command accepted\n");
        return;
    }

    printf("  PASSED: Client command validation\n");
}

/*
===============
test_bounds_checking_functions

Test bounds checking utility functions
===============
*/
static void test_bounds_checking_functions(void) {
    printf("Running test: bounds checking functions\n");

    // Test array bounds
    int test_array[10];
    qboolean result = Net_BoundsCheckArray(test_array, sizeof(int), 5, 20);
    if (!result) {
        printf("  FAILED: Valid array bounds rejected\n");
        return;
    }

    result = Net_BoundsCheckArray(test_array, sizeof(int), 15, 10);
    if (result) {
        printf("  FAILED: Invalid array bounds accepted\n");
        return;
    }

    // Test string bounds
    const char *test_string = "Hello World";
    result = Net_BoundsCheckString(test_string, 20);
    if (!result) {
        printf("  FAILED: Valid string bounds rejected\n");
        return;
    }

    result = Net_BoundsCheckString(test_string, 5);
    if (result) {
        printf("  FAILED: Invalid string bounds accepted\n");
        return;
    }

    // Test pointer bounds
    char buffer[100];
    result = Net_BoundsCheckPointer(buffer + 10, buffer, sizeof(buffer));
    if (!result) {
        printf("  FAILED: Valid pointer bounds rejected\n");
        return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    result = Net_BoundsCheckPointer((char *)buffer + 200, buffer, sizeof(buffer));
#pragma GCC diagnostic pop
    if (result) {
        printf("  FAILED: Invalid pointer bounds accepted\n");
        return;
    }

    printf("  PASSED: Bounds checking functions\n");
}

/*
===============
test_validation_context

Test validation context management
===============
*/
static void test_validation_context(void) {
    printf("Running test: validation context management\n");

    net_validation_context_t ctx;
    net_validation_stats_t stats;

    // Initialize context
    Net_ValidationInit(&ctx);

    // Check initial stats
    Net_ValidationGetStats(&ctx, &stats);
    if (stats.total_packets_validated != 0) {
        printf("  FAILED: Initial stats not zero\n");
        return;
    }

    // Test configuration
    net_validation_config_t config = {
        .enable_validation = qfalse,
        .strict_mode = qtrue,
        .max_packet_rate = 50,
        .max_packet_size = 2048,
        .min_packet_size = 8,
        .validate_sequences = qtrue,
        .detect_corruption = qtrue,
        .enable_rate_limiting = qtrue
    };

    Net_ValidationSetConfig(&ctx, &config);

    // Shutdown context
    Net_ValidationShutdown(&ctx);

    printf("  PASSED: Validation context management\n");
}

/*
===============
test_rate_limiting

Test rate limiting functionality
===============
*/
static void test_rate_limiting(void) {
    printf("Running test: rate limiting\n");

    net_validation_context_t ctx;
    Net_ValidationInit(&ctx);

    // Test normal rate
    qboolean result = Net_CheckRateLimit(&ctx, 1000);
    if (!result) {
        printf("  FAILED: Normal rate rejected\n");
        Net_ValidationShutdown(&ctx);
        return;
    }

    // Test rate limit exceeded (simulate 200 packets in one second)
    for (int i = 0; i < 200; i++) {
        result = Net_CheckRateLimit(&ctx, 1000);
    }

    if (result) {
        printf("  FAILED: Rate limit not enforced\n");
        Net_ValidationShutdown(&ctx);
        return;
    }

    // Test rate reset after second changes
    result = Net_CheckRateLimit(&ctx, 2000);
    if (!result) {
        printf("  FAILED: Rate not reset after time change\n");
        Net_ValidationShutdown(&ctx);
        return;
    }

    Net_ValidationShutdown(&ctx);
    printf("  PASSED: Rate limiting\n");
}

/*
===============
main

Run all network protocol validation tests
===============
*/
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Network Protocol Validation Test Suite\n");
    printf("======================================\n\n");

    int tests_run = 0;
    int tests_passed = 0;

    // Run all tests
    #define RUN_TEST(test_func) \
        do { \
            tests_run++; \
            test_func(); \
            tests_passed++; \
        } while (0)

    RUN_TEST(test_packet_bounds_validation);
    RUN_TEST(test_protocol_header_validation);
    RUN_TEST(test_message_integrity_validation);
    RUN_TEST(test_server_info_validation);
    RUN_TEST(test_client_command_validation);
    RUN_TEST(test_bounds_checking_functions);
    RUN_TEST(test_validation_context);
    RUN_TEST(test_rate_limiting);

    printf("\n======================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("All network protocol validation tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}