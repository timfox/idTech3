#include "../common/qcommon.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("id Tech 3 Engine Starting...\n");
    
    // Initialize common systems
    Com_Init(argv[0]);  // Pass the program name as command line
    
    // Engine main loop would go here
    // For now, just exit
    printf("Engine initialized successfully\n");
    
    return 0;
}
