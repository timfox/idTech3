/*
===========================================================================
Blacksun Feature Flags (C interface)

Lightweight C-callable API backed by a C++23 implementation. Lets gamecode
query named feature toggles without sprinkling globals everywhere.
===========================================================================
*/

#pragma once

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize defaults for Blacksun feature flags (idempotent).
void BS_Features_InitDefaults( void );

// Set a named feature on/off (creates if missing).
void BS_Features_Set( const char *name, qboolean enabled );

// Query a named feature; returns qfalse if unknown.
qboolean BS_Features_IsEnabled( const char *name );

#ifdef __cplusplus
}
#endif


