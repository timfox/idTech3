/*
===========================================================================
Steam Deck Integration Implementation
Steamworks SDK integration for Steam Deck features
===========================================================================
*/

#include "client.h"
#include "cl_steamdeck.h"
#include "../renderercommon/tr_public.h"  // For re (renderer interface)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <unistd.h>
#endif

#ifdef USE_STEAMWORKS
#include "steam/steam_api.h"
#endif

// CVars
static cvar_t *cl_steamdeck_enable;
static cvar_t *cl_steamdeck_textinput;
static cvar_t *cl_steamdeck_cloudsaves;
cvar_t *cl_steamdeck_simultaneous_input;  // Non-static so cl_input.c can access it

// State
static qboolean steamdeck_initialized = qfalse;
static qboolean steamdeck_is_steamdeck = qfalse;
static qboolean steamdeck_is_docked = qfalse;
static qboolean steamdeck_textinput_active = qfalse;
static char steamdeck_textinput_buffer[4096];

// Glyph system state
static qboolean steamdeck_glyphs_initialized = qfalse;
static qhandle_t steamdeck_glyph_textures[STEAMDECK_GLYPH_MAX];
static qboolean steamdeck_glyph_available[STEAMDECK_GLYPH_MAX];

// Glyph texture file mapping
static const char *steamdeck_glyph_files[STEAMDECK_GLYPH_MAX] = {
    "steamdeck/glyph_a.tga",        // STEAMDECK_GLYPH_A
    "steamdeck/glyph_b.tga",        // STEAMDECK_GLYPH_B
    "steamdeck/glyph_x.tga",        // STEAMDECK_GLYPH_X
    "steamdeck/glyph_y.tga",        // STEAMDECK_GLYPH_Y
    "steamdeck/glyph_l1.tga",       // STEAMDECK_GLYPH_L1
    "steamdeck/glyph_l2.tga",       // STEAMDECK_GLYPH_L2
    "steamdeck/glyph_r1.tga",       // STEAMDECK_GLYPH_R1
    "steamdeck/glyph_r2.tga",       // STEAMDECK_GLYPH_R2
    "steamdeck/glyph_lstick.tga",   // STEAMDECK_GLYPH_LSTICK
    "steamdeck/glyph_rstick.tga",   // STEAMDECK_GLYPH_RSTICK
    "steamdeck/glyph_dpad_up.tga",  // STEAMDECK_GLYPH_DPAD_UP
    "steamdeck/glyph_dpad_down.tga", // STEAMDECK_GLYPH_DPAD_DOWN
    "steamdeck/glyph_dpad_left.tga", // STEAMDECK_GLYPH_DPAD_LEFT
    "steamdeck/glyph_dpad_right.tga", // STEAMDECK_GLYPH_DPAD_RIGHT
    "steamdeck/glyph_start.tga",    // STEAMDECK_GLYPH_START
    "steamdeck/glyph_select.tga",   // STEAMDECK_GLYPH_SELECT
    "steamdeck/glyph_lgrip.tga",    // STEAMDECK_GLYPH_LGRIP
    "steamdeck/glyph_rgrip.tga",    // STEAMDECK_GLYPH_RGRIP
    "steamdeck/glyph_lpad_click.tga", // STEAMDECK_GLYPH_LPAD_CLICK
    "steamdeck/glyph_rpad_click.tga", // STEAMDECK_GLYPH_RPAD_CLICK
    "steamdeck/glyph_lpad_touch.tga", // STEAMDECK_GLYPH_LPAD_TOUCH
    "steamdeck/glyph_rpad_touch.tga", // STEAMDECK_GLYPH_RPAD_TOUCH
    "steamdeck/glyph_qam.tga"       // STEAMDECK_GLYPH_QAM
};

// Glyph display names
static const char *steamdeck_glyph_names[STEAMDECK_GLYPH_MAX] = {
    "A", "B", "X", "Y", "L1", "L2", "R1", "R2",
    "Left Stick", "Right Stick", "DPad Up", "DPad Down",
    "DPad Left", "DPad Right", "Start", "Select",
    "Left Grip", "Right Grip", "Left Pad Click", "Right Pad Click",
    "Left Pad Touch", "Right Pad Touch", "Quick Access Menu"
};

