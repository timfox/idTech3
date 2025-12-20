/*
===========================================================================
Assert System - Configurable assertion handling

Assert types:
- Q_ASSERT_HARD: Always fatal, abort immediately
- Q_ASSERT_SOFT: Log error, continue execution (for recoverable issues)
- Q_ASSERT_ONCE: Fire once per location, then suppress
- Q_ASSERT_DEBUG: Only enabled in debug builds

Configuration via cvars:
- assert_mode: 0=disabled, 1=log only, 2=break, 3=fatal
- assert_break_on_fail: trigger debugger breakpoint
===========================================================================
*/

#ifndef __Q_ASSERT_H__
#define __Q_ASSERT_H__

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// Assert modes
typedef enum {
    ASSERT_MODE_DISABLED = 0,   // Assertions disabled
    ASSERT_MODE_LOG = 1,        // Log and continue
    ASSERT_MODE_BREAK = 2,      // Break into debugger
    ASSERT_MODE_FATAL = 3       // Abort execution
} assertMode_t;

// Assert severity
typedef enum {
    ASSERT_HARD,    // Always fatal regardless of mode
    ASSERT_SOFT,    // Respects assert_mode cvar
    ASSERT_ONCE,    // Only fires once per location
    ASSERT_DEBUG    // Only in debug builds
} assertSeverity_t;

// Initialize assert system (call after cvars are available)
void Assert_Init(void);

// Core assert function - use macros below instead
qboolean Assert_Check(qboolean condition, assertSeverity_t severity,
                      const char *condition_str, const char *file, int line,
                      const char *func, const char *msg, ...);

// Check if an assert at this location has already fired (for ASSERT_ONCE)
qboolean Assert_HasFired(const char *file, int line);

// Reset all "once" asserts (useful for testing)
void Assert_ResetOnce(void);

// Get/set assert mode
assertMode_t Assert_GetMode(void);
void Assert_SetMode(assertMode_t mode);

// Debugger break helper
void Assert_DebugBreak(void);

//
// Assert macros
//

// Hard assert - always fatal
#define Q_ASSERT_HARD(cond) \
    do { \
        if (!(cond)) { \
            Assert_Check(qfalse, ASSERT_HARD, #cond, __FILE__, __LINE__, __func__, NULL); \
        } \
    } while(0)

#define Q_ASSERT_HARD_MSG(cond, ...) \
    do { \
        if (!(cond)) { \
            Assert_Check(qfalse, ASSERT_HARD, #cond, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } \
    } while(0)

// Soft assert - recoverable, respects assert_mode
#define Q_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            Assert_Check(qfalse, ASSERT_SOFT, #cond, __FILE__, __LINE__, __func__, NULL); \
        } \
    } while(0)

#define Q_ASSERT_MSG(cond, ...) \
    do { \
        if (!(cond)) { \
            Assert_Check(qfalse, ASSERT_SOFT, #cond, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } \
    } while(0)

// Once assert - fires only once per location
#define Q_ASSERT_ONCE(cond) \
    do { \
        if (!(cond) && !Assert_HasFired(__FILE__, __LINE__)) { \
            Assert_Check(qfalse, ASSERT_ONCE, #cond, __FILE__, __LINE__, __func__, NULL); \
        } \
    } while(0)

#define Q_ASSERT_ONCE_MSG(cond, ...) \
    do { \
        if (!(cond) && !Assert_HasFired(__FILE__, __LINE__)) { \
            Assert_Check(qfalse, ASSERT_ONCE, #cond, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } \
    } while(0)

// Debug assert - only in debug builds
#ifdef _DEBUG
#define Q_ASSERT_DEBUG(cond) \
    do { \
        if (!(cond)) { \
            Assert_Check(qfalse, ASSERT_DEBUG, #cond, __FILE__, __LINE__, __func__, NULL); \
        } \
    } while(0)

#define Q_ASSERT_DEBUG_MSG(cond, ...) \
    do { \
        if (!(cond)) { \
            Assert_Check(qfalse, ASSERT_DEBUG, #cond, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } \
    } while(0)
#else
#define Q_ASSERT_DEBUG(cond) ((void)0)
#define Q_ASSERT_DEBUG_MSG(cond, ...) ((void)0)
#endif

// Unreachable code marker
#define Q_UNREACHABLE() \
    do { \
        Assert_Check(qfalse, ASSERT_HARD, "unreachable code", __FILE__, __LINE__, __func__, \
                     "Reached supposedly unreachable code"); \
    } while(0)

// Not implemented marker
#define Q_NOT_IMPLEMENTED() \
    do { \
        Assert_Check(qfalse, ASSERT_SOFT, "not implemented", __FILE__, __LINE__, __func__, \
                     "Function not yet implemented"); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // __Q_ASSERT_H__

