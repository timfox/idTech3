#include "vm_native_loader.h"

qboolean VM_BuildNativeModuleCandidate( const char *moduleName, int candidateIndex, char *filename, int filenameSize ) {
	if ( !moduleName || !moduleName[0] || !filename || filenameSize <= 0 ) {
		return qfalse;
	}

	switch ( candidateIndex ) {
		case 0:
			Com_sprintf( filename, filenameSize, "%s.so", moduleName );
			return qtrue;
		case 1:
			Com_sprintf( filename, filenameSize, "%s." ARCH_STRING DLL_EXT, moduleName );
			return qtrue;
		case 2:
			Com_sprintf( filename, filenameSize, "%s" ARCH_STRING DLL_EXT, moduleName );
			return qtrue;
		default:
			return qfalse;
	}
}