#ifdef USE_STEAMWORKS
static ISteamUtils *steam_utils = NULL;
static ISteamInput *steam_input = NULL;
static ISteamRemoteStorage *steam_remote_storage = NULL;
#endif

/*
================
CL_SteamDeck_Detect
Detect if running on Steam Deck hardware
================
*/
qboolean CL_SteamDeck_Detect( void )
{
#ifdef __linux__
	FILE *f = fopen( "/sys/devices/virtual/dmi/id/product_name", "r" );
	if ( f ) {
		char product[256];
		if ( fgets( product, sizeof( product ), f ) ) {
			if ( strstr( product, "Jupiter" ) || strstr( product, "Steam Deck" ) ) {
				fclose( f );
				steamdeck_is_steamdeck = qtrue;
				Com_Printf( "Steam Deck hardware detected\n" );
				return qtrue;
			}
		}
		fclose( f );
	}
	
	// Also check for Steam runtime environment
	if ( getenv( "SteamDeck" ) || getenv( "STEAMDECK" ) ) {
		steamdeck_is_steamdeck = qtrue;
		Com_Printf( "Steam Deck environment detected\n" );
		return qtrue;
	}
#endif
	
	return qfalse;
}

/*
================
CL_SteamDeck_IsSteamDeck
Check if running on Steam Deck
================
*/
qboolean CL_SteamDeck_IsSteamDeck( void )
{
	return steamdeck_is_steamdeck;
}

/*
================
CL_SteamDeck_IsDocked
Check if Steam Deck is docked (external display connected)
================
*/
qboolean CL_SteamDeck_IsDocked( void )
{
	if ( !steamdeck_is_steamdeck ) {
		return qfalse;
	}
	
#ifdef __linux__
	// Check for external displays via DRM
	FILE *f = fopen( "/sys/class/drm/card0-DP-*/status", "r" );
	if ( !f ) {
		f = fopen( "/sys/class/drm/card0-HDMI-*/status", "r" );
	}
	if ( f ) {
		char status[16];
		if ( fgets( status, sizeof( status ), f ) ) {
			fclose( f );
			if ( !Q_stricmp( status, "connected\n" ) ) {
				steamdeck_is_docked = qtrue;
				return qtrue;
			}
		} else {
			fclose( f );
		}
	}
	
	// Fallback: check if resolution is not native 1280x800
	// cls is declared in client.h
	if ( cls.glconfig.vidWidth != 1280 || cls.glconfig.vidHeight != 800 ) {
		steamdeck_is_docked = qtrue;
		return qtrue;
	}
#endif
	
	return qfalse;
}

/*
================
CL_SteamDeck_ShowTextInput
Show Steam Input on-screen keyboard (callback-based)
================
*/
qboolean CL_SteamDeck_ShowTextInput( const char *description, const char *existingText, int maxChars, qboolean multiline )
{
	(void)description;
	(void)existingText;
	(void)maxChars;
	(void)multiline;
	
	if ( !cl_steamdeck_textinput->integer ) {
		return qfalse;
	}
	
#ifdef USE_STEAMWORKS
	if ( !steam_input || !SteamInput() ) {
		return qfalse;
	}
	
	EGamepadTextInputMode inputMode = multiline ? k_EGamepadTextInputModeModeMultiline : k_EGamepadTextInputModeModeSingleLine;
	EGamepadTextInputLineMode lineMode = k_EGamepadTextInputLineModeSingleLine;
	
	if ( SteamInput()->ShowGamepadTextInput( inputMode, lineMode, description, maxChars, existingText ? existingText : "" ) ) {
		steamdeck_textinput_active = qtrue;
		steamdeck_textinput_buffer[0] = '\0';
		return qtrue;
	}
#endif
	
	return qfalse;
}

/*
================
CL_SteamDeck_ShowFloatingTextInput
Show floating on-screen keyboard (direct key input)
================
*/
qboolean CL_SteamDeck_ShowFloatingTextInput( int x, int y, int width, int height )
{
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	
	if ( !cl_steamdeck_textinput->integer ) {
		return qfalse;
	}
	
#ifdef USE_STEAMWORKS
	if ( !steam_utils || !SteamUtils() ) {
		return qfalse;
	}
	
	if ( SteamUtils()->ShowFloatingGamepadTextInput( k_EFloatingGamepadTextInputModeModeSingleLine, x, y, width, height ) ) {
		steamdeck_textinput_active = qtrue;
		return qtrue;
	}
#endif
	
	return qfalse;
}

