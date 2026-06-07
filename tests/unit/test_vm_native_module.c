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

static int test_load_order_empty_inputs(void) {
	char out[VM_MAX_NATIVE_MODULE_LOAD_NAMES][MAX_QPATH];

	ASSERT(VM_BuildNativeModuleLoadOrder(NULL, out, VM_MAX_NATIVE_MODULE_LOAD_NAMES) == 0, "NULL load-order moduleName");
	ASSERT(VM_BuildNativeModuleLoadOrder("", out, VM_MAX_NATIVE_MODULE_LOAD_NAMES) == 0, "empty load-order moduleName");
	ASSERT(VM_BuildNativeModuleLoadOrder("qagame", NULL, VM_MAX_NATIVE_MODULE_LOAD_NAMES) == 0, "NULL load-order output buffer");
	ASSERT(VM_BuildNativeModuleLoadOrder("qagame", out, 0) == 0, "zero load-order maxModules");

	return 0;
}

static int assert_load_order(const char *moduleName, const char *expected0, const char *expected1, const char *expected2) {
	char out[VM_MAX_NATIVE_MODULE_LOAD_NAMES][MAX_QPATH];
	int expectedCount = 0;
	int count;

	if ( expected0 ) {
		expectedCount++;
	}
	if ( expected1 ) {
		expectedCount++;
	}
	if ( expected2 ) {
		expectedCount++;
	}

	count = VM_BuildNativeModuleLoadOrder(moduleName, out, VM_MAX_NATIVE_MODULE_LOAD_NAMES);
	ASSERT(count == expectedCount, "load-order count");
	if ( expected0 ) {
		ASSERT(strcmp(out[0], expected0) == 0, "load-order primary");
	}
	if ( expected1 ) {
		ASSERT(strcmp(out[1], expected1) == 0, "load-order first alias");
	}
	if ( expected2 ) {
		ASSERT(strcmp(out[2], expected2) == 0, "load-order second alias");
	}

	return 0;
}

static int test_load_order_aliases(void) {
	ASSERT(assert_load_order("qagame", "qagame", "game", "server") == 0, "qagame load order");
	ASSERT(assert_load_order("game", "game", "server", NULL) == 0, "game load order");
	ASSERT(assert_load_order("server", "server", "game", NULL) == 0, "server load order");
	ASSERT(assert_load_order("cgame", "cgame", "client", NULL) == 0, "cgame load order");
	ASSERT(assert_load_order("client", "client", "cgame", NULL) == 0, "client load order");
	ASSERT(assert_load_order("ui", "ui", "frontend", NULL) == 0, "ui load order");
	ASSERT(assert_load_order("frontend", "frontend", "ui", NULL) == 0, "frontend load order");

	return 0;
}

static int test_load_order_case_and_limits(void) {
	char out[VM_MAX_NATIVE_MODULE_LOAD_NAMES][MAX_QPATH];
	int count;

	count = VM_BuildNativeModuleLoadOrder("QAGAME", out, 2);
	ASSERT(count == 2, "limited qagame load-order count");
	ASSERT(strcmp(out[0], "QAGAME") == 0, "primary load-order name preserves input case");
	ASSERT(strcmp(out[1], "game") == 0, "limited qagame first alias");

	count = VM_BuildNativeModuleLoadOrder("FrontEnd", out, VM_MAX_NATIVE_MODULE_LOAD_NAMES);
	ASSERT(count == 2, "mixed-case frontend load-order count");
	ASSERT(strcmp(out[0], "FrontEnd") == 0, "frontend primary preserves input case");
	ASSERT(strcmp(out[1], "ui") == 0, "frontend reverse alias");

	ASSERT(VM_BuildNativeModuleLoadOrder("renderer", out, VM_MAX_NATIVE_MODULE_LOAD_NAMES) == 0, "custom modules keep legacy fallback path");

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
	if (test_load_order_empty_inputs() != 0) {
		return 1;
	}
	if (test_load_order_aliases() != 0) {
		return 1;
	}
	if (test_load_order_case_and_limits() != 0) {
		return 1;
	}

	printf("PASS: unit_vm_native_module\n");
	return 0;
}
