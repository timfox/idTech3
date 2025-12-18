/*
===========================================================================
qcommon_fuzz.h - Minimal common definitions for fuzzing builds

This header provides only the absolutely necessary definitions from qcommon.h
for standalone fuzzing targets, avoiding engine-wide dependencies.
===========================================================================
*/

#ifndef __QCOMMON_FUZZ_H__
#define __QCOMMON_FUZZ_H__

#include "q_shared_fuzz.h"

// Minimal cvar_t definition to satisfy includes if needed
typedef struct cvar_s {
    char        *name;
    char        *string;
    char        *resetString;   // cvar_restart will use this
    char        *latchedString; // for CVAR_LATCH
    int         flags;
    qboolean    modified;       // set when anything this cvar depends on is changed
    int         value;
    float       fvalue;
    int         integer;
    float       min_value;
    float       max_value;
    struct cvar_s *next;
    char        *description;   // brief description of the cvar
    char        *longDescription; // detailed description of the cvar
} cvar_t;

#define CVAR_ARCHIVE        0x0001  // don't write to config.cfg
#define CVAR_USERINFO       0x0002  // sent to servers, used for email etc
#define CVAR_SERVERINFO     0x0004  // sent to servers, but not to clients
#define CVAR_SYSTEMINFO     0x0008  // these cvars will be sent also to clients
#define CVAR_INIT           0x0010  // can only be set from the command line / autoexec
#define CVAR_LATCH          0x0020  // set once, but doesn't take effect until restart
#define CVAR_ROM            0x0040  // display only, cannot be set by user at all
#define CVAR_UNSAFE         0x0080  // will not be saved to config.cfg and can be reverted by server/client
#define CVAR_NOUNSET        0x0100  // not allowed to unset (only the value will be preserved)
#define CVAR_CHEAT          0x0200  // cheats
#define CVAR_TEMPORARY      0x0400  // don't write to config.cfg, useful for e.g. startup parameter storage
#define CVAR_NORESTART      0x0800  // won't be saved to config.cfg (similar to CVAR_ARCHIVE, but in code means don't save to cvar)
#define CVAR_PROTECTED      0x1000  // cannot be set from console (unless developer is 1), can only be set from commandline and autoexec.cfg
#define CVAR_INTEGER        0x2000  // only integer values will be considered
#define CVAR_GUID           0x4000  // GUID (Globally Unique Identifier)

// Minimal msg_t definition for fuzzing
typedef struct msg_s {
    int             maxsize;
    int             cursize;
    int             readcount;      // number of bits processed in current msg
    int             bit;
    byte            *data;
    qboolean        allowoverflow;  // if overflowed, set this flag
    qboolean        overflowed;     // set when the buffer can't hold any more data
} msg_t;

// Minimal Com_Printf/Com_Error (already defined in fuzz_msg.c)
extern void Com_Printf(const char *fmt, ...);
extern void Com_Error(errorParm_t level, const char *fmt, ...);

// Minimal Sys_Milliseconds (needed by msg.c)
extern long long Sys_Milliseconds( void );

#endif // __QCOMMON_FUZZ_H__
