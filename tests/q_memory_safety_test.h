/*
===========================================================================
Memory Safety Test Header - minimal definitions
===========================================================================
*/

#ifndef __Q_MEMORY_SAFETY_TEST_H__
#define __Q_MEMORY_SAFETY_TEST_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

// Minimal type definitions
typedef int qboolean;
#define qtrue 1
#define qfalse 0

// Com_Printf is defined in test_framework_minimal.h

// Safe string copy with validation
qboolean Q_strncpyz_safe(char *dest, const char *src, size_t destsize, const char *context);

// Safe string concatenation
qboolean Q_strcat_safe(char *dest, const char *src, size_t destsize, const char *context);

// Safe sprintf
int Q_snprintf_safe(char *dest, size_t destsize, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

#endif
