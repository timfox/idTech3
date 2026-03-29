#ifndef VM_NATIVE_MODULE_NAMES_H
#define VM_NATIVE_MODULE_NAMES_H

#include <stddef.h>

/*
 * Returns the number of native module filename candidates generated for
 * VM library loading fallbacks.
 */
size_t VM_NativeModuleCandidateCount( void );

/*
 * Formats a native module filename candidate by index:
 *   0 -> module.so
 *   1 -> module.arch.dllExt
 *   2 -> modulearch.dllExt
 *
 * Writes an empty string on invalid input.
 */
void VM_FormatNativeModuleCandidate( char *out,
	size_t outSize,
	const char *moduleName,
	const char *archString,
	const char *dllExt,
	size_t candidateIndex );

#endif
