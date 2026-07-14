/*
===========================================================================
Optional live-streaming controls for idTech3-tv / Owncast-compatible RTMP.

This module intentionally launches an external broadcaster command instead of
embedding a streaming server in the engine. Players can run idTech3-tv/Owcast
separately and push gameplay to its RTMP ingest endpoint.
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
static cvar_t *cl_stream_cmd;
static cvar_t *cl_stream_ffmpeg;
static cvar_t *cl_stream_width;
static cvar_t *cl_stream_height;
static cvar_t *cl_stream_fps;
static cvar_t *cl_stream_bitrate;
static cvar_t *cl_stream_audio_bitrate;
static cvar_t *cl_stream_autoStart;

static qboolean stream_active = qfalse;
static char stream_last_command[8192];

static const char *CL_StreamingDefaultTemplate( void )
{
#ifdef _WIN32
	return "start \"idTech3 TV\" /B %P -y -f gdigrab -framerate %F -video_size %Wx%H -i desktop -f dshow -i audio=\"default\" -c:v libx264 -preset veryfast -tune zerolatency -b:v %V -pix_fmt yuv420p -c:a aac -b:a %Q -f flv \"%U/%K\"";
#elif defined(__APPLE__)
	return "nohup %P -y -f avfoundation -framerate %F -video_size %Wx%H -i \"1:0\" -c:v libx264 -preset veryfast -tune zerolatency -b:v %V -pix_fmt yuv420p -c:a aac -b:a %Q -f flv \"%U/%K\" >/tmp/idtech3-tv-ffmpeg.log 2>&1 &";
#else
	return "nohup %P -y -f x11grab -framerate %F -video_size %Wx%H -i ${DISPLAY:-:0.0} -f pulse -i default -c:v libx264 -preset veryfast -tune zerolatency -b:v %V -pix_fmt yuv420p -c:a aac -b:a %Q -f flv \"%U/%K\" >/tmp/idtech3-tv-ffmpeg.log 2>&1 &";
#endif
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
	Com_Printf( "idTech3 TV streaming:\n" );
	Com_Printf( "  enabled: %d active: %d\n", cl_stream_enable ? cl_stream_enable->integer : 0, stream_active );
	Com_Printf( "  url:     %s\n", ( cl_stream_url && cl_stream_url->string[0] ) ? cl_stream_url->string : "(unset)" );
	Com_Printf( "  key:     %s\n", ( cl_stream_key && cl_stream_key->string[0] ) ? "(protected)" : "(unset)" );
	Com_Printf( "  video:   %sx%s @ %s fps, %s video / %s audio\n",
		cl_stream_width ? cl_stream_width->string : "?",
		cl_stream_height ? cl_stream_height->string : "?",
		cl_stream_fps ? cl_stream_fps->string : "?",
		cl_stream_bitrate ? cl_stream_bitrate->string : "?",
		cl_stream_audio_bitrate ? cl_stream_audio_bitrate->string : "?" );
	if ( stream_last_command[0] ) {
		Com_Printf( "  last command: %s\n", stream_last_command );
	}
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

	tmpl = ( cl_stream_cmd && cl_stream_cmd->string[0] ) ? cl_stream_cmd->string : CL_StreamingDefaultTemplate();
	CL_StreamingBuildExpand( &ex );
	if ( !CL_PipelineExpandTemplate( stream_last_command, sizeof( stream_last_command ), tmpl, &ex ) ) {
		stream_last_command[0] = '\0';
		Com_Printf( S_COLOR_RED "stream_start: command expansion failed\n" );
		return;
	}

	Com_Printf( "stream_start: launching external RTMP publisher for idTech3-tv / Owncast\n" );
	rc = system( stream_last_command );
	if ( rc != 0 ) {
		Com_Printf( S_COLOR_RED "stream_start: command returned %d\n", rc );
		return;
	}
	stream_active = qtrue;
	Com_Printf( S_COLOR_GREEN "stream_start: broadcaster launched\n" );
}

static void CL_StreamingStop_f( void )
{
	if ( !stream_active ) {
		Com_Printf( "stream_stop: no active broadcaster tracked by engine\n" );
		return;
	}
	stream_active = qfalse;
	Com_Printf( "stream_stop: marked inactive. Stop the external ffmpeg process if it is still running.\n" );
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
	cl_stream_cmd = Cvar_Get( "cl_stream_cmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_stream_cmd,
		"Optional shell template for stream_start. Tokens: %P ffmpeg, %U url, %K key, %L title, %W width, %H height, %F fps, %V video bitrate, %Q audio bitrate." );
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
	cl_stream_autoStart = Cvar_Get( "cl_stream_autoStart", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_stream_autoStart, "0", "1", CV_INTEGER );

	Cmd_AddCommand( "stream_start", CL_StreamingStart_f );
	Cmd_AddCommand( "stream_stop", CL_StreamingStop_f );
	Cmd_AddCommand( "stream_status", CL_StreamingStatus_f );

	stream_active = qfalse;
	stream_last_command[0] = '\0';
	if ( cl_stream_autoStart && cl_stream_autoStart->integer ) {
		CL_StreamingStart_f();
	}
}

void CL_Streaming_Shutdown( void )
{
	stream_active = qfalse;
	Cmd_RemoveCommand( "stream_start" );
	Cmd_RemoveCommand( "stream_stop" );
	Cmd_RemoveCommand( "stream_status" );
}
