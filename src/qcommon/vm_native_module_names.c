#include "vm_native_module_names.h"

#include <stddef.h>
#include <stdio.h>

size_t VM_NativeModuleCandidateCount( void )
{
	return 3;
}

void VM_FormatNativeModuleCandidate( char *out,
	size_t outSize,
	const char *moduleName,
	const char *archString,
	const char *dllExt,
	size_t candidateIndex )
{
	if ( !out || outSize == 0 ) {
		return;
	}

	out[0] = '\0';
	if ( !moduleName || !archString || !dllExt ) {
		return;
	}

	switch ( candidateIndex ) {
		case 0:
			/* module.so */
			snprintf( out, outSize, "%s.so", moduleName );
			break;
		case 1:
			/* module.arch.so */
			snprintf( out, outSize, "%s.%s%s", moduleName, archString, dllExt );
			break;
		case 2:
			/* modulearch.so */
			snprintf( out, outSize, "%s%s%s", moduleName, archString, dllExt );
			break;
		default:
			/* Invalid candidate index -> leave empty string. */
			break;
	}
}
