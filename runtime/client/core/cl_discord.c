#include "client.h"
#include "cl_discord.h"
#include "cl_discord_proto.h"
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define DISCORD_GETPID() ( (int)_getpid() )
#else
#include <unistd.h>
#define DISCORD_GETPID() ( (int)getpid() )
#endif

#define DISCORD_TICK_MS			1000	// evaluate presence at most once per second
#define DISCORD_RETRY_MS		15000	// wait between connect attempts when Discord absent
#define DISCORD_HANDSHAKE_MS	5000	// deadline for READY after handshake sent

typedef enum {
	DC_DISCONNECTED,
	DC_HANDSHAKING,
	DC_CONNECTED
} discordState_t;

static cvar_t			*cl_discordAppId;
static cvar_t			*cl_discordButton1Label;
static cvar_t			*cl_discordButton1Url;
static cvar_t			*cl_discordButton2Label;
static cvar_t			*cl_discordButton2Url;
static cvar_t			*cl_discordLargeImage;
static cvar_t			*cl_discordLargeText;
static discordConn_t	*dc_conn;
static discordState_t	dc_state;
static int				dc_nextAttempt;			// Sys_Milliseconds gate for reconnect
static int				dc_nextTick;			// Sys_Milliseconds gate for tick body
static int				dc_handshakeDeadline;	// Sys_Milliseconds deadline for READY
static int				dc_nonce;
static discordActivity_t dc_last;				// last activity successfully sent
static qboolean			dc_haveLast;

#if defined( __ANDROID__ )
/* Discord desktop IPC is unavailable on Android; keep symbols linked. */
struct discordConn_s { int unused; };

discordConn_t *Sys_DiscordConnect( void ) {
	return NULL;
}

int Sys_DiscordRead( discordConn_t *c, void *buf, int len ) {
	(void)c; (void)buf; (void)len;
	return -1;
}

int Sys_DiscordWrite( discordConn_t *c, const void *buf, int len ) {
	(void)c; (void)buf; (void)len;
	return -1;
}

void Sys_DiscordClose( discordConn_t *c ) {
	(void)c;
}
#endif

void CL_Discord_Init( void ) {
	cl_discordAppId = Cvar_Get( "cl_discordAppId", "", CVAR_ARCHIVE );
	cl_discordButton1Label = Cvar_Get( "cl_discordButton1Label", "", CVAR_ARCHIVE );
	cl_discordButton1Url = Cvar_Get( "cl_discordButton1Url", "", CVAR_ARCHIVE );
	cl_discordButton2Label = Cvar_Get( "cl_discordButton2Label", "", CVAR_ARCHIVE );
	cl_discordButton2Url = Cvar_Get( "cl_discordButton2Url", "", CVAR_ARCHIVE );
	cl_discordLargeImage = Cvar_Get( "cl_discordLargeImage", "logo", CVAR_ARCHIVE );
	cl_discordLargeText = Cvar_Get( "cl_discordLargeText", "Surf", CVAR_ARCHIVE );
	dc_state = DC_DISCONNECTED;
	dc_conn = NULL;
	dc_nextAttempt = 0;
	dc_nextTick = 0;
	dc_handshakeDeadline = 0;
	dc_nonce = 0;
	dc_haveLast = qfalse;
}

static void CL_Discord_Reset( void ) {
	if ( dc_conn ) {
		Sys_DiscordClose( dc_conn );
		dc_conn = NULL;
	}
	dc_state = DC_DISCONNECTED;
	dc_haveLast = qfalse;
	dc_nextAttempt = Sys_Milliseconds() + DISCORD_RETRY_MS;
}

static qboolean CL_Discord_Send( const char *frame, int len ) {
	if ( Sys_DiscordWrite( dc_conn, frame, len ) != len ) {
		CL_Discord_Reset();
		return qfalse;
	}
	return qtrue;
}

static void CL_Discord_FillOpts( discordPresenceOpts_t *opts ) {
	memset( opts, 0, sizeof( *opts ) );
	Q_strncpyz( opts->largeImage, cl_discordLargeImage->string, sizeof( opts->largeImage ) );
	Q_strncpyz( opts->largeText, cl_discordLargeText->string, sizeof( opts->largeText ) );
	Q_strncpyz( opts->button1Label, cl_discordButton1Label->string, sizeof( opts->button1Label ) );
	Q_strncpyz( opts->button1Url, cl_discordButton1Url->string, sizeof( opts->button1Url ) );
	Q_strncpyz( opts->button2Label, cl_discordButton2Label->string, sizeof( opts->button2Label ) );
	Q_strncpyz( opts->button2Url, cl_discordButton2Url->string, sizeof( opts->button2Url ) );
}

/* Map current engine state to a phase + gametype, build the desired activity. */
static void CL_Discord_CurrentActivity( discordActivity_t *act ) {
	discordPhase_t phase;
	const char *serverInfo = "";
	const char *mapMessage = "";
	int gametype = 0;

	switch ( cls.state ) {
		case CA_DISCONNECTED:
		case CA_UNINITIALIZED:
			phase = DISCORD_MENU; break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			phase = DISCORD_CONNECTING; break;
		case CA_LOADING:
		case CA_PRIMED:
			phase = DISCORD_LOADING; break;
		case CA_CINEMATIC:
			phase = DISCORD_CINEMATIC; break;
		case CA_ACTIVE:
			if ( clc.demoplaying ) {
				phase = DISCORD_WATCHING_DEMO;
			} else {
				phase = DISCORD_PLAYING;
			}
			break;
		default:
			phase = DISCORD_MENU;
			break;
	}

	if ( phase == DISCORD_PLAYING || phase == DISCORD_WATCHING_DEMO ) {
		serverInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
		mapMessage = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_MESSAGE ];
		gametype = atoi( Info_ValueForKey( serverInfo, "g_gametype" ) );
	}

	Discord_MapActivity( act, phase, serverInfo, mapMessage, gametype,
		(int)time( NULL ), dc_haveLast ? &dc_last : NULL );
}

