/*
===========================================================================
q_shared_fuzz.h - Minimal shared definitions for fuzzing builds

This header provides only the absolutely necessary definitions from q_shared.h
for standalone fuzzing targets, avoiding engine-wide dependencies.
===========================================================================
*/

#ifndef __Q_SHARED_FUZZ_H__
#define __Q_SHARED_FUZZ_H__

#include <stddef.h> // For size_t
#include <stdint.h> // For uint8_t
#include <stdarg.h> // For va_list

// Basic types
typedef int qboolean;
#define qtrue 1
#define qfalse 0
typedef unsigned char byte;

// Minimal errorParm_t for Com_Error mock
typedef enum { ERR_FATAL, ERR_DROP, ERR_SERVERDISCONNECT, ERR_MEMSET } errorParm_t;

// Minimal string operations (if needed, otherwise rely on libc)
// In fuzzing, we prefer standard library functions where possible
// (they are usually fuzzer-aware or well-tested)
extern size_t Q_strncpyz( char *dest, const char *src, size_t destsize );
extern int Q_vsnprintf( char *dest, size_t size, const char *fmt, va_list argptr );

#endif // __Q_SHARED_FUZZ_H__
