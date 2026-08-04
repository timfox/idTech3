/*
===========================================================================
Optional live-streaming controls for idTech3-tv / Owncast-compatible RTMP.

Players can run an external broadcast service separately and push gameplay to
its RTMP ingest endpoint. The preferred backend captures the engine framebuffer
and mixer through the existing AVI pipe; external desktop capture remains a
compatibility fallback.
===========================================================================
*/

#include "client.h"
#include "cl_pipeline.h"
#include "cl_streaming.h"

#include <stdlib.h>

static cvar_t *cl_stream_enable;
static cvar_t *cl_stream_url;
static cvar_t *cl_stream_key;
static cvar_t *cl_stream_title;
static cvar_t *cl_stream_backend;
static cvar_t *cl_stream_cmd;
static cvar_t *cl_stream_ffmpeg;
static cvar_t *cl_stream_width;
static cvar_t *cl_stream_height;
static cvar_t *cl_stream_fps;
static cvar_t *cl_stream_bitrate;
static cvar_t *cl_stream_audio_bitrate;
static cvar_t *cl_stream_queueMegs;
static cvar_t *cl_stream_autoStart;
static cvar_t *cl_stream_platform;

typedef enum {
	STREAM_BACKEND_NONE,
	STREAM_BACKEND_ENGINE,
	STREAM_BACKEND_EXTERNAL
} streamBackend_t;

static qboolean stream_active = qfalse;
static streamBackend_t stream_backend_active = STREAM_BACKEND_NONE;
static char stream_last_command[8192];
static qboolean stream_saved_streamer_mode;
static qboolean stream_streamer_mode_saved;

static void CL_StreamingStart_f( void );

/* KICK publishes through a custom RTMPS service and supplies the key separately. */
#define CL_STREAM_KICK_URL "rtmps://fa723fc1b171.global-contribute.live-video.net:443/app"
#define CL_STREAM_IDTECH3TV_URL "rtmp://127.0.0.1:1935/live"

static const char *CL_StreamingDefaultExternalTemplate( void )
{
#ifdef _WIN32
	return "start \"idTech3 TV\" /B %P -y -f gdigrab -framerate %F -video_size %Wx%H -i desktop -f dshow -i audio=\"default\" -c:v libx264 -preset veryfast -tune zerolatency -b:v %V -minrate %V -maxrate %V -bufsize 2M -pix_fmt yuv420p -g 120 -keyint_min 120 -sc_threshold 0 -c:a aac -b:a %Q -f flv \"%U/%K\"";
#elif defined(__APPLE__)
	return "nohup %P -y -f avfoundation -framerate %F -video_size %Wx%H -i \"1:0\" -c:v libx264 -preset veryfast -tune zerolatency -b:v %V -minrate %V -maxrate %V -bufsize 2M -pix_fmt yuv420p -g 120 -keyint_min 120 -sc_threshold 0 -c:a aac -b:a %Q -f flv \"%U/%K\" >/tmp/idtech3-tv-ffmpeg.log 2>&1 &";
#else
	return "nohup %P -y -f x11grab -framerate %F -video_size %Wx%H -i ${DISPLAY:-:0.0} -f pulse -i default -c:v libx264 -preset veryfast -tune zerolatency -b:v %V -minrate %V -maxrate %V -bufsize 2M -pix_fmt yuv420p -g 120 -keyint_min 120 -sc_threshold 0 -c:a aac -b:a %Q -f flv \"%U/%K\" >/tmp/idtech3-tv-ffmpeg.log 2>&1 &";
#endif
}

static const char *CL_StreamingDefaultEngineTemplate( void )
{
	return "%P -f avi -i - -threads 0 -y -c:v libx264 -preset veryfast -tune zerolatency -b:v %V -minrate %V -maxrate %V -bufsize 2M -pix_fmt yuv420p -g 120 -keyint_min 120 -sc_threshold 0 -c:a aac -b:a %Q -f flv \"%U/%K\"";
}

