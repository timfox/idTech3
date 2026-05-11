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

static int assert_load_order(const char *moduleName, const char *const *expected, int expectedCount) {
	char out[3][MAX_QPATH];
	int count;
	int i;

	count = VM_BuildNativeModuleLoadOrder(moduleName, out, 3);
	ASSERT(count == expectedCount, "load order count");

	for (i = 0; i < expectedCount; i++) {
		ASSERT(strcmp(out[i], expected[i]) == 0, "load order entry");
	}

	return 0;
}

static int test_load_order_aliases(void) {
	static const char *const qagameOrder[] = { "qagame", "game", "server" };
	static const char *const gameOrder[] = { "game", "server" };
	static const char *const cgameOrder[] = { "cgame", "client" };
	static const char *const uiOrder[] = { "ui", "frontend" };
	static const char *const serverOrder[] = { "server", "game" };
	static const char *const clientOrder[] = { "client", "cgame" };
	static const char *const frontendOrder[] = { "frontend", "ui" };
	static const char *const upperQagameOrder[] = { "QAGAME", "game", "server" };
	char out[3][MAX_QPATH];

	ASSERT(assert_load_order("qagame", qagameOrder, 3) == 0, "qagame aliases");
	ASSERT(assert_load_order("game", gameOrder, 2) == 0, "game aliases");
	ASSERT(assert_load_order("cgame", cgameOrder, 2) == 0, "cgame aliases");
	ASSERT(assert_load_order("ui", uiOrder, 2) == 0, "ui aliases");
	ASSERT(assert_load_order("server", serverOrder, 2) == 0, "server aliases");
	ASSERT(assert_load_order("client", clientOrder, 2) == 0, "client aliases");
	ASSERT(assert_load_order("frontend", frontendOrder, 2) == 0, "frontend aliases");
	ASSERT(assert_load_order("QAGAME", upperQagameOrder, 3) == 0, "case-insensitive alias recognition");

	ASSERT(VM_BuildNativeModuleLoadOrder("renderer", out, 3) == 0, "non-generic module should not use aliases");
	ASSERT(VM_BuildNativeModuleLoadOrder("", out, 3) == 0, "empty module load order");
	ASSERT(VM_BuildNativeModuleLoadOrder(NULL, out, 3) == 0, "NULL module load order");
	ASSERT(VM_BuildNativeModuleLoadOrder("qagame", NULL, 3) == 0, "NULL load-order output");
	ASSERT(VM_BuildNativeModuleLoadOrder("qagame", out, 0) == 0, "zero load-order capacity");

	return 0;
}

static int test_load_order_limit(void) {
	char out[3][MAX_QPATH];
	int count;

	count = VM_BuildNativeModuleLoadOrder("qagame", out, 2);
	ASSERT(count == 2, "limited load-order count");
	ASSERT(strcmp(out[0], "qagame") == 0, "limited load-order primary");
	ASSERT(strcmp(out[1], "game") == 0, "limited load-order first alias");

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
	if (test_load_order_aliases() != 0) {
		return 1;
	}
	if (test_load_order_limit() != 0) {
		return 1;
	}

	printf("PASS: unit_vm_native_module\n");
	return 0;
}
