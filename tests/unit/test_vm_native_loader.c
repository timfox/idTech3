/*
 * Unit test: VM native module filename candidate generation
 * Run: ctest -R unit_vm_native_loader
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "qcommon/vm_native_loader.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

/*
 * Standalone unit target shim for vm_native_loader.c:
 * provide Com_sprintf without linking the full engine runtime.
 */
int QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) {
	int written;
	va_list args;

	va_start( args, fmt );
	written = vsnprintf( dest, (size_t)size, fmt, args );
	va_end( args );

	if ( written < 0 ) {
		if ( size > 0 ) {
			dest[0] = '\0';
		}
		return 0;
	}

	if ( written >= size && size > 0 ) {
		dest[size - 1] = '\0';
		return size - 1;
	}

	return written;
}

static int test_candidate_order(void) {
	char candidate[128];
	char expected0[128];
	char expected1[128];
	char expected2[128];

	Com_sprintf(expected0, sizeof(expected0), "%s.so", "client");
	Com_sprintf(expected1, sizeof(expected1), "%s." ARCH_STRING DLL_EXT, "client");
	Com_sprintf(expected2, sizeof(expected2), "%s" ARCH_STRING DLL_EXT, "client");

	ASSERT(VM_BuildNativeModuleCandidate("client", 0, candidate, sizeof(candidate)) == qtrue, "candidate 0 valid");
	ASSERT(strcmp(candidate, expected0) == 0, "candidate 0 value");

	ASSERT(VM_BuildNativeModuleCandidate("client", 1, candidate, sizeof(candidate)) == qtrue, "candidate 1 valid");
	ASSERT(strcmp(candidate, expected1) == 0, "candidate 1 value");

	ASSERT(VM_BuildNativeModuleCandidate("client", 2, candidate, sizeof(candidate)) == qtrue, "candidate 2 valid");
	ASSERT(strcmp(candidate, expected2) == 0, "candidate 2 value");

	return 0;
}

static int test_input_validation(void) {
	char candidate[64];

	ASSERT(VM_BuildNativeModuleCandidate("client", -1, candidate, sizeof(candidate)) == qfalse, "negative index invalid");
	ASSERT(VM_BuildNativeModuleCandidate("client", VM_NATIVE_MODULE_CANDIDATE_COUNT, candidate, sizeof(candidate)) == qfalse, "out of range index invalid");
	ASSERT(VM_BuildNativeModuleCandidate(NULL, 0, candidate, sizeof(candidate)) == qfalse, "null module invalid");
	ASSERT(VM_BuildNativeModuleCandidate("", 0, candidate, sizeof(candidate)) == qfalse, "empty module invalid");
	ASSERT(VM_BuildNativeModuleCandidate("client", 0, NULL, sizeof(candidate)) == qfalse, "null output invalid");
	ASSERT(VM_BuildNativeModuleCandidate("client", 0, candidate, 0) == qfalse, "zero size invalid");

	return 0;
}

int main(void) {
	if (test_candidate_order() != 0) {
		return 1;
	}
	if (test_input_validation() != 0) {
		return 1;
	}

	printf("PASS: unit_vm_native_loader\n");
	return 0;
}