static void CL_StreamingBuildExpand( cl_pipeline_expand_t *ex )
{
	Com_Memset( ex, 0, sizeof( *ex ) );
	ex->py = cl_stream_ffmpeg ? cl_stream_ffmpeg->string : "ffmpeg";
	ex->url = cl_stream_url ? cl_stream_url->string : "";
	ex->key = cl_stream_key ? cl_stream_key->string : "";
	ex->title = cl_stream_title ? cl_stream_title->string : "";
	ex->width = cl_stream_width ? cl_stream_width->string : "1280";
	ex->height = cl_stream_height ? cl_stream_height->string : "720";
	ex->fps = cl_stream_fps ? cl_stream_fps->string : "30";
	ex->bitrate = cl_stream_bitrate ? cl_stream_bitrate->string : "3500k";
	ex->audio_bitrate = cl_stream_audio_bitrate ? cl_stream_audio_bitrate->string : "128k";
}

static qboolean CL_StreamingBackendIsExternal( void )
{
	return cl_stream_backend && !Q_stricmp( cl_stream_backend->string, "external" );
}

static qboolean CL_StreamingValidateConfig( void )
{
	if ( !cl_stream_enable || !cl_stream_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "stream_start: set cl_stream_enable 1 first\n" );
		return qfalse;
	}
	if ( !cl_stream_url || !cl_stream_url->string[0] ) {
		Com_Printf( S_COLOR_YELLOW "stream_start: set cl_stream_url, e.g. rtmp://127.0.0.1:1935/live\n" );
		return qfalse;
	}
	if ( !cl_stream_key || !cl_stream_key->string[0] ) {
		Com_Printf( S_COLOR_YELLOW "stream_start: set cl_stream_key from your idTech3-tv stream key\n" );
		return qfalse;
	}
	if ( !cl_stream_ffmpeg || !cl_stream_ffmpeg->string[0] ) {
		Com_Printf( S_COLOR_YELLOW "stream_start: set cl_stream_ffmpeg to ffmpeg executable path\n" );
		return qfalse;
	}
	return qtrue;
}

static void CL_StreamingStatus_f( void )
{
	int queuedBytes = 0;
	int peakQueuedBytes = 0;
	int droppedChunks = 0;
	int droppedBytes = 0;
	qboolean failed = qfalse;

	Com_Printf( "idTech3 TV streaming:\n" );
	Com_Printf( "  enabled: %d active: %d\n", cl_stream_enable ? cl_stream_enable->integer : 0, stream_active );
	Com_Printf( "  platform: %s\n", cl_stream_platform && cl_stream_platform->string[0] ? cl_stream_platform->string : "custom" );
	Com_Printf( "  backend: %s%s\n",
		cl_stream_backend ? cl_stream_backend->string : "engine",
		stream_backend_active == STREAM_BACKEND_ENGINE ? " (engine capture active)" :
		stream_backend_active == STREAM_BACKEND_EXTERNAL ? " (external capture active)" : "" );
	Com_Printf( "  url:     %s\n", ( cl_stream_url && cl_stream_url->string[0] ) ? cl_stream_url->string : "(unset)" );
	Com_Printf( "  key:     %s\n", ( cl_stream_key && cl_stream_key->string[0] ) ? "(protected)" : "(unset)" );
	Com_Printf( "  video:   %sx%s @ %s fps, %s video / %s audio\n",
		cl_stream_width ? cl_stream_width->string : "?",
		cl_stream_height ? cl_stream_height->string : "?",
		cl_stream_fps ? cl_stream_fps->string : "?",
		cl_stream_bitrate ? cl_stream_bitrate->string : "?",
		cl_stream_audio_bitrate ? cl_stream_audio_bitrate->string : "?" );
	if ( stream_backend_active == STREAM_BACKEND_ENGINE ) {
		CL_GetAVIPipeStats( &queuedBytes, &peakQueuedBytes, &droppedChunks, &droppedBytes, &failed );
		Com_Printf( "  queue:   %d KiB queued, %d KiB peak, %d chunks dropped (%d KiB), failed: %d\n",
			queuedBytes / 1024,
			peakQueuedBytes / 1024,
			droppedChunks,
			droppedBytes / 1024,
			failed );
	}
	if ( stream_last_command[0] ) {
		/* The expanded command contains the protected stream key. Never echo it. */
		Com_Printf( "  last command: configured (redacted)\n" );
	}
}

