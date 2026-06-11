#include "vm_native_module.h"
#include <stdio.h>

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

	if ( !moduleName || !moduleName[0] || !out || maxModules <= 0 ) {
		return 0;
	}

	if ( Q_stricmp( moduleName, "ui" ) != 0 && Q_stricmp( moduleName, "game" ) != 0 &&
		 Q_stricmp( moduleName, "cgame" ) != 0 && Q_stricmp( moduleName, "qagame" ) != 0 &&
		 Q_stricmp( moduleName, "frontend" ) != 0 && Q_stricmp( moduleName, "client" ) != 0 &&
		 Q_stricmp( moduleName, "server" ) != 0 ) {
		return 0;
	}

	VM_AddNativeModuleName( out, maxModules, &count, moduleName );

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

intptr_t VM_CallNativeModuleEntryPoint( vmNativeModuleEntryPoint_t entryPoint, int nargs, int callnum, va_list ap ) {
	int32_t args[MAX_VMMAIN_CALL_ARGS - 1];
	int i;

	Com_Memset( args, 0, sizeof( args ) );
	for ( i = 0; i < nargs; i++ ) {
		args[i] = va_arg( ap, int32_t );
	}

	return entryPoint( callnum, args[0], args[1], args[2] );
}
