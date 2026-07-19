/*
 * SDL3 webcam preview: dynamic RGBA texture uploaded as *webcam.
 */
#include "../../tr_local.h"
#include "../../tr_common.h"
#include "vk_webcam_screen.h"

static image_t *s_webcamImage = NULL;
static qboolean s_logged = qfalse;

void R_Webcam_Init( void )
{
	s_webcamImage = NULL;
	s_logged = qfalse;
}

void R_Webcam_Shutdown( void )
{
	s_webcamImage = NULL;
	s_logged = qfalse;
}

void RE_WebcamUploadFrame( const byte *rgba, int width, int height )
{
	if ( !rgba || width < 1 || height < 1 ) {
		return;
	}

	if ( !s_webcamImage || s_webcamImage->width != width || s_webcamImage->height != height ) {
		s_webcamImage = R_CreateImage( "*webcam", NULL, NULL, width, height,
			IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE, 0, 0 );
		if ( s_webcamImage && !s_logged ) {
			ri.Printf( PRINT_ALL, "[webcam] preview texture %dx%d (*webcam)\n", width, height );
			s_logged = qtrue;
		}
	}

	if ( s_webcamImage ) {
		R_UploadSubImage( (byte *)(uintptr_t)(const void *)rgba, 0, 0, width, height, s_webcamImage );
	}
}
