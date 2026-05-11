#include "vm_native_module.h"
#include <stdio.h>

static qboolean VM_IsGenericNativeModuleName( const char *moduleName ) {
	return Q_stricmp( moduleName, "ui" ) == 0 || Q_stricmp( moduleName, "game" ) == 0 ||
		Q_stricmp( moduleName, "cgame" ) == 0 || Q_stricmp( moduleName, "qagame" ) == 0 ||
		Q_stricmp( moduleName, "frontend" ) == 0 || Q_stricmp( moduleName, "client" ) == 0 ||
		Q_stricmp( moduleName, "server" ) == 0;
}

static void VM_AddNativeModuleName( char out[][MAX_QPATH], int maxModules, int *count, const char *moduleName ) {
	if ( *count >= maxModules ) {
		return;
	}

	(void)snprintf( out[*count], MAX_QPATH, "%s", moduleName );
	(*count)++;
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

int VM_BuildNativeModuleLoadOrder( const char *moduleName, char out[][MAX_QPATH], int maxModules ) {
	int count = 0;

	if ( !moduleName || !moduleName[0] || !out || maxModules <= 0 || !VM_IsGenericNativeModuleName( moduleName ) ) {
		return 0;
	}

	VM_AddNativeModuleName( out, maxModules, &count, moduleName );

	/* qagame historically aliases game; project-native names add server/client/frontend. */
	if ( Q_stricmp( moduleName, "qagame" ) == 0 ) {
		VM_AddNativeModuleName( out, maxModules, &count, "game" );
	}
	if ( Q_stricmp( moduleName, "qagame" ) == 0 || Q_stricmp( moduleName, "game" ) == 0 ) {
		VM_AddNativeModuleName( out, maxModules, &count, "server" );
	}
	if ( Q_stricmp( moduleName, "cgame" ) == 0 ) {
		VM_AddNativeModuleName( out, maxModules, &count, "client" );
	}
	if ( Q_stricmp( moduleName, "ui" ) == 0 ) {
		VM_AddNativeModuleName( out, maxModules, &count, "frontend" );
	}
	if ( Q_stricmp( moduleName, "server" ) == 0 ) {
		VM_AddNativeModuleName( out, maxModules, &count, "game" );
	}
	if ( Q_stricmp( moduleName, "client" ) == 0 ) {
		VM_AddNativeModuleName( out, maxModules, &count, "cgame" );
	}
	if ( Q_stricmp( moduleName, "frontend" ) == 0 ) {
		VM_AddNativeModuleName( out, maxModules, &count, "ui" );
	}

	return count;
}
