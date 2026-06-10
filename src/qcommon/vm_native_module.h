#ifndef VM_NATIVE_MODULE_H
#define VM_NATIVE_MODULE_H

#include <stdarg.h>
#include <stdint.h>

#include "q_shared.h"

/*
 * Builds native VM module filename candidates in priority order:
 *   1) module.so
 *   2) module.<arch><dll_ext>   (for example: client.aarch64.so)
 *   3) module<arch><dll_ext>    (for example: clientaarch64.so)
 */
int VM_BuildNativeModuleCandidates( const char *moduleName, char out[][MAX_QPATH], int maxCandidates );

/*
 * Packs native vmMain argument slots from VM_Call varargs. Native vmMain takes
 * exactly three integer argument slots after the command; missing slots must be
 * zero so low-arity calls such as UI_GETAPIVERSION cannot observe stack junk.
 */
void VM_BuildNativeModuleCallArgs( int nargs, int32_t *out, int maxArgs, va_list ap );

#endif
