#include <stdio.h>
#include <string.h>

#include "vm_native_module_names.h"

#define ASSERT(cond, msg) do { \
	if ( !( cond ) ) { \
		fprintf( stderr, "FAIL: %s\n", msg ); \
		return 1; \
	} \
} while ( 0 )

int main( void )
{
	char buf[128];

	ASSERT( VM_NativeModuleCandidateCount() == 3u, "candidate count is 3" );

	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", "aarch64", ".so", 0 );
	ASSERT( strcmp( buf, "client.so" ) == 0, "candidate 0: module.so" );

	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", "aarch64", ".so", 1 );
	ASSERT( strcmp( buf, "client.aarch64.so" ) == 0, "candidate 1: module.arch.so" );

	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", "aarch64", ".so", 2 );
	ASSERT( strcmp( buf, "clientaarch64.so" ) == 0, "candidate 2: modulearch.so" );

	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", "aarch64", ".so", 99 );
	ASSERT( buf[0] == '\0', "invalid candidate index is empty" );

	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", "aarch64", ".so", 1 );
	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), NULL, "aarch64", ".so", 1 );
	ASSERT( buf[0] == '\0', "NULL module input clears output" );

	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", "aarch64", ".so", 1 );
	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", NULL, ".so", 1 );
	ASSERT( buf[0] == '\0', "NULL arch input clears output" );

	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", "aarch64", ".so", 1 );
	VM_FormatNativeModuleCandidate( buf, sizeof( buf ), "client", "aarch64", NULL, 1 );
	ASSERT( buf[0] == '\0', "NULL ext input clears output" );

	/*
	 * outSize == 1 must remain NUL-terminated and never overflow.
	 * This catches tiny-buffer callers and future formatting changes.
	 */
	buf[0] = 'X';
	VM_FormatNativeModuleCandidate( buf, 1u, "client", "aarch64", ".so", 1 );
	ASSERT( buf[0] == '\0', "tiny output buffer remains empty string" );

	/* NULL output pointer should be a no-op (no crash). */
	VM_FormatNativeModuleCandidate( NULL, 0u, "client", "aarch64", ".so", 0 );

	printf( "PASS: unit_vm_native_module_names\n" );
	return 0;
}
