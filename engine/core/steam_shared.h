#ifndef STEAM_SHARED_H
#define STEAM_SHARED_H

#include "q_shared.h"

qboolean SteamShared_Init( void );
void SteamShared_Shutdown( void );
void SteamShared_Frame( void );

qboolean SteamShared_IsInitialized( void );
uint64_t SteamShared_GetSteamID( void );
const char *SteamShared_GetPersonaName( void );

#endif /* STEAM_SHARED_H */