/*
================
CL_SteamDeck_IsTextInputActive
Check if text input is currently active
================
*/
qboolean CL_SteamDeck_IsTextInputActive( void )
{
	return steamdeck_textinput_active;
}

/*
================
CL_SteamDeck_GetTextInputResult
Get text input result (for callback-based input)
================
*/
void CL_SteamDeck_GetTextInputResult( char *buffer, int bufferSize )
{
	if ( !buffer || bufferSize <= 0 ) {
		return;
	}
	
	Q_strncpyz( buffer, steamdeck_textinput_buffer, bufferSize );
}

/*
================
CL_SteamDeck_CloudSave_Init
Initialize Steam Cloud save system
================
*/
qboolean CL_SteamDeck_CloudSave_Init( void )
{
	if ( !cl_steamdeck_cloudsaves->integer ) {
		return qfalse;
	}
	
#ifdef USE_STEAMWORKS
	if ( SteamRemoteStorage() ) {
		steam_remote_storage = SteamRemoteStorage();
		if ( steam_remote_storage->IsCloudEnabledForAccount() && steam_remote_storage->IsCloudEnabledForApp() ) {
			Com_Printf( "Steam Cloud saves enabled\n" );
			return qtrue;
		}
	}
#endif
	
	return qfalse;
}

/*
================
CL_SteamDeck_CloudSave_Write
Write save file to Steam Cloud
================
*/
qboolean CL_SteamDeck_CloudSave_Write( const char *filename, const void *data, int dataSize )
{
#ifdef USE_STEAMWORKS
	if ( !steam_remote_storage || !data || dataSize <= 0 ) {
		return qfalse;
	}
	
	char cloudPath[MAX_QPATH];
	Com_sprintf( cloudPath, sizeof( cloudPath ), "saves/%s", filename );
	
	if ( steam_remote_storage->FileWrite( cloudPath, data, dataSize ) ) {
		Com_Printf( "Saved to Steam Cloud: %s\n", cloudPath );
		return qtrue;
	}
#else
	(void)filename;
	(void)data;
	(void)dataSize;
#endif
	
	return qfalse;
}

/*
================
CL_SteamDeck_CloudSave_Read
Read save file from Steam Cloud
================
*/
qboolean CL_SteamDeck_CloudSave_Read( const char *filename, void *data, int maxSize, int *outSize )
{
#ifdef USE_STEAMWORKS
	if ( !steam_remote_storage || !data || maxSize <= 0 ) {
		return qfalse;
	}
	char cloudPath[MAX_QPATH];
	Com_sprintf( cloudPath, sizeof( cloudPath ), "saves/%s", filename );
	
	int32 fileSize = steam_remote_storage->GetFileSize( cloudPath );
	if ( fileSize <= 0 || fileSize > maxSize ) {
		return qfalse;
	}
	
	if ( steam_remote_storage->FileRead( cloudPath, data, maxSize ) == fileSize ) {
		if ( outSize ) {
			*outSize = fileSize;
		}
		return qtrue;
	}
#else
	(void)filename;
	(void)data;
	(void)maxSize;
	(void)outSize;
#endif
	
	return qfalse;
}

/*
================
CL_SteamDeck_CloudSave_FileExists
Check if file exists in Steam Cloud
================
*/
qboolean CL_SteamDeck_CloudSave_FileExists( const char *filename )
{
#ifdef USE_STEAMWORKS
	if ( !steam_remote_storage ) {
		return qfalse;
	}
	char cloudPath[MAX_QPATH];
	Com_sprintf( cloudPath, sizeof( cloudPath ), "saves/%s", filename );
	return steam_remote_storage->FileExists( cloudPath );
#else
	(void)filename;
	return qfalse;
#endif
}

/*
================
CL_SteamDeck_CloudSave_Shutdown
Shutdown Steam Cloud save system
================
*/
void CL_SteamDeck_CloudSave_Shutdown( void )
{
#ifdef USE_STEAMWORKS
	steam_remote_storage = NULL;
#endif
}

