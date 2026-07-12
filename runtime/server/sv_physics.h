/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Server Soft Step lifecycle + misc_phys_* map spawn (Box3D).
Dedicated owns Phys_StepSimulation; listen servers leave ticking to the client.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void SV_Physics_Init( void );
void SV_Physics_Shutdown( void );
void SV_Physics_LoadMap( const char *entityString );
void SV_Physics_SpawnMapEntities( void );
void SV_Physics_Clear( void );
void SV_Physics_Frame( int frameMsec );

#ifdef __cplusplus
}
#endif
