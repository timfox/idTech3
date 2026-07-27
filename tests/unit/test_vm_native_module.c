/*
 * Unit test: VM native module candidate naming
 * Run: ctest -R unit_vm_native_module
 */
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
	char out[4][MAX_QPATH];

	ASSERT(VM_BuildNativeModuleCandidates(NULL, out, 4) == 0, "NULL moduleName");
	ASSERT(VM_BuildNativeModuleCandidates("", out, 4) == 0, "empty moduleName");
	ASSERT(VM_BuildNativeModuleCandidates("client", NULL, 4) == 0, "NULL output buffer");
	ASSERT(VM_BuildNativeModuleCandidates("client", out, 0) == 0, "zero maxCandidates");

	return 0;
}

static int test_candidate_order_full(void) {
	char out[4][MAX_QPATH];
	char expected0[MAX_QPATH];
	char expected1[MAX_QPATH];
	char expected2[MAX_QPATH];
	char expected3[MAX_QPATH];
	int count;

	count = VM_BuildNativeModuleCandidates("client", out, 4);
	ASSERT(count == 4, "full count");

	snprintf(expected0, sizeof(expected0), "%s.so", "client");
	snprintf(expected1, sizeof(expected1), "lib%s%s", "client", DLL_EXT);
	snprintf(expected2, sizeof(expected2), "%s." ARCH_STRING DLL_EXT, "client");
	snprintf(expected3, sizeof(expected3), "%s" ARCH_STRING DLL_EXT, "client");

	ASSERT(strcmp(out[0], expected0) == 0, "candidate 0 should be module.so");
	ASSERT(strcmp(out[1], expected1) == 0, "candidate 1 should be libmodule.so");
	ASSERT(strcmp(out[2], expected2) == 0, "candidate 2 should be module.arch.so");
	ASSERT(strcmp(out[3], expected3) == 0, "candidate 3 should be modulearch.so");

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
	snprintf(expected1, sizeof(expected1), "lib%s%s", "server", DLL_EXT);

	ASSERT(strcmp(out[0], expected0) == 0, "limited candidate 0");
	ASSERT(strcmp(out[1], expected1) == 0, "limited candidate 1");

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

	printf("PASS: unit_vm_native_module\n");
	return 0;
}
