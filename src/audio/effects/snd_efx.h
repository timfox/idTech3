/*
===========================================================================
Heuristic Acoustics (OpenAL)
===========================================================================
Runtime-only acoustics estimation for listener environment. Uses lightweight
raycasts to derive room metrics and, when OpenAL EFX is available, applies a
blended reverb preset and optional per-source occlusion filtering.
===========================================================================
*/

#ifndef S_ACOUSTICS_H
#define S_ACOUSTICS_H

#include "../../qcommon/q_shared.h"

#ifdef USE_OPENAL
#include <AL/al.h>

void S_Acoustics_Init( void );
void S_Acoustics_Shutdown( void );
void S_Acoustics_Reset( void );
void S_Acoustics_Frame( const vec3_t listenerOrigin, const vec3_t listenerForward,
	const vec3_t listenerRight, const vec3_t listenerUp );

float S_Acoustics_SourceOcclusion( const vec3_t srcPos, const vec3_t listenerPos );
void S_Acoustics_ApplySource( ALuint source, float baseGain, const vec3_t srcPos );

#endif

#endif
