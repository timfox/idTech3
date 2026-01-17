/*
===============================================================================
Simple Network Protocol Fuzz Testing

Basic fuzz testing for network protocol parsing.
===============================================================================
*/

#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Simple fuzz data generator
static void GenerateFuzzData(uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)(rand() % 256);
    }
}

// Simple network message fuzzing
static void FuzzNetworkMessage(const uint8_t *data, size_t size) {
    if (size < sizeof(msg_t)) {
        return;
    }

    // Create a message buffer
    byte buffer[2048];
    msg_t msg;

    MSG_Init(&msg, buffer, sizeof(buffer));

    // Write fuzzer data to message (safely)
    size_t write_size = size > 1000 ? 1000 : size; // Limit to prevent overflow
    for (size_t i = 0; i < write_size; i++) {
        MSG_WriteByte(&msg, data[i]);
    }

    // Try to read back data
    msg_t readMsg;
    MSG_Init(&readMsg, buffer, sizeof(buffer));
    readMsg.cursize = msg.cursize;

    // Read some bytes back
    while (readMsg.readcount < readMsg.cursize && readMsg.readcount < 100) {
        (void)MSG_ReadByte(&readMsg);
    }
}

// Simple address parsing fuzzing
static void FuzzAddressParsing(const uint8_t *data, size_t size) {
    if (size < 8) {
        return;
    }

    // Create a null-terminated string from fuzz data
    char addr_str[256];
    size_t copy_size = size > sizeof(addr_str) - 1 ? sizeof(addr_str) - 1 : size;
    memcpy(addr_str, data, copy_size);
    addr_str[copy_size] = '\0';

    // Try to parse as address
    netadr_t addr;
    NET_StringToAdr(addr_str, NULL, NA_IP);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("Network Protocol Fuzz Testing\n");
    printf("=============================\n\n");

    srand((unsigned int)time(NULL));

    const int NUM_ITERATIONS = 1000;
    const size_t MAX_DATA_SIZE = 512;
    uint8_t fuzz_data[MAX_DATA_SIZE];

    printf("Running %d fuzz iterations...\n", NUM_ITERATIONS);

    int iterations_completed = 0;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Generate random fuzz data
        size_t data_size = (rand() % MAX_DATA_SIZE) + 1;
        GenerateFuzzData(fuzz_data, data_size);

        // Run fuzz tests
        FuzzNetworkMessage(fuzz_data, data_size);
        FuzzAddressParsing(fuzz_data, data_size);

        iterations_completed++;
        if (iterations_completed % 100 == 0) {
            printf("Completed %d/%d iterations\n", iterations_completed, NUM_ITERATIONS);
        }
    }

    printf("\nFuzz testing completed successfully!\n");
    printf("No crashes detected in %d iterations.\n", NUM_ITERATIONS);

    return 0;
}