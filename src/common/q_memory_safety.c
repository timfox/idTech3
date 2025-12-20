/*
===========================================================================
Memory Safety Utilities Implementation

Enhanced string and buffer operations with bounds checking and validation.
===========================================================================
*/

#include "q_memory_safety.h"
#include <stdarg.h>

/*
=================
Q_strncpyz_safe

Safe string copy with comprehensive validation
=================
*/
qboolean Q_strncpyz_safe(char *dest, const char *src, size_t destsize, const char *context)
{
    size_t srclen;

    // Validate parameters
    if (!dest) {
        Com_Printf("ERROR: Q_strncpyz_safe NULL dest in %s\n", context ? context : "unknown");
        return qfalse;
    }

    if (!src) {
        Com_Printf("WARNING: Q_strncpyz_safe NULL src in %s, setting dest empty\n",
                  context ? context : "unknown");
        if (destsize > 0) {
            dest[0] = '\0';
        }
        return qtrue;
    }

    if (destsize < 1) {
        Com_Printf("ERROR: Q_strncpyz_safe destsize < 1 (%zu) in %s\n",
                  destsize, context ? context : "unknown");
        return qfalse;
    }

    srclen = strlen(src);

    // Check for truncation
    if (srclen >= destsize) {
        Com_Printf("WARNING: String truncation in %s: source length %zu >= dest size %zu\n",
                  context ? context : "unknown", srclen, destsize);
    }

    // Perform the copy
    Q_strncpyz(dest, src, destsize);

    // Verify null termination
    if (destsize > 0 && dest[destsize - 1] != '\0') {
        Com_Printf("ERROR: String not properly null-terminated in %s\n",
                  context ? context : "unknown");
        dest[destsize - 1] = '\0';
        return qfalse;
    }

    return qtrue;
}

/*
=================
Q_strcat_safe

Safe string concatenation with validation
=================
*/
qboolean Q_strcat_safe(char *dest, const char *src, size_t destsize, const char *context)
{
    size_t destlen, srclen;

    // Validate parameters
    if (!dest) {
        Com_Printf("ERROR: Q_strcat_safe NULL dest in %s\n", context ? context : "unknown");
        return qfalse;
    }

    if (!src) {
        Com_Printf("WARNING: Q_strcat_safe NULL src in %s, no-op\n",
                  context ? context : "unknown");
        return qtrue;
    }

    if (destsize < 1) {
        Com_Printf("ERROR: Q_strcat_safe destsize < 1 (%zu) in %s\n",
                  destsize, context ? context : "unknown");
        return qfalse;
    }

    destlen = strlen(dest);
    srclen = strlen(src);

    // Check if concatenation would overflow
    if (destlen + srclen >= destsize) {
        Com_Printf("WARNING: String concatenation truncation in %s: %zu + %zu >= %zu\n",
                  context ? context : "unknown", destlen, srclen, destsize);
    }

    // Perform the concatenation
    Q_strcat(dest, destsize, src);

    // Verify null termination
    if (destsize > 0 && dest[destsize - 1] != '\0') {
        Com_Printf("ERROR: Concatenated string not properly null-terminated in %s\n",
                  context ? context : "unknown");
        dest[destsize - 1] = '\0';
        return qfalse;
    }

    return qtrue;
}

/*
=================
Q_snprintf_safe

Safe sprintf with buffer validation
=================
*/
int Q_snprintf_safe(char *dest, size_t destsize, const char *fmt, ...)
{
    va_list argptr;
    int result;

    if (!dest) {
        Com_Printf("ERROR: Q_snprintf_safe NULL dest\n");
        return -1;
    }

    if (destsize < 1) {
        Com_Printf("ERROR: Q_snprintf_safe destsize < 1 (%zu)\n", destsize);
        return -1;
    }

    va_start(argptr, fmt);
    result = Q_vsnprintf(dest, destsize, fmt, argptr);
    va_end(argptr);

    // Check for truncation
    if ((size_t)result >= destsize) {
        Com_Printf("WARNING: Q_snprintf_safe truncation: %d >= %zu\n", result, destsize);
        // Ensure null termination
        dest[destsize - 1] = '\0';
    }

    return result;
}