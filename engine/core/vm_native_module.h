#ifndef VM_NATIVE_MODULE_H
#define VM_NATIVE_MODULE_H

#include "q_shared.h"

/*
 * Builds native VM module filename candidates in priority order:
 *   1) module.so
 *   2) module.<arch><dll_ext>   (for example: client.aarch64.so)
 *   3) module<arch><dll_ext>    (for example: clientaarch64.so)
 */
int VM_BuildNativeModuleCandidates( const char *moduleName, char out[][MAX_QPATH], int maxCandidates );

#endif
