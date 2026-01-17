/*
===============================================================================
Simple VM System Test

Basic validation of VM system functionality.
===============================================================================
*/

#include "../common/vm_local.h"
#include "../common/qcommon.h"
#include "../common/q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock system call function for testing
static intptr_t TestSystemCall(intptr_t *args) {
    (void)args; // Suppress unused parameter warning
    return 0; // Mock implementation
}

// Mock DLL syscall function
static intptr_t TestDLLSyscall(intptr_t arg, ...) {
    (void)arg; // Mock implementation
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("VM System Test Suite\n");
    printf("===================\n\n");

    vm_t *vm;
    int test_count = 0;
    int pass_count = 0;

    printf("Testing VM creation and destruction...\n");

    // Test invalid VM indices
    vm = VM_Create(-1, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    test_count++;
    if (vm == NULL) {
        printf("✓ VM_Create correctly failed with negative index\n");
        pass_count++;
    } else {
        printf("✗ VM_Create should fail with negative index\n");
    }

    vm = VM_Create(VM_COUNT, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    test_count++;
    if (vm == NULL) {
        printf("✓ VM_Create correctly failed with index >= VM_COUNT\n");
        pass_count++;
    } else {
        printf("✗ VM_Create should fail with index >= VM_COUNT\n");
    }

    // Test valid VM creation
    vm = VM_Create(0, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    test_count++;
    if (vm != NULL) {
        printf("✓ VM_Create succeeded for valid index\n");
        pass_count++;

        // Test VM destruction
        VM_Free(vm);
        printf("✓ VM_Free completed\n");
    } else {
        printf("✗ VM_Create failed for valid index\n");
    }

    printf("\nTest Results: %d/%d passed\n", pass_count, test_count);

    if (pass_count == test_count) {
        printf("✓ All VM tests passed!\n");
        return 0;
    } else {
        printf("✗ Some VM tests failed!\n");
        return 1;
    }
}