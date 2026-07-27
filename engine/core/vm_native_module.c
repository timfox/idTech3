#include "vm_native_module.h"
#include <stdio.h>

int VM_BuildNativeModuleCandidates( const char *moduleName, char out[][MAX_QPATH], int maxCandidates ) {
	int count = 0;

	if ( !moduleName || !moduleName[0] || !out || maxCandidates <= 0 ) {
		return 0;
	}

	/* Preserve the legacy first probe exactly as vm.c used it. */
	(void)snprintf( out[count], MAX_QPATH, "%s.so", moduleName );
	count++;

	if ( count < maxCandidates ) {
		(void)snprintf( out[count], MAX_QPATH, "lib%s%s", moduleName, DLL_EXT );
		count++;
	}

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
