/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#ifndef CL_ENGINE_DECALS_H
#define CL_ENGINE_DECALS_H

#include "../qcommon/q_shared.h"
#include "../renderers/common/tr_types.h"

void CL_EngineDecals_Init( void );
void CL_EngineDecals_AddFromSnapshot( void );
void CL_EngineDecal_AddLocal( const engineDecalDesc_t *desc );
void CL_EngineDecal_AddLocalAtTime( const engineDecalDesc_t *desc, int timeMs );

#endif /* CL_ENGINE_DECALS_H */
