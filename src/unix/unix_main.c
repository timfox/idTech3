#include "../common/qcommon.h"
#include "../client/client.h"
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argc;  // Suppress unused parameter warning

    printf("id Tech 3 Engine Starting...\n");

    // Initialize common systems
    Com_Init(argv[0]);

    printf("Engine initialized successfully\n");

    // Simple test: keep the engine running for a few seconds
    sleep(10);

    printf("Engine shutting down...\n");

    return 0;
}