/*
================
CL_SteamDeck_SteamInput_Init
Initialize Steam Input API
================
*/
qboolean CL_SteamDeck_SteamInput_Init( void )
{
#ifdef USE_STEAMWORKS
	if ( SteamInput() ) {
		steam_input = SteamInput();
		if ( SteamInput()->Init( qtrue ) ) {
			Com_Printf( "Steam Input initialized\n" );
			return qtrue;
		}
	}
#endif
	
	return qfalse;
}

/*
================
CL_SteamDeck_SteamInput_Shutdown
Shutdown Steam Input API
================
*/
void CL_SteamDeck_SteamInput_Shutdown( void )
{
#ifdef USE_STEAMWORKS
	if ( steam_input && SteamInput() ) {
		SteamInput()->Shutdown();
	}
	steam_input = NULL;
#endif
}

/*
================
CL_SteamDeck_SteamInput_RunFrame
Run Steam Input frame (call every frame)
================
*/
void CL_SteamDeck_SteamInput_RunFrame( void )
{
#ifdef USE_STEAMWORKS
	if ( !steam_input || !SteamInput() ) {
		return;
	}
	
	// Run Steam Input callbacks
	SteamInput()->RunFrame();
	
	// Check for text input results
	if ( steamdeck_textinput_active ) {
		uint32 submittedTextLength = 0;
		if ( SteamInput()->GetGamepadTextInput( NULL, NULL, 0, &submittedTextLength ) ) {
			if ( submittedTextLength > 0 && submittedTextLength < sizeof( steamdeck_textinput_buffer ) ) {
				SteamInput()->GetGamepadTextInput( steamdeck_textinput_buffer, sizeof( steamdeck_textinput_buffer ), NULL, &submittedTextLength );
				steamdeck_textinput_active = qfalse;
			}
		}
	}
#endif
}

/*
================
CL_SteamDeck_Init
Initialize Steam Deck features
================
*/
void CL_SteamDeck_Init( void )
{
	if ( steamdeck_initialized ) {
		return;
	}
	
	// Register CVars
	cl_steamdeck_enable = Cvar_Get( "cl_steamdeck_enable", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_steamdeck_enable, "Enable Steam Deck features (auto-detected)" );
	
	cl_steamdeck_textinput = Cvar_Get( "cl_steamdeck_textinput", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_steamdeck_textinput, "Enable Steam Input on-screen keyboard" );
	
	cl_steamdeck_cloudsaves = Cvar_Get( "cl_steamdeck_cloudsaves", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_steamdeck_cloudsaves, "Enable Steam Cloud save sync" );
	
	cl_steamdeck_simultaneous_input = Cvar_Get( "cl_steamdeck_simultaneous_input", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_steamdeck_simultaneous_input, "Enable simultaneous mouse and controller input" );
	
	if ( !cl_steamdeck_enable->integer ) {
		return;
	}
	
	// Detect Steam Deck
	if ( CL_SteamDeck_Detect() ) {
		Com_Printf( "Steam Deck features enabled\n" );
		
#ifdef USE_STEAMWORKS
		// Initialize Steam API
		if ( SteamAPI_Init() ) {
			steam_utils = SteamUtils();
			steam_input = SteamInput();
			steam_remote_storage = SteamRemoteStorage();
			
			// Initialize Steam Input
			CL_SteamDeck_SteamInput_Init();
			
			// Initialize Cloud Saves
			CL_SteamDeck_CloudSave_Init();
		} else {
			Com_Printf( "WARNING: Steam API initialization failed\n" );
		}
#endif
	}
	
	steamdeck_initialized = qtrue;
}

/*
================
CL_SteamDeck_Shutdown
Shutdown Steam Deck features
================
*/
void CL_SteamDeck_Shutdown( void )
{
	if ( !steamdeck_initialized ) {
		return;
	}
	
	CL_SteamDeck_SteamInput_Shutdown();
	CL_SteamDeck_CloudSave_Shutdown();
	
#ifdef USE_STEAMWORKS
	SteamAPI_Shutdown();
#endif
	
	steamdeck_initialized = qfalse;
}

/*
================
CL_SteamDeck_RunFrame
Run Steam Deck frame updates (call every frame)
================
*/
void CL_SteamDeck_RunFrame( void )
{
	if ( !steamdeck_initialized ) {
		return;
	}

#ifdef USE_STEAMWORKS
	// Run Steam API callbacks
	SteamAPI_RunCallbacks();

	// Run Steam Input frame
	CL_SteamDeck_SteamInput_RunFrame();
#endif
}

