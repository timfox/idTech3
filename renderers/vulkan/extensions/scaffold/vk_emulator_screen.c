/*
 * In-world OS sandbox display: dynamic RGBA texture from QEMU guest frames.
 */
#include "../../tr_local.h"
#include "../../tr_common.h"
#include "vk_emulator_screen.h"

#ifdef USE_IDTECH3_EMULATOR

static image_t *s_emulatorImage = NULL;
static cvar_t *r_emulatorScreen;
static qboolean s_logged;

void R_Emulator_Init( void )
{
	r_emulatorScreen = ri.Cvar_Get( "r_emulatorScreen", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_emulatorScreen, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_emulatorScreen,
		"Upload guest display frames to *emulator_screen texture (pair with cl_emulator 1)." );
}

void R_Emulator_Shutdown( void )
{
	s_emulatorImage = NULL;
	s_logged = qfalse;
}

void RE_EmulatorUploadFrame( const byte *rgba, int width, int height )
{
	if ( !rgba || width < 1 || height < 1 ) {
		return;
	}
	if ( !r_emulatorScreen || !r_emulatorScreen->integer ) {
		return;
	}

	if ( !s_emulatorImage || s_emulatorImage->width != width || s_emulatorImage->height != height ) {
		s_emulatorImage = R_CreateImage( "*emulator_screen", NULL, NULL, width, height,
			IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE, 0, 0 );
		if ( s_emulatorImage && !s_logged ) {
			ri.Printf( PRINT_ALL, "[emulator] screen texture %dx%d (*emulator_screen)\n", width, height );
			s_logged = qtrue;
		}
	}

	if ( s_emulatorImage ) {
		R_UploadSubImage( (byte *)rgba, 0, 0, width, height, s_emulatorImage );
	}
}

#else /* !USE_IDTECH3_EMULATOR */

void R_Emulator_Init( void ) {}
void R_Emulator_Shutdown( void ) {}
void RE_EmulatorUploadFrame( const byte *rgba, int width, int height )
{
	(void)rgba;
	(void)width;
	(void)height;
}

#endif
