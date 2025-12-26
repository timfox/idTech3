#include "../common/qcommon.h"
#include "../client/client.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {
    (void)argc;  // Suppress unused parameter warning

    printf("id Tech 3 Engine Starting...\n");
    fflush(stdout);

    // Initialize common systems
    Com_Init(argv[0]);

    printf("Engine initialized successfully\n");
    fflush(stdout);

    // Main game loop with frame processing
    int frameCount = 0;
    while (frameCount < 10) {
        frameCount++;
        printf("Processing frame %d\n", frameCount);
        fflush(stdout);

        // Run a frame
        Com_Frame(qfalse);

        // Small delay between frames
        sleep(1);
    }

    printf("Test completed, engine shutting down...\n");
    fflush(stdout);

    return 0;
}
