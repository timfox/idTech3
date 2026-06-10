#include "vm_native_module.h"
#include <stdio.h>
#include <string.h>

int VM_BuildNativeModuleCandidates( const char *moduleName, char out[][MAX_QPATH], int maxCandidates ) {
	int count = 0;

	if ( !moduleName || !moduleName[0] || !out || maxCandidates <= 0 ) {
		return 0;
	}

	/* Preserve the legacy first probe exactly as vm.c used it. */
	(void)snprintf( out[count], MAX_QPATH, "%s.so", moduleName );
	count++;

	if ( count < maxCandidates ) {
		(void)snprintf( out[count], MAX_QPATH, "%s." ARCH_STRING DLL_EXT, moduleName );
		count++;
	}

	if ( count < maxCandidates ) {
		(void)snprintf( out[count], MAX_QPATH, "%s" ARCH_STRING DLL_EXT, moduleName );
		count++;
	}

	return count;
}

void VM_BuildNativeModuleCallArgs( int nargs, int32_t *out, int maxArgs, va_list ap ) {
	int i;
	int copyCount;

	if ( !out || maxArgs <= 0 ) {
		return;
	}

	memset( out, 0, (size_t)maxArgs * sizeof( out[0] ) );

	copyCount = nargs;
	if ( copyCount > maxArgs ) {
		copyCount = maxArgs;
	}

	for ( i = 0; i < copyCount; i++ ) {
		out[i] = va_arg( ap, int32_t );
	}
}
