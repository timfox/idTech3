/*
===========================================================================
Steam Deck Integration
Steamworks SDK integration for Steam Deck features
===========================================================================
*/

#ifndef __CL_STEAMDECK_H__
#define __CL_STEAMDECK_H__

#include "../common/q_shared.h"

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

// Controller glyphs
typedef enum {
    STEAMDECK_GLYPH_A,
    STEAMDECK_GLYPH_B,
    STEAMDECK_GLYPH_X,
    STEAMDECK_GLYPH_Y,
    STEAMDECK_GLYPH_L1,
    STEAMDECK_GLYPH_L2,
    STEAMDECK_GLYPH_R1,
    STEAMDECK_GLYPH_R2,
    STEAMDECK_GLYPH_LSTICK,
    STEAMDECK_GLYPH_RSTICK,
    STEAMDECK_GLYPH_DPAD_UP,
    STEAMDECK_GLYPH_DPAD_DOWN,
    STEAMDECK_GLYPH_DPAD_LEFT,
    STEAMDECK_GLYPH_DPAD_RIGHT,
    STEAMDECK_GLYPH_START,
    STEAMDECK_GLYPH_SELECT,
    STEAMDECK_GLYPH_LGRIP,
    STEAMDECK_GLYPH_RGRIP,
    STEAMDECK_GLYPH_LPAD_CLICK,
    STEAMDECK_GLYPH_RPAD_CLICK,
    STEAMDECK_GLYPH_LPAD_TOUCH,
    STEAMDECK_GLYPH_RPAD_TOUCH,
    STEAMDECK_GLYPH_QAM,
    STEAMDECK_GLYPH_MAX
} steamdeck_glyph_t;

qboolean CL_SteamDeck_Glyphs_Init( void );
void CL_SteamDeck_Glyphs_Shutdown( void );
qhandle_t CL_SteamDeck_GetGlyphTexture( steamdeck_glyph_t glyph );
void CL_SteamDeck_RenderGlyph( float x, float y, float scale, steamdeck_glyph_t glyph );
const char *CL_SteamDeck_GetGlyphName( steamdeck_glyph_t glyph );
qboolean CL_SteamDeck_IsGlyphAvailable( steamdeck_glyph_t glyph );

// Initialization and shutdown
void CL_SteamDeck_Init( void );
void CL_SteamDeck_Shutdown( void );
void CL_SteamDeck_RunFrame( void );

#endif // __CL_STEAMDECK_H__

