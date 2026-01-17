/*
===============================================================================
Simple Network System Test

Basic validation of network system functionality.
===============================================================================
*/

#include "../common/qcommon.h"
#include "../common/q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("Network System Test Suite\n");
    printf("========================\n\n");

    int test_count = 0;
    int pass_count = 0;

    printf("Testing network system initialization...\n");

    // Test network initialization (NET_Init returns void)
    test_count++;
    NET_Init();
    printf("✓ NET_Init called\n");
    pass_count++;

    // Test address parsing
    printf("Testing address parsing...\n");
    netadr_t addr;

    test_count++;
    if (NET_StringToAdr("127.0.0.1", &addr, NA_IP) == 0) {
        printf("✗ Failed to parse localhost IPv4\n");
    } else {
        printf("✓ Successfully parsed localhost IPv4\n");
        pass_count++;
    }

    test_count++;
    if (NET_StringToAdr("::1", &addr, NA_IP6) == 0) {
        printf("✗ Failed to parse localhost IPv6\n");
    } else {
        printf("✓ Successfully parsed localhost IPv6\n");
        pass_count++;
    }

    // Test invalid addresses
    test_count++;
    if (NET_StringToAdr("invalid.address", &addr, NA_IP) != 0) {
        printf("✗ Should fail to parse invalid address\n");
    } else {
        printf("✓ Correctly rejected invalid address\n");
        pass_count++;
    }

    printf("\nTest Results: %d/%d passed\n", pass_count, test_count);

    // Shutdown network system
    NET_Shutdown();

    if (pass_count == test_count) {
        printf("✓ All network tests passed!\n");
        return 0;
    } else {
        printf("✗ Some network tests failed!\n");
        return 1;
    }
}