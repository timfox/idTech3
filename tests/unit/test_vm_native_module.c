/*
 * Unit test: VM native module candidate naming
 * Run: ctest -R unit_vm_native_module
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qcommon/vm_native_module.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_empty_inputs(void) {
	char out[3][MAX_QPATH];

	ASSERT(VM_BuildNativeModuleCandidates(NULL, out, 3) == 0, "NULL moduleName");
	ASSERT(VM_BuildNativeModuleCandidates("", out, 3) == 0, "empty moduleName");
	ASSERT(VM_BuildNativeModuleCandidates("client", NULL, 3) == 0, "NULL output buffer");
	ASSERT(VM_BuildNativeModuleCandidates("client", out, 0) == 0, "zero maxCandidates");

	return 0;
}

static int test_candidate_order_full(void) {
	char out[3][MAX_QPATH];
	char expected0[MAX_QPATH];
	char expected1[MAX_QPATH];
	char expected2[MAX_QPATH];
	int count;

	count = VM_BuildNativeModuleCandidates("client", out, 3);
	ASSERT(count == 3, "full count");

	snprintf(expected0, sizeof(expected0), "%s.so", "client");
	snprintf(expected1, sizeof(expected1), "%s." ARCH_STRING DLL_EXT, "client");
	snprintf(expected2, sizeof(expected2), "%s" ARCH_STRING DLL_EXT, "client");

	ASSERT(strcmp(out[0], expected0) == 0, "candidate 0 should be module.so");
	ASSERT(strcmp(out[1], expected1) == 0, "candidate 1 should be module.arch.so");
	ASSERT(strcmp(out[2], expected2) == 0, "candidate 2 should be modulearch.so");

	return 0;
}

static int test_candidate_limit(void) {
	char out[3][MAX_QPATH];
	char expected0[MAX_QPATH];
	char expected1[MAX_QPATH];
	int count;

	count = VM_BuildNativeModuleCandidates("server", out, 2);
	ASSERT(count == 2, "limited count");

	snprintf(expected0, sizeof(expected0), "%s.so", "server");
	snprintf(expected1, sizeof(expected1), "%s." ARCH_STRING DLL_EXT, "server");

	ASSERT(strcmp(out[0], expected0) == 0, "limited candidate 0");
	ASSERT(strcmp(out[1], expected1) == 0, "limited candidate 1");

	return 0;
}

static void build_call_args(int nargs, int32_t out[3], ...) {
	va_list ap;

	va_start(ap, out);
	VM_BuildNativeModuleCallArgs(nargs, out, 3, ap);
	va_end(ap);
}

static int test_native_call_args_zero_fill(void) {
	int32_t out[3] = { 0x11111111, 0x22222222, 0x33333333 };

	build_call_args(0, out);
	ASSERT(out[0] == 0, "zero-arg call should clear arg0");
	ASSERT(out[1] == 0, "zero-arg call should clear arg1");
	ASSERT(out[2] == 0, "zero-arg call should clear arg2");

	build_call_args(1, out, 1234);
	ASSERT(out[0] == 1234, "one-arg call should copy arg0");
	ASSERT(out[1] == 0, "one-arg call should clear arg1");
	ASSERT(out[2] == 0, "one-arg call should clear arg2");

	return 0;
}

static int test_native_call_args_copy_all_slots(void) {
	int32_t out[3] = { 0 };

	build_call_args(3, out, -1, 0x12345678, 42);
	ASSERT(out[0] == -1, "three-arg call should copy arg0");
	ASSERT(out[1] == 0x12345678, "three-arg call should copy arg1");
	ASSERT(out[2] == 42, "three-arg call should copy arg2");

	return 0;
}

static void build_call_args_limited(int nargs, int32_t out[3], int maxArgs, ...) {
	va_list ap;

	va_start(ap, maxArgs);
	VM_BuildNativeModuleCallArgs(nargs, out, maxArgs, ap);
	va_end(ap);
}

static int test_native_call_args_respect_limit(void) {
	int32_t out[3] = { 77, 88, 99 };

	build_call_args_limited(3, out, 2, 10, 20, 30);
	ASSERT(out[0] == 10, "limited native args should copy arg0");
	ASSERT(out[1] == 20, "limited native args should copy arg1");
	ASSERT(out[2] == 99, "limited native args should not write past maxArgs");

	return 0;
}

int main(void) {
	if (test_empty_inputs() != 0) {
		return 1;
	}
	if (test_candidate_order_full() != 0) {
		return 1;
	}
	if (test_candidate_limit() != 0) {
		return 1;
	}
	if (test_native_call_args_zero_fill() != 0) {
		return 1;
	}
	if (test_native_call_args_copy_all_slots() != 0) {
		return 1;
	}
	if (test_native_call_args_respect_limit() != 0) {
		return 1;
	}

	printf("PASS: unit_vm_native_module\n");
	return 0;
}
