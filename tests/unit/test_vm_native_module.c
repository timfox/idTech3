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

static int assert_load_order(const char *moduleName, int maxModules, int expectedCount, const char **expected) {
	char out[3][MAX_QPATH];
	int count;
	int i;

	count = VM_BuildNativeModuleLoadOrder(moduleName, out, maxModules);
	ASSERT(count == expectedCount, "load-order count");
	for (i = 0; i < count; i++) {
		ASSERT(strcmp(out[i], expected[i]) == 0, "load-order entry");
	}

	return 0;
}

static int test_load_order_aliases(void) {
	const char *qagame[] = { "qagame", "game", "server" };
	const char *game[] = { "game", "server" };
	const char *cgame[] = { "cgame", "client" };
	const char *ui[] = { "ui", "frontend" };
	const char *server[] = { "server", "game" };
	const char *client[] = { "client", "cgame" };
	const char *frontend[] = { "frontend", "ui" };
	const char *mixedCase[] = { "QAGAME", "game", "server" };

	if (assert_load_order("qagame", 3, 3, qagame) != 0) {
		return 1;
	}
	if (assert_load_order("game", 3, 2, game) != 0) {
		return 1;
	}
	if (assert_load_order("cgame", 3, 2, cgame) != 0) {
		return 1;
	}
	if (assert_load_order("ui", 3, 2, ui) != 0) {
		return 1;
	}
	if (assert_load_order("server", 3, 2, server) != 0) {
		return 1;
	}
	if (assert_load_order("client", 3, 2, client) != 0) {
		return 1;
	}
	if (assert_load_order("frontend", 3, 2, frontend) != 0) {
		return 1;
	}
	if (assert_load_order("QAGAME", 3, 3, mixedCase) != 0) {
		return 1;
	}

	return 0;
}

static int test_load_order_limits_and_custom_modules(void) {
	const char *limited[] = { "qagame", "game" };
	char out[3][MAX_QPATH];

	if (assert_load_order("qagame", 2, 2, limited) != 0) {
		return 1;
	}

	ASSERT(VM_BuildNativeModuleLoadOrder(NULL, out, 3) == 0, "NULL load-order input");
	ASSERT(VM_BuildNativeModuleLoadOrder("", out, 3) == 0, "empty load-order input");
	ASSERT(VM_BuildNativeModuleLoadOrder("custommod", out, 3) == 0, "custom module should use legacy platform-specific fallback only");
	ASSERT(VM_BuildNativeModuleLoadOrder("qagame", NULL, 3) == 0, "NULL load-order output");
	ASSERT(VM_BuildNativeModuleLoadOrder("qagame", out, 0) == 0, "zero load-order capacity");

	return 0;
}

static int capturedCommand;
static int capturedArg0;
static int capturedArg1;
static int capturedArg2;

static intptr_t QDECL capture_entry_point(int command, int arg0, int arg1, int arg2) {
	capturedCommand = command;
	capturedArg0 = arg0;
	capturedArg1 = arg1;
	capturedArg2 = arg2;
	return 1234;
}

static intptr_t call_native_entry_point(int nargs, int callnum, ...) {
	intptr_t result;
	va_list ap;

	va_start(ap, callnum);
	result = VM_CallNativeModuleEntryPoint(capture_entry_point, nargs, callnum, ap);
	va_end(ap);
	return result;
}

static int test_native_call_args_zero_fill(void) {
	ASSERT(call_native_entry_point(0, 77) == 1234, "native entry return value");
	ASSERT(capturedCommand == 77, "zero-arg callnum");
	ASSERT(capturedArg0 == 0, "zero-arg arg0");
	ASSERT(capturedArg1 == 0, "zero-arg arg1");
	ASSERT(capturedArg2 == 0, "zero-arg arg2");

	ASSERT(call_native_entry_point(1, 88, 111) == 1234, "one-arg native entry return value");
	ASSERT(capturedCommand == 88, "one-arg callnum");
	ASSERT(capturedArg0 == 111, "one-arg arg0");
	ASSERT(capturedArg1 == 0, "one-arg arg1 zero-filled");
	ASSERT(capturedArg2 == 0, "one-arg arg2 zero-filled");

	ASSERT(call_native_entry_point(3, 99, 11, 22, 33) == 1234, "three-arg native entry return value");
	ASSERT(capturedCommand == 99, "three-arg callnum");
	ASSERT(capturedArg0 == 11, "three-arg arg0");
	ASSERT(capturedArg1 == 22, "three-arg arg1");
	ASSERT(capturedArg2 == 33, "three-arg arg2");

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
	if (test_load_order_limits_and_custom_modules() != 0) {
		return 1;
	}
	if (test_native_call_args_zero_fill() != 0) {
		return 1;
	}

	printf("PASS: unit_vm_native_module\n");
	return 0;
}
