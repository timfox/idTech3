/*
===========================================================================
Memory Safety Functions - Standalone test implementation
===========================================================================
*/

#include "q_memory_safety_test.h"

// Com_Printf implementation for this translation unit
void Com_Printf(const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    va_end(argptr);
}

// Minimal implementation of Q_strncpyz for testing
static void Q_strncpyz(char *dest, const char *src, size_t destsize) {
    if (!destsize) return;
    strncpy(dest, src, destsize - 1);
    dest[destsize - 1] = '\0';
}

// Minimal implementation of Q_strcat for testing
static void Q_strcat(char *dest, size_t destsize, const char *src) {
    size_t len = strlen(dest);
    if (len >= destsize) return;
    strncat(dest, src, destsize - len - 1);
}

// Minimal implementation of Q_vsnprintf for testing
static int Q_vsnprintf(char *dest, size_t destsize, const char *fmt, va_list argptr) {
    return vsnprintf(dest, destsize, fmt, argptr);
}

qboolean Q_strncpyz_safe(char *dest, const char *src, size_t destsize, const char *context)
{
    size_t srclen;

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

    if (srclen >= destsize) {
        Com_Printf("WARNING: String truncation in %s: source length %zu >= dest size %zu\n",
                  context ? context : "unknown", srclen, destsize);
    }

    Q_strncpyz(dest, src, destsize);
    return qtrue;
}

qboolean Q_strcat_safe(char *dest, const char *src, size_t destsize, const char *context)
{
    size_t destlen, srclen;

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

    if (destlen + srclen >= destsize) {
        Com_Printf("WARNING: String concatenation truncation in %s: %zu + %zu >= %zu\n",
                  context ? context : "unknown", destlen, srclen, destsize);
    }

    Q_strcat(dest, destsize, src);
    return qtrue;
}

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

    if ((size_t)result >= destsize) {
        Com_Printf("WARNING: Q_snprintf_safe truncation: %d >= %zu\n", result, destsize);
        dest[destsize - 1] = '\0';
    }

    return result;
}
