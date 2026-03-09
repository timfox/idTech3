/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Menu background video playback.

Plays a looping video file as the main menu background. When the menu
is active and no map is loaded, the video fills the screen behind the
UI elements. When the player enters a game, the video stops.

Usage:
  r_menuVideo "video/menu_bg.roq"    — set video file
  r_menuVideoLoop 1                  — loop playback (default)

Supported formats: ROQ (native), plus any format supported by the
modern cinematic system (AV1, WebM, Theora, FFmpeg).
===========================================================================
*/

#include "client.h"
#include "cl_menuvideo.h"

/* CIN functions declared in client.h */

static cvar_t *r_menuVideo;
static cvar_t *r_menuVideoLoop;
static int menuVideoHandle = -1;
static qboolean menuVideoInited = qfalse;
static qboolean menuVideoLooping = qfalse;
static char currentVideoFile[MAX_QPATH];

void MenuVideo_Init( void ) {
	r_menuVideo = Cvar_Get( "r_menuVideo", "", CVAR_ARCHIVE );
	Cvar_SetDescription( r_menuVideo, "Video file to play as menu background (e.g. video/menu_bg.roq). Empty = no video." );

	r_menuVideoLoop = Cvar_Get( "r_menuVideoLoop", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_menuVideoLoop, "Loop the menu background video (0 = play once, 1 = loop)." );

	menuVideoHandle = -1;
	menuVideoLooping = qfalse;
	currentVideoFile[0] = '\0';
	menuVideoInited = qtrue;
	r_menuVideo->modified = qfalse;
	r_menuVideoLoop->modified = qfalse;

	Com_Printf( "Menu video: %s\n", r_menuVideo->string[0] ? r_menuVideo->string : "disabled (r_menuVideo empty)" );
}

void MenuVideo_Shutdown( void ) {
	if ( menuVideoHandle >= 0 ) {
		CIN_StopCinematic( menuVideoHandle );
		menuVideoHandle = -1;
	}
	menuVideoLooping = qfalse;
	currentVideoFile[0] = '\0';
	menuVideoInited = qfalse;
}

static void MenuVideo_Start( void ) {
	int bits = CIN_hold;
	const qboolean shouldLoop = ( r_menuVideoLoop && r_menuVideoLoop->integer ) ? qtrue : qfalse;

	if ( shouldLoop ) {
		bits |= CIN_loop;
	}

	menuVideoHandle = CIN_PlayCinematic( r_menuVideo->string,
		0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, bits );

	if ( menuVideoHandle >= 0 ) {
		Q_strncpyz( currentVideoFile, r_menuVideo->string, sizeof( currentVideoFile ) );
		menuVideoLooping = shouldLoop;
		Com_Printf( "Menu video: playing '%s' (handle %d, %s)\n",
			r_menuVideo->string, menuVideoHandle,
			( bits & CIN_loop ) ? "looping" : "once" );
	} else {
		Com_Printf( S_COLOR_YELLOW "Menu video: failed to play '%s'\n", r_menuVideo->string );
	}
}

static void MenuVideo_Stop( void ) {
	if ( menuVideoHandle >= 0 ) {
		CIN_StopCinematic( menuVideoHandle );
		menuVideoHandle = -1;
		currentVideoFile[0] = '\0';
		menuVideoLooping = qfalse;
	}
}

qboolean MenuVideo_IsPlaying( void ) {
	return menuVideoHandle >= 0;
}

void MenuVideo_Frame( void ) {
	qboolean requestedLoop;

	if ( !menuVideoInited ) return;
	requestedLoop = ( r_menuVideoLoop && r_menuVideoLoop->integer ) ? qtrue : qfalse;

	/* Stop video if we're in a game */
	if ( cls.state >= CA_LOADING ) {
		if ( menuVideoHandle >= 0 ) {
			MenuVideo_Stop();
		}
		return;
	}

	/* Apply loop mode changes immediately while menu video is active. */
	if ( r_menuVideoLoop && r_menuVideoLoop->modified ) {
		r_menuVideoLoop->modified = qfalse;
		if ( menuVideoHandle >= 0 && menuVideoLooping != requestedLoop ) {
			MenuVideo_Stop();
			MenuVideo_Start();
		}
	}

	/* Start/change video if cvar changed or not playing */
	if ( r_menuVideo->string[0] ) {
		if ( r_menuVideo->modified || menuVideoHandle < 0 || Q_stricmp( currentVideoFile, r_menuVideo->string ) != 0 ) {
			r_menuVideo->modified = qfalse;
			MenuVideo_Stop();
			MenuVideo_Start();
		}
	} else {
		r_menuVideo->modified = qfalse;
		if ( menuVideoHandle >= 0 ) {
			MenuVideo_Stop();
		}
		return;
	}

	/* Advance video frame */
	if ( menuVideoHandle >= 0 ) {
		e_status status = CIN_RunCinematic( menuVideoHandle );

		if ( status == FMV_EOF ) {
			if ( r_menuVideoLoop && r_menuVideoLoop->integer ) {
				/* Restart */
				MenuVideo_Stop();
				MenuVideo_Start();
			} else {
				MenuVideo_Stop();
			}
		}
	}
}

void MenuVideo_Draw( void ) {
	if ( menuVideoHandle < 0 ) return;
	if ( cls.state >= CA_LOADING ) return;

	CIN_DrawCinematic( menuVideoHandle );
}
