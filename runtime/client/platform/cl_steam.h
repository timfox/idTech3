/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Steam platform integration.
Provides Steam API initialization, overlay support, achievement tracking,
Steam Deck detection, and Steam Input configuration.
Compiled only when USE_STEAM is defined and steam_api is available.
===========================================================================
*/

#ifndef CL_STEAM_H
#define CL_STEAM_H

#include "../../qcommon/q_shared.h"

void        Steam_Init( void );
void        Steam_Shutdown( void );
void        Steam_Frame( void );

qboolean    Steam_IsInitialized( void );
qboolean    Steam_IsSteamDeck( void );
qboolean    Steam_IsOverlayActive( void );

uint64_t    Steam_GetSteamID( void );
const char *Steam_GetPersonaName( void );

void        Steam_SetAchievement( const char *name );
void        Steam_SetRichPresence( const char *key, const char *value );

#endif /* CL_STEAM_H */
