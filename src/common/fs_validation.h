#ifndef FS_VALIDATION_H
#define FS_VALIDATION_H

/*
 * Shared API surface for content validation during startup.
 * This header defines the result struct and the core validation hooks
 * used by the engine startup sequence.
 */

#include "qcommon.h"

// fs_validation_result_t is defined in the core qcommon/q_shared headers.
// Use the type directly as defined there for cross-module compatibility.

// Validate a single pak file's integrity (returns true if valid)
qboolean FS_ValidatePakFile( const char *pakPath, char *errorMsg, int errorMsgSize );

// Validate overall game content (base content, mods, paks). Result captured via output param
qboolean FS_ValidateGameContent( fs_validation_result_t *result );

// Validate a single mod (by name). Result captured via output param
qboolean FS_ValidateMod( const char *modName, fs_validation_result_t *result );

// Surface any missing/invalid content information for diagnostics
void FS_ReportMissingContent( fs_validation_result_t *result );

// Run startup-time content validation (paks, base content, mods)
qboolean FS_ValidateContentOnStartup( void );

// Validate a specific mod before loading (returns true if loadable)
qboolean Mod_ValidateBeforeLoad( const char *modName );

#endif // FS_VALIDATION_H

