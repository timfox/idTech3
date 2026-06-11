#ifndef VM_NATIVE_MODULE_H
#define VM_NATIVE_MODULE_H

#include "qcommon.h"

typedef intptr_t (QDECL *vmNativeModuleEntryPoint_t)( int command, int arg0, int arg1, int arg2 );

/*
 * Builds native VM module filename candidates in priority order:
 *   1) module.so
 *   2) module.<arch><dll_ext>   (for example: client.aarch64.so)
 *   3) module<arch><dll_ext>    (for example: clientaarch64.so)
 */
int VM_BuildNativeModuleCandidates( const char *moduleName, char out[][MAX_QPATH], int maxCandidates );

/*
 * Builds logical native VM module names in the order loadNative() should probe
 * them before falling back to the legacy platform-specific filename.
 */
int VM_BuildNativeModuleLoadOrder( const char *moduleName, char out[][MAX_QPATH], int maxModules );

intptr_t VM_CallNativeModuleEntryPoint( vmNativeModuleEntryPoint_t entryPoint, int nargs, int callnum, va_list ap );

#endif