static void CL_StreamingKickSetup_f( void )
{
	Cvar_Set( "cl_stream_platform", "kick" );
	Cvar_Set( "cl_stream_enable", "1" );
	Cvar_Set( "cl_stream_backend", "engine" );
	Cvar_Set( "cl_stream_url", CL_STREAM_KICK_URL );
	Cvar_Set( "cl_stream_width", "1920" );
	Cvar_Set( "cl_stream_height", "1080" );
	Cvar_Set( "cl_stream_fps", "60" );
	Cvar_Set( "cl_stream_bitrate", "6000k" );
	Cvar_Set( "cl_stream_audio_bitrate", "128k" );
	if ( !cl_stream_title || !cl_stream_title->string[0] || !Q_stricmp( cl_stream_title->string, "idTech3 Live" ) ) {
		Cvar_Set( "cl_stream_title", "Surf Live" );
	}
	Com_Printf( S_COLOR_GREEN "stream_kick_setup: configured KICK RTMPS capture.\n" );
	Com_Printf( "stream_kick_setup: set cl_stream_key <your KICK stream key>, then run stream_start.\n" );
	Com_Printf( "stream_kick_setup: key is protected and will not be shown by stream_status.\n" );
}

static void CL_StreamingKickStart_f( void )
{
	CL_StreamingKickSetup_f();
	CL_StreamingStart_f();
}

static void CL_StreamingIdTech3TVSetup_f( void )
{
	Cvar_Set( "cl_stream_platform", "idtech3tv" );
	Cvar_Set( "cl_stream_enable", "1" );
	Cvar_Set( "cl_stream_backend", "engine" );
	Cvar_Set( "cl_stream_url", CL_STREAM_IDTECH3TV_URL );
	Cvar_Set( "cl_stream_width", "1280" );
	Cvar_Set( "cl_stream_height", "720" );
	Cvar_Set( "cl_stream_fps", "60" );
	Cvar_Set( "cl_stream_bitrate", "5000k" );
	Cvar_Set( "cl_stream_audio_bitrate", "128k" );
	if ( !cl_stream_title || !cl_stream_title->string[0] || !Q_stricmp( cl_stream_title->string, "idTech3 Live" ) ) {
		Cvar_Set( "cl_stream_title", "Surf Live" );
	}
	Com_Printf( S_COLOR_GREEN "stream_idtech3tv_setup: configured local idTech3TV/Owncast RTMP capture.\n" );
	Com_Printf( "stream_idtech3tv_setup: run the server, set cl_stream_key to its configured key, then run stream_start.\n" );
}

static void CL_StreamingIdTech3TVStart_f( void )
{
	CL_StreamingIdTech3TVSetup_f();
	CL_StreamingStart_f();
}

