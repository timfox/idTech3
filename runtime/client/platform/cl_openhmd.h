/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

OpenHMD client integration: head tracking + stereo eye separation.
Loads libopenhmd at runtime (dlopen) so builds succeed without the SDK.
===========================================================================
*/
#ifndef CL_OPENHMD_H
#define CL_OPENHMD_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void     OHMD_Init( void );
void     OHMD_Shutdown( void );
void     OHMD_Frame( void );

/* True when an HMD is open and vr_openhmd is enabled. */
qboolean OHMD_IsActive( void );
/* Dual-eye SCR path (software stereo via r_stereoSeparation). */
qboolean OHMD_WantStereo( void );
/* When active and mouse look should be suppressed. */
qboolean OHMD_BlockMouseLook( void );

/* Apply HMD orientation into cl.viewangles (after keyboard, before/after mouse). */
void     OHMD_ApplyViewAngles( void );

float    OHMD_GetIPD( void );
void     OHMD_GetOrientationEuler( vec3_t anglesOut );

#ifdef __cplusplus
}
#endif

#endif
