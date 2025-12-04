/*
===========================================================================
Steam Deck Integration
Steamworks SDK integration for Steam Deck features
===========================================================================
*/

#ifndef __CL_STEAMDECK_H__
#define __CL_STEAMDECK_H__

#include "../qcommon/q_shared.h"

#ifdef USE_STEAMWORKS
#include "steam/steam_api.h"
#endif

// CVar for simultaneous input (defined in cl_steamdeck.c)
extern cvar_t *cl_steamdeck_simultaneous_input;

// Steam Deck detection
qboolean CL_SteamDeck_Detect( void );
qboolean CL_SteamDeck_IsSteamDeck( void );
qboolean CL_SteamDeck_IsDocked( void );

// Steam Input text input
qboolean CL_SteamDeck_ShowTextInput( const char *description, const char *existingText, int maxChars, qboolean multiline );
qboolean CL_SteamDeck_ShowFloatingTextInput( int x, int y, int width, int height );
qboolean CL_SteamDeck_IsTextInputActive( void );
void CL_SteamDeck_GetTextInputResult( char *buffer, int bufferSize );

// Steam Cloud saves
qboolean CL_SteamDeck_CloudSave_Init( void );
qboolean CL_SteamDeck_CloudSave_Write( const char *filename, const void *data, int dataSize );
qboolean CL_SteamDeck_CloudSave_Read( const char *filename, void *data, int maxSize, int *outSize );
qboolean CL_SteamDeck_CloudSave_FileExists( const char *filename );
void CL_SteamDeck_CloudSave_Shutdown( void );

// Steam Input API
qboolean CL_SteamDeck_SteamInput_Init( void );
void CL_SteamDeck_SteamInput_Shutdown( void );
void CL_SteamDeck_SteamInput_RunFrame( void );

// Initialization and shutdown
void CL_SteamDeck_Init( void );
void CL_SteamDeck_Shutdown( void );
void CL_SteamDeck_RunFrame( void );

#endif // __CL_STEAMDECK_H__

