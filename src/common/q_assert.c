/*
===========================================================================
Assert System Implementation
===========================================================================
*/

#include "q_assert.h"
#include "qcommon.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Track "once" assert locations
#define MAX_ONCE_ASSERTS 256

typedef struct {
    const char *file;
    int line;
} once_assert_t;

static once_assert_t g_once_asserts[MAX_ONCE_ASSERTS];
static int g_once_assert_count = 0;

static assertMode_t g_assert_mode = ASSERT_MODE_LOG;
static cvar_t *assert_mode_cvar = NULL;
static cvar_t *assert_break_on_fail = NULL;

/*
=================
Assert_Init
=================
*/
void Assert_Init(void)
{
    // Use centralized cvar for consistency
    extern cvar_t *com_assertLevel;
    assert_mode_cvar = com_assertLevel;

    // Keep legacy cvar for backwards compatibility
    assert_break_on_fail = Cvar_Get("assert_break_on_fail", "0", CVAR_ARCHIVE);
    Cvar_SetDescription(assert_break_on_fail,
        "Break into debugger on assert failure");

    if (assert_mode_cvar) {
        g_assert_mode = (assertMode_t)assert_mode_cvar->integer;
    } else {
        g_assert_mode = ASSERT_MODE_LOG; // fallback
    }

    Com_Printf("Assert system initialized (mode: %d)\n", g_assert_mode);
}
/*
=================
Assert_GetMode
=================
*/
assertMode_t Assert_GetMode(void)
{
    if (assert_mode_cvar) {
        return (assertMode_t)assert_mode_cvar->integer;
    }
    return g_assert_mode;
}

/*
=================
Assert_SetMode
=================
*/
void Assert_SetMode(assertMode_t mode)
{
    g_assert_mode = mode;
    if (assert_mode_cvar) {
        Cvar_SetValue("assert_mode", (float)mode);
    }
}

/*
=================
Assert_DebugBreak
=================
*/
void Assert_DebugBreak(void)
{
#if defined(_WIN32)
    __debugbreak();
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    __asm__ volatile("int $3");
#elif defined(__GNUC__) && defined(__aarch64__)
    __asm__ volatile("brk #0");
#else
    // Fallback: raise SIGTRAP
    raise(SIGTRAP);
#endif
}

/*
=================
Assert_HasFired

Check if a "once" assert at this location has already fired
=================
*/
qboolean Assert_HasFired(const char *file, int line)
{
    int i;

    for (i = 0; i < g_once_assert_count; i++) {
        if (g_once_asserts[i].line == line &&
            strcmp(g_once_asserts[i].file, file) == 0) {
            return qtrue;
        }
    }
    return qfalse;
}

/*
=================
Assert_MarkFired

Record that an assert at this location has fired
=================
*/
static void Assert_MarkFired(const char *file, int line)
{
    if (g_once_assert_count < MAX_ONCE_ASSERTS) {
        g_once_asserts[g_once_assert_count].file = file;
        g_once_asserts[g_once_assert_count].line = line;
        g_once_assert_count++;
    }
}

/*
=================
Assert_ResetOnce
=================
*/
void Assert_ResetOnce(void)
{
    g_once_assert_count = 0;
}

/*
=================
Assert_Check

Core assertion check function
=================
*/
qboolean Assert_Check(qboolean condition, assertSeverity_t severity,
                      const char *condition_str, const char *file, int line,
                      const char *func, const char *msg, ...)
{
    char full_msg[1024];
    char user_msg[512];
    assertMode_t mode;

    if (condition) {
        return qtrue;  // Assertion passed
    }

    // Get current mode
    mode = Assert_GetMode();

    // Check if disabled (except for hard asserts)
    if (mode == ASSERT_MODE_DISABLED && severity != ASSERT_HARD) {
        return qfalse;
    }

    // Build user message if provided
    if (msg) {
        va_list argptr;
        va_start(argptr, msg);
        Q_vsnprintf(user_msg, sizeof(user_msg), msg, argptr);
        va_end(argptr);
    } else {
        user_msg[0] = '\0';
    }

    // Build full message
    if (user_msg[0]) {
        Com_sprintf(full_msg, sizeof(full_msg),
                    "ASSERT FAILED: %s\n  Condition: %s\n  Location: %s:%d (%s)\n  Message: %s",
                    (severity == ASSERT_HARD) ? "[HARD]" :
                    (severity == ASSERT_SOFT) ? "[SOFT]" :
                    (severity == ASSERT_ONCE) ? "[ONCE]" : "[DEBUG]",
                    condition_str, file, line, func, user_msg);
    } else {
        Com_sprintf(full_msg, sizeof(full_msg),
                    "ASSERT FAILED: %s\n  Condition: %s\n  Location: %s:%d (%s)",
                    (severity == ASSERT_HARD) ? "[HARD]" :
                    (severity == ASSERT_SOFT) ? "[SOFT]" :
                    (severity == ASSERT_ONCE) ? "[ONCE]" : "[DEBUG]",
                    condition_str, file, line, func);
    }

    // Log the assertion
    Com_Printf("^1%s\n", full_msg);

    // Mark "once" asserts
    if (severity == ASSERT_ONCE) {
        Assert_MarkFired(file, line);
    }

    // Handle based on severity and mode
    if (severity == ASSERT_HARD) {
        // Hard asserts are always fatal
        Com_Error(ERR_FATAL, "%s", full_msg);
        return qfalse;  // Never reached
    }

    // Check for debug break
    if (assert_break_on_fail && assert_break_on_fail->integer) {
        Assert_DebugBreak();
    }

    switch (mode) {
        case ASSERT_MODE_BREAK:
            Assert_DebugBreak();
            break;

        case ASSERT_MODE_FATAL:
            Com_Error(ERR_DROP, "%s", full_msg);
            break;

        case ASSERT_MODE_LOG:
        case ASSERT_MODE_DISABLED:
        default:
            // Just log and continue
            break;
    }

    return qfalse;
}