static void CL_StreamingStart_f( void )
{
	cl_pipeline_expand_t ex;
	const char *tmpl;
	int rc;

	if ( stream_active ) {
		Com_Printf( S_COLOR_YELLOW "stream_start: already active; use stream_stop when your broadcaster exits\n" );
		return;
	}
	if ( !CL_StreamingValidateConfig() ) {
		return;
	}

	tmpl = ( cl_stream_cmd && cl_stream_cmd->string[0] ) ? cl_stream_cmd->string :
		( CL_StreamingBackendIsExternal() ? CL_StreamingDefaultExternalTemplate() : CL_StreamingDefaultEngineTemplate() );
	CL_StreamingBuildExpand( &ex );
	if ( !CL_PipelineExpandTemplate( stream_last_command, sizeof( stream_last_command ), tmpl, &ex ) ) {
		stream_last_command[0] = '\0';
		Com_Printf( S_COLOR_RED "stream_start: command expansion failed\n" );
		return;
	}

	if ( CL_StreamingBackendIsExternal() ) {
		Com_Printf( "stream_start: launching external RTMP publisher for idTech3-tv / Owncast\n" );
		rc = system( stream_last_command );
		if ( rc != 0 ) {
			Com_Printf( S_COLOR_RED "stream_start: command returned %d\n", rc );
			return;
		}
		stream_backend_active = STREAM_BACKEND_EXTERNAL;
	} else {
		Com_Printf( "stream_start: opening engine framebuffer/audio pipe for idTech3-tv / Owncast\n" );
		if ( !CL_OpenAVIForPipeCommand( "idtech3-tv-live", stream_last_command,
				cl_stream_fps ? cl_stream_fps->integer : 30,
				( cl_stream_queueMegs ? cl_stream_queueMegs->integer : 64 ) * 1024 * 1024 ) ) {
			stream_last_command[0] = '\0';
			Com_Printf( S_COLOR_RED "stream_start: could not open engine streaming pipe\n" );
			return;
		}
		stream_backend_active = STREAM_BACKEND_ENGINE;
	}
	/* Protect player names and chat automatically while broadcasting. */
	stream_saved_streamer_mode = Cvar_VariableIntegerValue( "cg_surfStreamerMode" ) != 0 ? qtrue : qfalse;
	stream_streamer_mode_saved = qtrue;
	Cvar_Set( "cg_surfStreamerMode", "1" );
	stream_active = qtrue;
	Com_Printf( S_COLOR_GREEN "stream_start: broadcaster launched\n" );
}

static void CL_StreamingStop_f( void )
{
	qboolean wasExternal;

	if ( !stream_active ) {
		Com_Printf( "stream_stop: no active broadcaster tracked by engine\n" );
		return;
	}
	wasExternal = ( stream_backend_active == STREAM_BACKEND_EXTERNAL ) ? qtrue : qfalse;
	if ( stream_backend_active == STREAM_BACKEND_ENGINE ) {
		CL_CloseAVI( qfalse );
	}
	if ( stream_streamer_mode_saved ) {
		Cvar_Set( "cg_surfStreamerMode", stream_saved_streamer_mode ? "1" : "0" );
		stream_streamer_mode_saved = qfalse;
	}
	stream_active = qfalse;
	stream_backend_active = STREAM_BACKEND_NONE;
	Com_Printf( "stream_stop: stream capture stopped%s\n",
		wasExternal ? ". Stop the external ffmpeg process if it is still running." : "." );
}

qboolean CL_Streaming_EngineCaptureActive( void )
{
	return ( stream_active && stream_backend_active == STREAM_BACKEND_ENGINE ) ? qtrue : qfalse;
}

int CL_Streaming_EngineCaptureFPS( void )
{
	if ( !cl_stream_fps || cl_stream_fps->integer <= 0 ) {
		return 30;
	}
	return cl_stream_fps->integer;
}