// ============================================================================
// Controller Glyph System
// ============================================================================

/*
================
CL_SteamDeck_Glyphs_Init

Initialize the Steam Deck controller glyph system
================
*/
qboolean CL_SteamDeck_Glyphs_Init( void )
{
	int i;

	if ( steamdeck_glyphs_initialized ) {
		return qtrue;
	}

	if ( !steamdeck_is_steamdeck ) {
		Com_Printf( "Steam Deck glyphs: Not initializing (not on Steam Deck)\n" );
		return qfalse;
	}

	Com_Printf( "Initializing Steam Deck controller glyphs...\n" );

	// Initialize glyph state
	for ( i = 0; i < STEAMDECK_GLYPH_MAX; i++ ) {
		steamdeck_glyph_textures[i] = 0;
		steamdeck_glyph_available[i] = qfalse;
	}

	// Load glyph textures
	for ( i = 0; i < STEAMDECK_GLYPH_MAX; i++ ) {
		if ( re.RegisterShader( steamdeck_glyph_files[i] ) ) {
			steamdeck_glyph_textures[i] = re.RegisterShader( steamdeck_glyph_files[i] );
			steamdeck_glyph_available[i] = qtrue;
			Com_DPrintf( "Loaded Steam Deck glyph: %s\n", steamdeck_glyph_files[i] );
		} else {
			Com_DPrintf( "Steam Deck glyph not found: %s\n", steamdeck_glyph_files[i] );
		}
	}

	steamdeck_glyphs_initialized = qtrue;
	Com_Printf( "Steam Deck controller glyphs initialized\n" );

	return qtrue;
}

/*
================
CL_SteamDeck_Glyphs_Shutdown

Shutdown the Steam Deck controller glyph system
================
*/
void CL_SteamDeck_Glyphs_Shutdown( void )
{
	if ( !steamdeck_glyphs_initialized ) {
		return;
	}

	Com_Printf( "Shutting down Steam Deck controller glyphs...\n" );

	// Glyph textures are managed by the renderer, no explicit cleanup needed

	steamdeck_glyphs_initialized = qfalse;
}

/*
================
CL_SteamDeck_GetGlyphTexture

Get the texture handle for a Steam Deck controller glyph
================
*/
qhandle_t CL_SteamDeck_GetGlyphTexture( steamdeck_glyph_t glyph )
{
	if ( !steamdeck_glyphs_initialized || glyph < 0 || glyph >= STEAMDECK_GLYPH_MAX ) {
		return 0;
	}

	return steamdeck_glyph_textures[glyph];
}

/*
================
CL_SteamDeck_RenderGlyph

Render a Steam Deck controller glyph at the specified position
================
*/
void CL_SteamDeck_RenderGlyph( float x, float y, float scale, steamdeck_glyph_t glyph )
{
	if ( !steamdeck_glyphs_initialized || !steamdeck_glyph_available[glyph] ) {
		return;
	}

	qhandle_t shader = steamdeck_glyph_textures[glyph];
	if ( !shader ) {
		return;
	}

	// Calculate glyph dimensions (assuming square glyphs)
	float size = 32.0f * scale;

	// Set up render state for 2D rendering
	re.SetColor( (float*)&colorWhite );

	// Draw the glyph texture
	// This would typically use the UI rendering system
	// For now, we'll use a simple quad rendering approach
	// (In a real implementation, this would integrate with the UI system)

	re.DrawStretchPic( x, y, size, size, 0, 0, 1, 1, shader );
}

/*
================
CL_SteamDeck_GetGlyphName

Get the display name for a Steam Deck controller glyph
================
*/
const char *CL_SteamDeck_GetGlyphName( steamdeck_glyph_t glyph )
{
	if ( glyph < 0 || glyph >= STEAMDECK_GLYPH_MAX ) {
		return "Unknown";
	}

	return steamdeck_glyph_names[glyph];
}

/*
================
CL_SteamDeck_IsGlyphAvailable

Check if a Steam Deck controller glyph is available
================
*/
qboolean CL_SteamDeck_IsGlyphAvailable( steamdeck_glyph_t glyph )
{
	if ( !steamdeck_glyphs_initialized || glyph < 0 || glyph >= STEAMDECK_GLYPH_MAX ) {
		return qfalse;
	}

	return steamdeck_glyph_available[glyph];
}
