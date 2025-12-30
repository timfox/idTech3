#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// Simple test to verify crash handler works
int main() {
    printf("Testing crash handler...\n");
    
    // Give a moment for output to appear
    sleep(1);
    
    // Trigger segmentation fault to test crash handler
    printf("Triggering crash...\n");
    *(int*)0 = 42;
    
    return 0;
}
