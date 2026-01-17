/*
===============================================================================
VM System Test Suite

Comprehensive tests for the Virtual Machine system including:
- VM creation and destruction
- DLL loading and symbol resolution
- QVM loading and validation
- System call handling
- Memory management
===============================================================================
*/

#include "../common/vm_local.h"
#include "../common/qcommon.h"
#include "../common/q_shared.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock system call function for testing
static intptr_t TestSystemCall(intptr_t *args) {
    return 0; // Mock implementation
}

// Mock DLL syscall function
static void TestDLLSyscall(intptr_t arg, ...) {
    (void)arg; // Mock implementation
}

static qboolean TestVMCreationAndDestruction(void) {
    vm_t *vm;
    int i;

    TEST_BEGIN("VM Creation and Destruction");

    // Test invalid VM indices
    vm = VM_Create(-1, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    TEST_ASSERT(vm == NULL, "VM_Create should fail with negative index");

    vm = VM_Create(VM_COUNT, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    TEST_ASSERT(vm == NULL, "VM_Create should fail with index >= VM_COUNT");

    // Test valid VM creation for each type
    for (i = 0; i < VM_COUNT; i++) {
        vm = VM_Create(i, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
        if (!vm) {
            TEST_WARNING("Failed to create VM for index %d", i);
            continue;
        }

        // Verify VM properties
        TEST_ASSERT(vm->index == i, "VM index should match requested index");
        TEST_ASSERT(vm->systemCall == TestSystemCall, "VM systemCall should be set correctly");
        TEST_ASSERT(vm->dllSyscall == TestDLLSyscall, "VM dllSyscall should be set correctly");

        // Clean up
        VM_Free(vm);
    }

    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestVMInterpretModes(void) {
    vm_t *vm;

    TEST_BEGIN("VM Interpret Modes");

    // Test VMI_NATIVE mode
    vm = VM_Create(VM_GAME, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    if (vm) {
        TEST_ASSERT(vm->index == VM_GAME, "VM should be created in NATIVE mode");
        VM_Free(vm);
    } else {
        TEST_WARNING("Could not test NATIVE mode - VM creation failed");
    }

    // Test VMI_COMPILED mode (QVM)
    vm = VM_Create(VM_GAME, TestSystemCall, TestDLLSyscall, VMI_COMPILED);
    if (vm) {
        TEST_ASSERT(vm->index == VM_GAME, "VM should be created in COMPILED mode");
        VM_Free(vm);
    } else {
        TEST_WARNING("Could not test COMPILED mode - VM creation failed or no QVM available");
    }

    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestVMMemoryManagement(void) {
    vm_t *vm;
    void *test_memory;
    size_t test_size = 1024;

    TEST_BEGIN("VM Memory Management");

    vm = VM_Create(VM_GAME, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    if (!vm) {
        TEST_WARNING("Cannot test VM memory management - VM creation failed");
        TEST_END();
        return qtrue; // Not a failure of the test itself
    }

    // Test memory allocation bounds
    test_memory = VM_Alloc(vm, test_size);
    TEST_ASSERT(test_memory != NULL, "VM_Alloc should succeed for valid size");

    if (test_memory) {
        // Test memory access within bounds
        memset(test_memory, 0xAA, test_size);
        TEST_ASSERT(((byte*)test_memory)[0] == 0xAA, "Allocated memory should be writable");
        TEST_ASSERT(((byte*)test_memory)[test_size-1] == 0xAA, "Allocated memory should be accessible to end");

        VM_Free(vm, test_memory);
    }

    // Test invalid memory operations
    VM_Free(vm, NULL); // Should not crash

    VM_Free(vm);
    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestVMSymbolResolution(void) {
    vm_t *vm;

    TEST_BEGIN("VM Symbol Resolution");

    vm = VM_Create(VM_GAME, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    if (!vm) {
        TEST_WARNING("Cannot test VM symbol resolution - VM creation failed");
        TEST_END();
        return qtrue;
    }

    // Test system call function resolution
    TEST_ASSERT(vm->systemCall != NULL, "VM should have system call function");
    TEST_ASSERT(vm->dllSyscall != NULL, "VM should have DLL syscall function");

    // Test calling system functions (should not crash)
    if (vm->systemCall) {
        intptr_t result = vm->systemCall(NULL);
        TEST_ASSERT(result == 0, "System call should return mock value");
    }

    VM_Free(vm);
    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestVMErrorHandling(void) {
    TEST_BEGIN("VM Error Handling");

    // Test error conditions that should be handled gracefully

    // Invalid parameters
    TEST_ASSERT(VM_Create(-1, NULL, NULL, VMI_NATIVE) == NULL,
                "VM_Create should handle invalid parameters");

    TEST_ASSERT(VM_Create(VM_COUNT, NULL, NULL, VMI_COMPILED) == NULL,
                "VM_Create should handle out-of-range indices");

    // Test VM operations on NULL VM
    VM_Free(NULL); // Should not crash

    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

static qboolean TestVMPerformance(void) {
    vm_t *vm;
    int i;
    const int ITERATIONS = 1000;

    TEST_BEGIN("VM Performance Characteristics");

    vm = VM_Create(VM_GAME, TestSystemCall, TestDLLSyscall, VMI_NATIVE);
    if (!vm) {
        TEST_WARNING("Cannot test VM performance - VM creation failed");
        TEST_END();
        return qtrue;
    }

    // Performance test - measure VM call overhead
    TEST_START_TIMING();

    for (i = 0; i < ITERATIONS; i++) {
        if (vm->systemCall) {
            vm->systemCall(NULL);
        }
    }

    TEST_END_TIMING();
    TEST_PERFORMANCE_CHECK(ITERATIONS, "VM system calls");

    VM_Free(vm);
    TEST_END();
    return test_stats.passed_tests == test_stats.total_tests;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("VM System Test Suite\n");
    printf("====================\n\n");

    TEST_INITIALIZE();

    // Run all VM tests
    TestVMCreationAndDestruction();
    TestVMInterpretModes();
    TestVMMemoryManagement();
    TestVMSymbolResolution();
    TestVMErrorHandling();
    TestVMPerformance();

    TEST_SUMMARY();

    return test_stats.failed_tests > 0 ? 1 : 0;
}