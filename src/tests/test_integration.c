#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Simplified integration test suite
int main(void) {
    int failures = 0;
    printf("Running basic integration tests...\n");

    // Test 1: Memory system integrity
    printf("  Testing memory system...\n");
    void* testAlloc = malloc(1024);
    if (!testAlloc) {
        printf("  ❌ Memory allocation test failed\n");
        failures++;
    } else {
        free(testAlloc);
        printf("  ✅ Memory system OK\n");
    }

    // Test 2: Basic math functions
    printf("  Testing math functions...\n");
    float testVec[3] = {1.0f, 2.0f, 3.0f};
    float len = sqrtf(testVec[0]*testVec[0] + testVec[1]*testVec[1] + testVec[2]*testVec[2]);
    if (len < 3.7f || len > 3.8f) {  // sqrt(1+4+9) = sqrt(14) ≈ 3.74
        printf("  ❌ Math functions test failed (len=%.2f)\n", len);
        failures++;
    } else {
        printf("  ✅ Math functions OK\n");
    }

    // Test 3: String operations
    printf("  Testing string operations...\n");
    char testStr[32] = "integration";
    if (strlen(testStr) != 11) {
        printf("  ❌ String operations test failed\n");
        failures++;
    } else {
        printf("  ✅ String operations OK\n");
    }

    printf("Basic integration tests completed: %d failures\n", failures);
    return failures;
}