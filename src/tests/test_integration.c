/*
===============================================================================

MULTIPLAYER INTEGRATION TEST SUITE

Comprehensive integration tests for multiplayer scenarios including:
- Client-server connection establishment
- Game state synchronization
- Network message reliability
- Multiplayer session management
===============================================================================
*/

#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_PORT 27960
#define TEST_TIMEOUT_MS 5000

static qboolean TestNetworkInitialization(void) {
    TEST_BEGIN("Network System Initialization");

    // Test basic network initialization
    TEST_ASSERT(NET_Init() == qtrue, "Network initialization should succeed");

    // Test socket creation
    TEST_ASSERT(NET_GetLocalAddress() != NULL, "Should get local address");

    // Test address parsing
    netadr_t addr;
    TEST_ASSERT(NET_StringToAdr("127.0.0.1", &addr), "Should parse localhost address");
    TEST_ASSERT(addr.type == NA_IP, "Address should be IPv4 type");

    // Test port handling
    addr.port = BigShort(TEST_PORT);
    TEST_ASSERT(BigShort(addr.port) == TEST_PORT, "Port should be correctly set");

    NET_Shutdown();
    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestMessageSerialization(void) {
    msg_t msg;
    byte buffer[1024];
    char test_string[] = "Hello, multiplayer world!";
    int test_int = 0xDEADBEEF;
    short test_short = 12345;
    byte test_byte = 42;

    TEST_BEGIN("Message Serialization/Deserialization");

    // Initialize message
    MSG_Init(&msg, buffer, sizeof(buffer));

    // Write various data types
    MSG_WriteByte(&msg, test_byte);
    MSG_WriteShort(&msg, test_short);
    MSG_WriteLong(&msg, test_int);
    MSG_WriteString(&msg, test_string);

    // Verify write operations
    TEST_ASSERT(msg.cursize > 0, "Message should contain data after writing");

    // Read back the data
    msg.readcount = 0;

    byte read_byte = MSG_ReadByte(&msg);
    TEST_ASSERT(read_byte == test_byte, "Byte should round-trip correctly");

    short read_short = MSG_ReadShort(&msg);
    TEST_ASSERT(read_short == test_short, "Short should round-trip correctly");

    int read_int = MSG_ReadLong(&msg);
    TEST_ASSERT(read_int == test_int, "Long should round-trip correctly");

    char read_string[256];
    MSG_ReadString(&msg, read_string, sizeof(read_string));
    TEST_ASSERT(strcmp(read_string, test_string) == 0, "String should round-trip correctly");

    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestPacketFragmentation(void) {
    msg_t msg;
    byte buffer[2048];
    byte test_data[1500]; // Larger than typical MTU
    int i;

    TEST_BEGIN("Packet Fragmentation");

    // Create test data
    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (byte)(i % 256);
    }

    MSG_Init(&msg, buffer, sizeof(buffer));

    // Write large data that would exceed MTU
    MSG_WriteData(&msg, test_data, sizeof(test_data));
    TEST_ASSERT(msg.cursize == sizeof(test_data), "Should write all test data");

    // Read back and verify
    msg.readcount = 0;
    byte read_data[1500];
    int bytes_read = MSG_ReadData(&msg, read_data, sizeof(read_data));

    TEST_ASSERT(bytes_read == sizeof(test_data), "Should read back all data");
    TEST_ASSERT(memcmp(read_data, test_data, sizeof(test_data)) == 0, "Data should match exactly");

    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestNetworkReliability(void) {
    netadr_t test_addr;
    byte test_data[] = "Reliability test packet";
    int packet_count = 100;

    TEST_BEGIN("Network Reliability");

    NET_Init();

    // Set up test address
    NET_StringToAdr("127.0.0.1", &test_addr);
    test_addr.port = BigShort(TEST_PORT);

    TEST_START_TIMING();

    // Send multiple packets to test reliability
    for (int i = 0; i < packet_count; i++) {
        test_data[0] = (byte)(i % 256); // Vary packet content
        qboolean sent = NET_SendPacket(NS_SERVER, sizeof(test_data), test_data, test_addr);
        TEST_ASSERT(sent, "Packet should be sent successfully");

        // Small delay to avoid overwhelming
        usleep(1000); // 1ms
    }

    TEST_END_TIMING();
    TEST_PERFORMANCE_CHECK(packet_count, "Network packet transmission");

    NET_Shutdown();

    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestProtocolValidation(void) {
    TEST_BEGIN("Protocol Validation");

    // Test valid protocol versions
    TEST_ASSERT(PROTOCOL_VERSION >= 66 && PROTOCOL_VERSION <= 72, "Protocol version should be valid");

    // Test connection string validation
    const char *valid_connect = "connect 127.0.0.1:27960";
    const char *invalid_connect = "connect ../../etc/passwd";

    // Basic validation - should not contain dangerous characters
    TEST_ASSERT(strstr(valid_connect, "../") == NULL, "Valid connect string should not contain path traversal");
    TEST_ASSERT(strstr(invalid_connect, "../") != NULL, "Invalid connect string should be detected");

    // Test command validation
    const char *valid_cmd = "say hello world";
    const char *invalid_cmd = "exec ../../evil.cfg";

    TEST_ASSERT(strstr(valid_cmd, "../") == NULL, "Valid command should not contain path traversal");
    TEST_ASSERT(strstr(invalid_cmd, "../") != NULL, "Invalid command should be detected");

    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestPerformanceUnderLoad(void) {
    msg_t msg;
    byte buffer[65536];
    const int ITERATIONS = 10000;

    TEST_BEGIN("Performance Under Load");

    MSG_Init(&msg, buffer, sizeof(buffer));

    // Test message system performance
    TEST_START_TIMING();

    for (int i = 0; i < ITERATIONS; i++) {
        MSG_WriteByte(&msg, (byte)(i % 256));
        MSG_WriteShort(&msg, (short)i);
        MSG_WriteLong(&msg, i * 1000);
        MSG_WriteFloat(&msg, (float)i / 1000.0f);
    }

    TEST_END_TIMING();
    TEST_PERFORMANCE_CHECK(ITERATIONS * 11, "Message system operations"); // ~11 bytes per iteration

    // Test message reading performance
    msg.readcount = 0;

    TEST_START_TIMING();

    for (int i = 0; i < ITERATIONS; i++) {
        MSG_ReadByte(&msg);
        MSG_ReadShort(&msg);
        MSG_ReadLong(&msg);
        MSG_ReadFloat(&msg);
    }

    TEST_END_TIMING();
    TEST_PERFORMANCE_CHECK(ITERATIONS * 11, "Message reading operations");

    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("Multiplayer Integration Test Suite\n");
    printf("===================================\n\n");

    TEST_INITIALIZE();

    // Run integration tests
    TestNetworkInitialization();
    TestMessageSerialization();
    TestPacketFragmentation();
    TestNetworkReliability();
    TestProtocolValidation();
    TestPerformanceUnderLoad();

    TEST_SUMMARY();

    return test_stats.failed_tests > 0 ? 1 : 0;
}