static void CL_Discord_Update( void ) {
	char frame[ 1536 ];
	discordActivity_t want;
	discordPresenceOpts_t opts;
	int len;

	CL_Discord_CurrentActivity( &want );
	CL_Discord_FillOpts( &opts );

	if ( dc_haveLast && Discord_ActivityEqual( &want, &dc_last ) &&
	     want.startTimestamp == dc_last.startTimestamp &&
	     !cl_discordLargeImage->modified &&
	     !cl_discordLargeText->modified &&
	     !cl_discordButton1Label->modified &&
	     !cl_discordButton1Url->modified &&
	     !cl_discordButton2Label->modified &&
	     !cl_discordButton2Url->modified ) {
		return;	// nothing changed
	}

	len = Discord_BuildSetActivity( frame, sizeof( frame ), &want, DISCORD_GETPID(), ++dc_nonce, &opts );
	if ( len > 0 && CL_Discord_Send( frame, len ) ) {
		dc_last = want;
		dc_haveLast = qtrue;
		cl_discordLargeImage->modified = qfalse;
		cl_discordLargeText->modified = qfalse;
		cl_discordButton1Label->modified = qfalse;
		cl_discordButton1Url->modified = qfalse;
		cl_discordButton2Label->modified = qfalse;
		cl_discordButton2Url->modified = qfalse;
		Com_DPrintf( "Discord: %s | %s\n", want.details, want.state );
	}
}

/* Drain inbound bytes; detect READY during handshake. Returns qfalse on closed. */
static qboolean CL_Discord_Pump( qboolean *gotReady ) {
	char buf[ 2048 ];
	int n = Sys_DiscordRead( dc_conn, buf, sizeof( buf ) );
	if ( n < 0 ) {
		CL_Discord_Reset();
		return qfalse;
	}
	if ( n > 0 && gotReady && Discord_BufContains( buf, n, "\"evt\":\"READY\"" ) ) {
		*gotReady = qtrue;
	}
	return qtrue;
}

static qboolean CL_Discord_Enabled( void ) {
	return ( cl_discordAppId && cl_discordAppId->string[0] ) ? qtrue : qfalse;
}

void CL_Discord_Frame( void ) {
	int now;

	if ( !CL_Discord_Enabled() ) {
		if ( dc_conn ) {
			CL_Discord_Shutdown();	// clears activity and closes
		}
		return;
	}

	/* App ID changed while connected — re-handshake with the new client id. */
	if ( cl_discordAppId->modified && dc_state != DC_DISCONNECTED ) {
		CL_Discord_Shutdown();
		cl_discordAppId->modified = qfalse;
		dc_nextAttempt = 0;
	} else if ( cl_discordAppId->modified ) {
		cl_discordAppId->modified = qfalse;
		dc_nextAttempt = 0;
	}

	now = Sys_Milliseconds();
	if ( now < dc_nextTick ) {
		return;
	}
	dc_nextTick = now + DISCORD_TICK_MS;

	switch ( dc_state ) {
		case DC_DISCONNECTED: {
			char frame[ 512 ];
			int len;
			if ( now < dc_nextAttempt ) {
				return;
			}
			dc_conn = Sys_DiscordConnect();
			if ( !dc_conn ) {
				dc_nextAttempt = now + DISCORD_RETRY_MS;
				return;
			}
			len = Discord_BuildHandshake( frame, sizeof( frame ), cl_discordAppId->string );
			if ( len <= 0 || !CL_Discord_Send( frame, len ) ) {
				return;
			}
			dc_state = DC_HANDSHAKING;
			dc_handshakeDeadline = now + DISCORD_HANDSHAKE_MS;
			break;
		}
		case DC_HANDSHAKING: {
			qboolean ready = qfalse;
			if ( !CL_Discord_Pump( &ready ) ) {
				return;
			}
			if ( ready ) {
				dc_state = DC_CONNECTED;
				CL_Discord_Update();
			} else if ( now >= dc_handshakeDeadline ) {
				CL_Discord_Reset();
				return;
			}
			break;
		}
		case DC_CONNECTED:
			if ( !CL_Discord_Pump( NULL ) ) {
				return;
			}
			CL_Discord_Update();
			break;
	}
}

void CL_Discord_Shutdown( void ) {
	if ( dc_conn ) {
		if ( dc_state == DC_CONNECTED ) {
			char frame[ 512 ];
			int len = Discord_BuildClearActivity( frame, sizeof( frame ),
				DISCORD_GETPID(), ++dc_nonce );
			if ( len > 0 ) {
				Sys_DiscordWrite( dc_conn, frame, len );
			}
		}
		Sys_DiscordClose( dc_conn );
		dc_conn = NULL;
	}
	dc_state = DC_DISCONNECTED;
	dc_haveLast = qfalse;
}
