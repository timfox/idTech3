#include "vm_native_module.h"
#include <stdio.h>

static void VM_CopyNativeModuleName( char *dst, const char *src ) {
	(void)snprintf( dst, MAX_QPATH, "%s", src );
}

static qboolean VM_IsGenericNativeModuleName( const char *moduleName ) {
	return Q_stricmp( moduleName, "ui" ) == 0 ||
		Q_stricmp( moduleName, "game" ) == 0 ||
		Q_stricmp( moduleName, "cgame" ) == 0 ||
		Q_stricmp( moduleName, "qagame" ) == 0 ||
		Q_stricmp( moduleName, "frontend" ) == 0 ||
		Q_stricmp( moduleName, "client" ) == 0 ||
		Q_stricmp( moduleName, "server" ) == 0;
}

static int VM_AppendNativeModuleName( char out[][MAX_QPATH], int maxModules, int count, const char *moduleName ) {
	if ( count >= maxModules ) {
		return count;
	}

	VM_CopyNativeModuleName( out[count], moduleName );
	return count + 1;
}

int VM_BuildNativeModuleLoadOrder( const char *moduleName, char out[][MAX_QPATH], int maxModules ) {
	int count = 0;

	if ( !moduleName || !moduleName[0] || !out || maxModules <= 0 ) {
		return 0;
	}

	if ( !VM_IsGenericNativeModuleName( moduleName ) ) {
		return 0;
	}

	count = VM_AppendNativeModuleName( out, maxModules, count, moduleName );

	/* qagame historically probes game.*, and this fork also supports server.*. */
	if ( Q_stricmp( moduleName, "qagame" ) == 0 ) {
		count = VM_AppendNativeModuleName( out, maxModules, count, "game" );
		count = VM_AppendNativeModuleName( out, maxModules, count, "server" );
	} else if ( Q_stricmp( moduleName, "game" ) == 0 ) {
		count = VM_AppendNativeModuleName( out, maxModules, count, "server" );
	} else if ( Q_stricmp( moduleName, "cgame" ) == 0 ) {
		count = VM_AppendNativeModuleName( out, maxModules, count, "client" );
	} else if ( Q_stricmp( moduleName, "ui" ) == 0 ) {
		count = VM_AppendNativeModuleName( out, maxModules, count, "frontend" );
	} else if ( Q_stricmp( moduleName, "server" ) == 0 ) {
		count = VM_AppendNativeModuleName( out, maxModules, count, "game" );
	} else if ( Q_stricmp( moduleName, "client" ) == 0 ) {
		count = VM_AppendNativeModuleName( out, maxModules, count, "cgame" );
	} else if ( Q_stricmp( moduleName, "frontend" ) == 0 ) {
		count = VM_AppendNativeModuleName( out, maxModules, count, "ui" );
	}

	return count;
}

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