void CL_Streaming_Init( void )
{
	cl_stream_enable = Cvar_Get( "cl_stream_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_stream_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_stream_enable, "Enable in-engine live-stream controls for idTech3-tv / Owncast-compatible RTMP publishing." );

	cl_stream_url = Cvar_Get( "cl_stream_url", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_stream_url, "RTMP ingest URL, e.g. rtmp://127.0.0.1:1935/live." );
	cl_stream_key = Cvar_Get( "cl_stream_key", "", CVAR_ARCHIVE | CVAR_PROTECTED );
	Cvar_SetDescription( cl_stream_key, "Protected stream key for idTech3-tv / Owncast RTMP ingest." );
	cl_stream_title = Cvar_Get( "cl_stream_title", "idTech3 Live", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_stream_title, "Human-readable title available to custom stream command templates as %L." );
	cl_stream_backend = Cvar_Get( "cl_stream_backend", "engine", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_stream_backend, "Streaming capture backend: engine pipes rendered frames and mixed audio; external uses desktop/audio capture command templates." );
	cl_stream_cmd = Cvar_Get( "cl_stream_cmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_stream_cmd,
		"Optional shell template for stream_start. Engine backend command must read AVI from stdin. Tokens: %P ffmpeg, %U url, %K key, %L title, %W width, %H height, %F fps, %V video bitrate, %Q audio bitrate." );
	cl_stream_ffmpeg = Cvar_Get( "cl_stream_ffmpeg", "ffmpeg", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_stream_ffmpeg, "FFmpeg executable used by stream_start (substituted as %P)." );
	cl_stream_width = Cvar_Get( "cl_stream_width", "1280", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_stream_width, "320", "7680", CV_INTEGER );
	cl_stream_height = Cvar_Get( "cl_stream_height", "720", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_stream_height, "240", "4320", CV_INTEGER );
	cl_stream_fps = Cvar_Get( "cl_stream_fps", "30", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_stream_fps, "1", "240", CV_INTEGER );
	cl_stream_bitrate = Cvar_Get( "cl_stream_bitrate", "3500k", CVAR_ARCHIVE );
	cl_stream_audio_bitrate = Cvar_Get( "cl_stream_audio_bitrate", "128k", CVAR_ARCHIVE );
	cl_stream_queueMegs = Cvar_Get( "cl_stream_queueMegs", "64", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_stream_queueMegs, "4", "512", CV_INTEGER );
	Cvar_SetDescription( cl_stream_queueMegs, "Maximum queued live-stream pipe data in MiB before capture chunks are dropped instead of growing memory." );
	cl_stream_autoStart = Cvar_Get( "cl_stream_autoStart", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_stream_autoStart, "0", "1", CV_INTEGER );
	cl_stream_platform = Cvar_Get( "cl_stream_platform", "custom", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_stream_platform, "Streaming destination preset: custom or kick." );

	Cmd_AddCommand( "stream_start", CL_StreamingStart_f );
	Cmd_AddCommand( "stream_stop", CL_StreamingStop_f );
	Cmd_AddCommand( "stream_status", CL_StreamingStatus_f );
	Cmd_AddCommand( "stream_kick_setup", CL_StreamingKickSetup_f );
	Cmd_AddCommand( "stream_kick_start", CL_StreamingKickStart_f );
	Cmd_AddCommand( "stream_idtech3tv_setup", CL_StreamingIdTech3TVSetup_f );
	Cmd_AddCommand( "stream_idtech3tv_start", CL_StreamingIdTech3TVStart_f );

	stream_active = qfalse;
	stream_backend_active = STREAM_BACKEND_NONE;
	stream_last_command[0] = '\0';
	stream_streamer_mode_saved = qfalse;
	if ( cl_stream_autoStart && cl_stream_autoStart->integer ) {
		CL_StreamingStart_f();
	}
}

void CL_Streaming_Shutdown( void )
{
	if ( stream_backend_active == STREAM_BACKEND_ENGINE ) {
		CL_CloseAVI( qfalse );
	}
	if ( stream_streamer_mode_saved ) {
		Cvar_Set( "cg_surfStreamerMode", stream_saved_streamer_mode ? "1" : "0" );
		stream_streamer_mode_saved = qfalse;
	}
	stream_active = qfalse;
	stream_backend_active = STREAM_BACKEND_NONE;
	Cmd_RemoveCommand( "stream_start" );
	Cmd_RemoveCommand( "stream_stop" );
	Cmd_RemoveCommand( "stream_status" );
	Cmd_RemoveCommand( "stream_kick_setup" );
	Cmd_RemoveCommand( "stream_kick_start" );
	Cmd_RemoveCommand( "stream_idtech3tv_setup" );
	Cmd_RemoveCommand( "stream_idtech3tv_start" );
}
