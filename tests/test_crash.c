#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

// Simple test to verify crash handler works
int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "input_validation") == 0) {
        // Test input validation functionality
        printf("Testing input validation...\n");

        // Test variables
        const char *test_string = "valid_test_string";
        const char *safe_string = "hello world";
        const char *unsafe_string = "hello; rm -rf /";
        int handle = 5;

        // Test string bounds checking
        printf("String bounds check for '%s': %s\n",
               test_string,
               (strlen(test_string) < 50) ? "PASS" : "FAIL");

        // Test injection detection
        printf("Injection check for safe string: %s\n",
               (strpbrk(safe_string, "|&;<>()$`\\") == NULL) ? "PASS" : "FAIL");
        printf("Injection check for unsafe string: %s\n",
               (strpbrk(unsafe_string, "|&;<>()$`\\") != NULL) ? "PASS" : "FAIL");

        // Test validation macros (using the actual utility functions)
        printf("Handle validation test (valid): %s\n",
               VALID_HANDLE(handle, 100) ? "PASS" : "FAIL");
        printf("Handle validation test (invalid): %s\n",
               !VALID_HANDLE(-1, 100) ? "PASS" : "FAIL");

        printf("String validation test (valid): %s\n",
               VALID_STRING(test_string) ? "PASS" : "FAIL");
        printf("String validation test (null): %s\n",
               !VALID_STRING(NULL) ? "PASS" : "FAIL");

        printf("String length validation test (valid): %s\n",
               STRING_LENGTH_VALID(test_string, 100) ? "PASS" : "FAIL");
        printf("String length validation test (too long): %s\n",
               !STRING_LENGTH_VALID(test_string, 5) ? "PASS" : "FAIL");

        printf("Range validation test (valid): %s\n",
               IN_RANGE(50, 0, 100) ? "PASS" : "FAIL");
        printf("Range validation test (invalid): %s\n",
               !IN_RANGE(150, 0, 100) ? "PASS" : "FAIL");

        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "performance") == 0) {
        // Test performance profiling functionality
        printf("Testing performance profiling...\n");

        // Start a counter
        Perf_BeginCounter("test_counter");

        // Simulate some work
        volatile int sum = 0;
        for (int i = 0; i < 10000; i++) {
            sum += i;
        }

        // End the counter
        Perf_EndCounter("test_counter");

        // Print report
        Perf_PrintReport();

        // Reset counters
        Perf_ResetCounters();

        printf("Performance profiling test completed\n");
        return 0;
    }

    printf("Testing crash handler...\n");

    // Give a moment for output to appear
    sleep(1);

    // Trigger segmentation fault to test crash handler
    printf("Triggering crash...\n");
    *(int*)0 = 42;

    return 0;
}
