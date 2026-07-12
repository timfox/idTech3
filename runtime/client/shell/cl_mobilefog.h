/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mobile-friendly fog system for Android and low-end devices.
Provides three fog methods ranked by cost:

  1. Exponential height fog (vertex/fragment, nearly free)
  2. Sprite-based volumetric fog (particle billboards around camera)
  3. Material-based fog sphere (translucent sphere mesh)

These complement the full froxel volumetric fog (r_volumetricFog)
which is too expensive for most mobile GPUs.
===========================================================================
*/

#ifndef CL_MOBILEFOG_H
#define CL_MOBILEFOG_H

#include "q_shared.h"

typedef enum {
	MOBILE_FOG_NONE = 0,
	MOBILE_FOG_HEIGHT,
	MOBILE_FOG_SPRITES,
	MOBILE_FOG_FULL
} mobileFogMode_t;

void MobileFog_Init( void );
void MobileFog_Shutdown( void );
void MobileFog_Frame( const vec3_t viewOrigin, const vec3_t viewForward,
	const vec3_t viewRight, const vec3_t viewUp, float frametime );

mobileFogMode_t MobileFog_GetMode( void );

#endif /* CL_MOBILEFOG_H */
