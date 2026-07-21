/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Steam platform integration.
Provides Steam API initialization, overlay support, achievement tracking,
Steam Deck detection, and Steam Input status.
Compiled with full API when USE_STEAM is defined and steam_api is available;
otherwise all entry points are stubs.
===========================================================================
*/

#ifndef CL_STEAM_H
#define CL_STEAM_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void        Steam_Init( void );
void        Steam_Shutdown( void );
void        Steam_Frame( void );

qboolean    Steam_IsInitialized( void );
qboolean    Steam_IsSteamDeck( void );
qboolean    Steam_IsOverlayActive( void );

uint64_t    Steam_GetSteamID( void );
const char *Steam_GetPersonaName( void );

void        Steam_SetAchievement( const char *name );
void        Steam_ClearAchievement( const char *name );
qboolean    Steam_GetAchievement( const char *name );
void        Steam_IndicateAchievementProgress( const char *name, uint32_t current, uint32_t max );
void        Steam_SetRichPresence( const char *key, const char *value );

int         Steam_GetConnectedControllerCount( void );

#ifdef __cplusplus
}
#endif

#endif /* CL_STEAM_H */
