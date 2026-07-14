/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client demo record/playback: extracted from cl_main.c for modularization.
===========================================================================
*/

#include "client.h"
#include "cl_demo.h"
#include "../platform/cl_streaming.h"
#ifdef USE_CURL
#include "cl_curl.h"
#endif

cvar_t *cl_autoRecordDemo;
cvar_t *cl_drawRecording;
cvar_t *cl_aviFrameRate;
cvar_t *cl_aviMotionJpeg;
cvar_t *cl_forceavidemo;
cvar_t *cl_aviPipeFormat;

static void CL_Video_f( void );
static void CL_StopVideo_f( void );
static void CL_CompleteVideoName( const char *args, int argNum );

static void CL_Demo_RegisterCvars( void ) {
	cl_autoRecordDemo = Cvar_Get( "cl_autoRecordDemo", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_autoRecordDemo, "Auto-record demos when starting or joining a game." );
	cl_drawRecording = Cvar_Get( "cl_drawRecording", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_drawRecording, "Hide (0) or shorten (1) \"RECORDING\" HUD message when recording demo." );

	cl_aviFrameRate = Cvar_Get( "cl_aviFrameRate", "25", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_aviFrameRate, "1", "1000", CV_INTEGER );
	Cvar_SetDescription( cl_aviFrameRate, "The framerate used for capturing video." );
	cl_aviMotionJpeg = Cvar_Get( "cl_aviMotionJpeg", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_aviMotionJpeg, "Enable/disable the MJPEG codec for avi output." );
	cl_forceavidemo = Cvar_Get( "cl_forceavidemo", "0", 0 );
	Cvar_SetDescription( cl_forceavidemo, "Forces all demo recording into a sequence of screenshots in TGA format." );

	cl_aviPipeFormat = Cvar_Get( "cl_aviPipeFormat",
		"-preset medium -crf 23 -c:v libx264 -flags +cgop -pix_fmt yuvj420p "
		"-bf 2 -c:a aac -strict -2 -b:a 160k -movflags faststart",
		CVAR_ARCHIVE );
	Cvar_SetDescription( cl_aviPipeFormat, "Encoder parameters used for \\video-pipe." );
}

//===========================================================================================


/*
===============
CL_Video_f

video
video [filename]
===============
*/
static void CL_Video_f( void )
{
	char filename[ MAX_OSPATH ];
	const char *ext;
	qboolean pipe;
	int i;

	if( !clc.demoplaying )
	{
		Com_Printf( "The %s command can only be used when playing back demos\n", Cmd_Argv( 0 ) );
		return;
	}

	pipe = ( Q_stricmp( Cmd_Argv( 0 ), "video-pipe" ) == 0 );

	if ( pipe )
		ext = "mp4";
	else
		ext = "avi";

	if ( Cmd_Argc() == 2 )
	{
		// explicit filename
		Com_sprintf( filename, sizeof( filename ), "videos/%s", Cmd_Argv( 1 ) );

		// override video file extension
		if ( pipe )
		{
			char *sep = strrchr( filename, '/' ); // last path separator
			char *e = strrchr( filename, '.' );

			if ( e && e > sep && *(e+1) != '\0' ) {
				ext = e + 1;
				*e = '\0';
			}
		}
	}
	else
	{
		 // scan for a free filename
		for ( i = 0; i <= 9999; i++ )
		{
			Com_sprintf( filename, sizeof( filename ), "videos/video%04d.%s", i, ext );
			if ( !FS_FileExists( filename ) )
				break; // file doesn't exist
		}

		if ( i > 9999 )
		{
			Com_Printf( S_COLOR_RED "ERROR: no free file names to create video\n" );
			return;
		}

		// without extension
		Com_sprintf( filename, sizeof( filename ), "videos/video%04d", i );
	}


	clc.aviSoundFrameRemainder = 0.0f;
	clc.aviVideoFrameRemainder = 0.0f;

	Q_strncpyz( clc.videoName, filename, sizeof( clc.videoName ) );
	clc.videoIndex = 0;

	CL_OpenAVIForWriting( va( "%s.%s", clc.videoName, ext ), pipe, qfalse );
}


/*
===============
CL_StopVideo_f
===============
*/
static void CL_StopVideo_f( void )
{
	CL_CloseAVI( qfalse );
}


/*
====================
CL_CompleteRecordName
====================
*/
static void CL_CompleteVideoName(const char *args, int argNum )
{
	(void)args;
	if ( argNum == 2 )
	{
		Field_CompleteFilename( "videos", ".avi", qtrue, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}

/*
==================
CL_Demo_Frame

Demo AVI capture timing and auto-record before the main simulation step.
==================
*/
void CL_Demo_Frame( int *msec, int *realMsec ) {
	// if recording an avi, lock to a fixed fps
	if ( CL_VideoRecording() && *msec ) {
		// save the current screen
		if ( cls.state == CA_ACTIVE || cl_forceavidemo->integer || CL_Streaming_EngineCaptureActive() ) {
			float fps, frameDuration;

			if ( CL_Streaming_EngineCaptureActive() ) {
				fps = (float)CL_Streaming_EngineCaptureFPS();
			} else if ( com_timescale->value > 0.0001f ) {
				fps = MIN( cl_aviFrameRate->value / com_timescale->value, 1000.0f );
			}
			else
				fps = 1000.0f;

			frameDuration = MAX( 1000.0f / fps, 1.0f ) + clc.aviVideoFrameRemainder;

			CL_TakeVideoFrame();

			*msec = (int)frameDuration;
			clc.aviVideoFrameRemainder = frameDuration - *msec;

			*realMsec = *msec; // sync sound duration
		}
	}

	if ( cl_autoRecordDemo->integer && !clc.demoplaying ) {
		if ( cls.state == CA_ACTIVE && !clc.demorecording ) {
			// If not recording a demo, and we should be, start one
			qtime_t	now;
			const char	*nowString;
			char		*p;
			char		mapName[ MAX_QPATH ];
			char		serverName[ MAX_OSPATH ];

			Com_RealTime( &now );
			nowString = va( "%04d%02d%02d%02d%02d%02d",
					1900 + now.tm_year,
					1 + now.tm_mon,
					now.tm_mday,
					now.tm_hour,
					now.tm_min,
					now.tm_sec );

			Q_strncpyz( serverName, cls.servername, MAX_OSPATH );
			// Replace the ":" in the address as it is not a valid
			// file name character
			p = strchr( serverName, ':' );
			if ( p ) {
				*p = '.';
			}

			Q_strncpyz( mapName, COM_SkipPath( cl.mapname ), sizeof( cl.mapname ) );
			COM_StripExtension(mapName, mapName, sizeof(mapName));

			Cbuf_ExecuteText( EXEC_NOW,
					va( "record %s-%s-%s", nowString, serverName, mapName ) );
		}
		else if ( cls.state != CA_ACTIVE && clc.demorecording ) {
			// Recording, but not CA_ACTIVE, so stop recording
			CL_StopRecord_f();
		}
	}
}

/*
=======================================================================

CLIENT SIDE DEMO RECORDING

=======================================================================
*/

static void CL_WriteDemoMessage( msg_t *msg, int headerBytes ) {
	int len, swlen;

	len = clc.serverMessageSequence;
	swlen = LittleLong( len );
	FS_Write( &swlen, 4, clc.recordfile );

	len = msg->cursize - headerBytes;
	swlen = LittleLong( len );
	FS_Write( &swlen, 4, clc.recordfile );
	FS_Write( msg->data + headerBytes, len, clc.recordfile );
}

void CL_Demo_WriteServerPacket( msg_t *msg, int headerBytes ) {
	CL_WriteDemoMessage( msg, headerBytes );
}

void CL_StopRecord_f( void ) {

	if ( clc.recordfile != FS_INVALID_HANDLE ) {
		char tempName[MAX_OSPATH];
		char finalName[MAX_OSPATH];
		int protocol;
		int len, sequence;

		len = -1;
		FS_Write( &len, 4, clc.recordfile );
		FS_Write( &len, 4, clc.recordfile );
		FS_FCloseFile( clc.recordfile );
		clc.recordfile = FS_INVALID_HANDLE;

		if ( clc.dm68compat || clc.demoplaying ) {
			protocol = OLD_PROTOCOL_VERSION;
		} else {
			protocol = NEW_PROTOCOL_VERSION;
		}

		if ( com_protocol->integer != DEFAULT_PROTOCOL_VERSION ) {
			protocol = com_protocol->integer;
		}

		Com_sprintf( tempName, sizeof( tempName ), "%s.tmp", clc.recordName );

		Com_sprintf( finalName, sizeof( finalName ), "%s.%s%d", clc.recordName, DEMOEXT, protocol );

		if ( clc.explicitRecordName ) {
			FS_Remove( finalName );
		} else {
			sequence = 0;
			while ( FS_FileExists( finalName ) && ++sequence < 1000 ) {
				Com_sprintf( finalName, sizeof( finalName ), "%s-%02d.%s%d",
					clc.recordName, sequence, DEMOEXT, protocol );
			}
		}

		FS_Rename( tempName, finalName );
	}

	if ( !clc.demorecording ) {
		Com_Printf( "Not recording a demo.\n" );
	} else {
		Com_Printf( "Stopped demo recording.\n" );
	}

	clc.demorecording = qfalse;
	clc.spDemoRecording = qfalse;
}

static void CL_WriteServerCommands( msg_t *msg ) {
	int i;

	if ( clc.serverCommandSequence - clc.demoCommandSequence > 0 ) {

		if ( clc.serverCommandSequence - clc.demoCommandSequence > MAX_RELIABLE_COMMANDS ) {
			clc.demoCommandSequence = clc.serverCommandSequence - MAX_RELIABLE_COMMANDS;
		}

		for ( i = clc.demoCommandSequence + 1; i <= clc.serverCommandSequence; i++ ) {
			MSG_WriteByte( msg, svc_serverCommand );
			MSG_WriteLong( msg, i );
			MSG_WriteString( msg, clc.serverCommands[ i & ( MAX_RELIABLE_COMMANDS - 1 ) ] );
		}
	}

	clc.demoCommandSequence = clc.serverCommandSequence;
}

static void CL_WriteGamestate( qboolean initial ) {
	byte bufData[MAX_MSGLEN_BUF];
	char *s;
	msg_t msg;
	int i;
	int len;
	entityState_t *ent;
	entityState_t nullstate;

	MSG_Init( &msg, bufData, MAX_MSGLEN );
	MSG_Bitstream( &msg );

	MSG_WriteLong( &msg, clc.reliableSequence );

	if ( initial ) {
		clc.demoMessageSequence = 1;
		clc.demoCommandSequence = clc.serverCommandSequence;
	} else {
		CL_WriteServerCommands( &msg );
	}

	clc.demoDeltaNum = 0;

	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, clc.serverCommandSequence );

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( !cl.gameState.stringOffsets[i] ) {
			continue;
		}
		s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		MSG_WriteByte( &msg, svc_configstring );
		MSG_WriteShort( &msg, i );
		MSG_WriteBigString( &msg, s );
	}

	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		if ( !cl.baselineUsed[i] )
			continue;
		ent = &cl.entityBaselines[i];
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, ent, qtrue );
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, clc.clientNum );

	MSG_WriteLong( &msg, clc.checksumFeed );

	MSG_WriteByte( &msg, svc_EOF );

	if ( clc.demoplaying )
		len = LittleLong( clc.demoMessageSequence - 1 );
	else
		len = LittleLong( clc.serverMessageSequence - 1 );

	FS_Write( &len, 4, clc.recordfile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clc.recordfile );
	FS_Write( msg.data, msg.cursize, clc.recordfile );
}

static void CL_EmitPacketEntities( clSnapshot_t *from, clSnapshot_t *to, msg_t *msg, entityState_t *oldents ) {
	entityState_t *oldent, *newent;
	int oldindex, newindex;
	int oldnum, newnum;
	int from_num_entities;

	if ( !from ) {
		from_num_entities = 0;
	} else {
		from_num_entities = from->numEntities;
	}

	newent = NULL;
	oldent = NULL;
	newindex = 0;
	oldindex = 0;
	while ( newindex < to->numEntities || oldindex < from_num_entities ) {
		if ( newindex >= to->numEntities ) {
			newnum = MAX_GENTITIES + 1;
		} else {
			newent = &cl.parseEntities[( to->parseEntitiesNum + newindex ) % MAX_PARSE_ENTITIES];
			newnum = newent->number;
		}

		if ( oldindex >= from_num_entities ) {
			oldnum = MAX_GENTITIES + 1;
		} else {
			oldent = &oldents[oldindex];
			oldnum = oldent->number;
		}

		if ( newnum == oldnum ) {
			MSG_WriteDeltaEntity( msg, oldent, newent, qfalse );
			oldindex++;
			newindex++;
			continue;
		}

		if ( newnum < oldnum ) {
			MSG_WriteDeltaEntity( msg, &cl.entityBaselines[newnum], newent, qtrue );
			newindex++;
			continue;
		}

		if ( newnum > oldnum ) {
			MSG_WriteDeltaEntity( msg, oldent, NULL, qtrue );
			oldindex++;
			continue;
		}
	}

	MSG_WriteBits( msg, ( MAX_GENTITIES - 1 ), GENTITYNUM_BITS );
}

static void CL_WriteSnapshot( void ) {

	static clSnapshot_t saved_snap;
	static entityState_t saved_ents[MAX_SNAPSHOT_ENTITIES];

	clSnapshot_t *snap, *oldSnap;
	byte bufData[MAX_MSGLEN_BUF];
	msg_t msg;
	int i, len;

	snap = &cl.snapshots[cl.snap.messageNum & PACKET_MASK];

	if ( clc.demoDeltaNum == 0 ) {
		oldSnap = NULL;
	} else {
		oldSnap = &saved_snap;
	}

	MSG_Init( &msg, bufData, MAX_MSGLEN );
	MSG_Bitstream( &msg );

	MSG_WriteLong( &msg, clc.reliableSequence );

	CL_WriteServerCommands( &msg );

	MSG_WriteByte( &msg, svc_snapshot );
	MSG_WriteLong( &msg, snap->serverTime );
	MSG_WriteByte( &msg, clc.demoDeltaNum );
	MSG_WriteByte( &msg, snap->snapFlags );
	MSG_WriteByte( &msg, snap->areabytes );
	MSG_WriteData( &msg, snap->areamask, snap->areabytes );
	if ( oldSnap )
		MSG_WriteDeltaPlayerstate( &msg, &oldSnap->ps, &snap->ps );
	else
		MSG_WriteDeltaPlayerstate( &msg, NULL, &snap->ps );

	CL_EmitPacketEntities( oldSnap, snap, &msg, saved_ents );

	MSG_WriteByte( &msg, svc_EOF );

	if ( clc.demoplaying )
		len = LittleLong( clc.demoMessageSequence );
	else
		len = LittleLong( clc.serverMessageSequence );
	FS_Write( &len, 4, clc.recordfile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clc.recordfile );
	FS_Write( msg.data, msg.cursize, clc.recordfile );

	for ( i = 0; i < snap->numEntities; i++ )
		saved_ents[i] = cl.parseEntities[( snap->parseEntitiesNum + i ) % MAX_PARSE_ENTITIES];

	saved_snap = *snap;
	saved_snap.parseEntitiesNum = 0;

	clc.demoMessageSequence++;
	clc.demoDeltaNum = 1;
}

static void CL_Record_f( void ) {
	char demoName[MAX_OSPATH];
	char name[MAX_OSPATH];
	char demoExt[16];
	const char *ext;
	qtime_t t;

	if ( Cmd_Argc() > 2 ) {
		Com_Printf( "record <demoname>\n" );
		return;
	}

	if ( clc.demorecording ) {
		if ( !clc.spDemoRecording ) {
			Com_Printf( "Already recording.\n" );
		}
		return;
	}

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}

	if ( NET_IsLocalAddress( &clc.serverAddress ) && !Cvar_VariableIntegerValue( "g_synchronousClients" ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: You should set 'g_synchronousClients 1' for smoother demo recording\n" );
	}

	if ( Cmd_Argc() == 2 ) {
		Q_strncpyz( demoName, Cmd_Argv( 1 ), sizeof( demoName ) );
		ext = COM_GetExtension( demoName );
		if ( *ext ) {
			Com_sprintf( demoExt, sizeof( demoExt ), "%s%d", DEMOEXT, OLD_PROTOCOL_VERSION );
			if ( Q_stricmp( ext, demoExt ) == 0 ) {
				*( strrchr( demoName, '.' ) ) = '\0';
			} else {
				Com_sprintf( demoExt, sizeof( demoExt ), "%s%d", DEMOEXT, NEW_PROTOCOL_VERSION );
				if ( Q_stricmp( ext, demoExt ) == 0 ) {
					*( strrchr( demoName, '.' ) ) = '\0';
				}
			}
		}
		Com_sprintf( name, sizeof( name ), "demos/%s", demoName );

		clc.explicitRecordName = qtrue;
	} else {

		Com_RealTime( &t );
		Com_sprintf( name, sizeof( name ), "demos/demo-%04d%02d%02d-%02d%02d%02d",
			1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec );

		clc.explicitRecordName = qfalse;
	}

	Q_strncpyz( clc.recordName, name, sizeof( clc.recordName ) );

	Com_Printf( "recording to %s.\n", name );

	Q_strcat( name, sizeof( name ), ".tmp" );

	clc.recordfile = FS_FOpenFileWrite( name );
	if ( clc.recordfile == FS_INVALID_HANDLE ) {
		Com_Printf( "ERROR: couldn't open.\n" );
		clc.recordName[0] = '\0';
		return;
	}

	clc.demorecording = qtrue;

	Com_TruncateLongString( clc.recordNameShort, clc.recordName );

	if ( Cvar_VariableIntegerValue( "ui_recordSPDemo" ) ) {
		clc.spDemoRecording = qtrue;
	} else {
		clc.spDemoRecording = qfalse;
	}

	clc.demowaiting = qtrue;

	clc.dm68compat = qtrue;

	CL_WriteGamestate( qtrue );
}

static void CL_CompleteRecordName( const char *args, int argNum ) {
	(void)args;
	if ( argNum == 2 ) {
		char demoExt[16];

		Com_sprintf( demoExt, sizeof( demoExt ), "." DEMOEXT "%d", com_protocol->integer );
		Field_CompleteFilename( "demos", demoExt, qtrue, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}

/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/

static void CL_NextDemo( void ) {
	char v[MAX_CVAR_VALUE_STRING];

	Cvar_VariableStringBuffer( "nextdemo", v, sizeof( v ) );
	Com_DPrintf( "CL_NextDemo: %s\n", v );
	if ( !v[0] ) {
		return;
	}

	Cvar_Set( "nextdemo", "" );
	Cbuf_AddText( v );
	Cbuf_AddText( "\n" );
	Cbuf_Execute();
}

static void CL_DemoCompleted( void ) {
	if ( com_timedemo->integer ) {
		int time;

		time = Sys_Milliseconds() - clc.timeDemoStart;
		if ( time > 0 ) {
			Com_Printf( "%i frames, %3.*f seconds: %3.1f fps\n", clc.timeDemoFrames,
				time > 10000 ? 1 : 2, time / 1000.0, clc.timeDemoFrames * 1000.0 / time );
		}
	}

	CL_Disconnect( qtrue );
	CL_NextDemo();
}

void CL_ReadDemoMessage( void ) {
	int r;
	msg_t buf;
	byte bufData[MAX_MSGLEN_BUF];
	int s;

	if ( clc.demofile == FS_INVALID_HANDLE ) {
		CL_DemoCompleted();
		return;
	}

	r = FS_Read( &s, 4, clc.demofile );
	if ( r != 4 ) {
		CL_DemoCompleted();
		return;
	}
	clc.serverMessageSequence = LittleLong( s );

	MSG_Init( &buf, bufData, MAX_MSGLEN );

	r = FS_Read( &buf.cursize, 4, clc.demofile );
	if ( r != 4 ) {
		CL_DemoCompleted();
		return;
	}
	buf.cursize = LittleLong( buf.cursize );
	if ( buf.cursize == -1 ) {
		CL_DemoCompleted();
		return;
	}
	if ( buf.cursize > buf.maxsize ) {
		Com_Error( ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN" );
	}
	r = FS_Read( buf.data, buf.cursize, clc.demofile );
	if ( r != buf.cursize ) {
		Com_Printf( "Demo file was truncated.\n" );
		CL_DemoCompleted();
		return;
	}

	clc.lastPacketTime = cls.realtime;
	buf.readcount = 0;

	clc.demoCommandSequence = clc.serverCommandSequence;

	CL_ParseServerMessage( &buf );

	if ( clc.demorecording ) {
		if ( clc.eventMask & EM_GAMESTATE ) {
			CL_WriteGamestate( qfalse );
		} else if ( clc.eventMask & ( EM_SNAPSHOT | EM_COMMAND ) ) {
			CL_WriteSnapshot();
		}
	}
}

static int CL_WalkDemoExt( const char *arg, char *name, int name_len, fileHandle_t *handle ) {
	int i;

	*handle = FS_INVALID_HANDLE;
	i = 0;

	while ( demo_protocols[i] ) {
		Com_sprintf( name, name_len, "demos/%s.%s%d", arg, DEMOEXT, demo_protocols[i] );
		FS_BypassPure();
		FS_FOpenFileRead( name, handle, qtrue );
		FS_RestorePure();
		if ( *handle != FS_INVALID_HANDLE ) {
			Com_Printf( "Demo file: %s\n", name );
			return demo_protocols[i];
		} else
			Com_Printf( "Not found: %s\n", name );
		i++;
	}
	return -1;
}

static qboolean CL_DemoNameCallback_f( const char *filename, int length ) {
	const int ext_len = (int)strlen( "." DEMOEXT );
	const int num_len = 2;
	int version;

	if ( length <= ext_len + num_len || Q_stricmpn( filename + length - ( ext_len + num_len ), "." DEMOEXT, (size_t)ext_len ) != 0 )
		return qfalse;

	version = atoi( filename + length - num_len );
	if ( version == com_protocol->integer )
		return qtrue;

	if ( version < 66 || version > NEW_PROTOCOL_VERSION )
		return qfalse;

	return qtrue;
}

static void CL_CompleteDemoName( const char *args, int argNum ) {
	(void)args;
	if ( argNum == 2 ) {
		FS_SetFilenameCallback( CL_DemoNameCallback_f );
		Field_CompleteFilename( "demos", "." DEMOEXT "??", qfalse, FS_MATCH_ANY | FS_MATCH_STICK | FS_MATCH_SUBDIRS );
		FS_SetFilenameCallback( NULL );
	}
}

	static void CL_PlayDemo_f( void ) {
	char name[MAX_OSPATH];
	const char *arg;
	const char *ext_test;
	int protocol, i;
	char retry[MAX_OSPATH];
	const char *shortname, *slash;
	fileHandle_t hFile;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "demo <demoname>\n" );
		return;
	}

	arg = Cmd_Argv( 1 );

	ext_test = strrchr( arg, '.' );
	if ( ext_test && !Q_stricmpn( ext_test + 1, DEMOEXT, ARRAY_LEN( DEMOEXT ) - 1 ) ) {
		protocol = atoi( ext_test + ARRAY_LEN( DEMOEXT ) );

		for ( i = 0; demo_protocols[i]; i++ ) {
			if ( demo_protocols[i] == protocol )
				break;
		}

		if ( demo_protocols[i] || protocol == com_protocol->integer ) {
			Com_sprintf( name, sizeof( name ), "demos/%s", arg );
			FS_BypassPure();
			FS_FOpenFileRead( name, &hFile, qtrue );
			FS_RestorePure();
		} else {
			size_t len;

			Com_Printf( "Protocol %d not supported for demos\n", protocol );
			len = (size_t)( ext_test - arg );

			if ( len > ARRAY_LEN( retry ) - 1 ) {
				len = ARRAY_LEN( retry ) - 1;
			}

			Q_strncpyz( retry, arg, len + 1 );
			retry[len] = '\0';
			protocol = CL_WalkDemoExt( retry, name, sizeof( name ), &hFile );
		}
	} else
		protocol = CL_WalkDemoExt( arg, name, sizeof( name ), &hFile );

	if ( hFile == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_YELLOW "couldn't open %s\n", name );
		return;
	}

	FS_FCloseFile( hFile );
	hFile = FS_INVALID_HANDLE;

	Cvar_Set( "sv_killserver", "2" );

	CL_Disconnect( qtrue );

	if ( FS_FOpenFileRead( name, &clc.demofile, qtrue ) == -1 ) {
		Com_Error( ERR_DROP, "couldn't open %s\n", name );
	}

	if ( ( slash = strrchr( name, '/' ) ) != NULL )
		shortname = slash + 1;
	else
		shortname = name;

	Q_strncpyz( clc.demoName, shortname, sizeof( clc.demoName ) );

	Con_Close();

	cls.state = CA_CONNECTED;
	clc.demoplaying = qtrue;
	Q_strncpyz( cls.servername, shortname, sizeof( cls.servername ) );

	if ( protocol <= OLD_PROTOCOL_VERSION )
		clc.compat = qtrue;
	else
		clc.compat = qfalse;

#ifdef USE_CURL
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED && !Com_DL_InProgress( &download ) ) {
#else
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED ) {
#endif
		CL_ReadDemoMessage();
	}

	clc.firstDemoFrameSkipped = qfalse;
}

void CL_Demo_Init( void ) {
	CL_Demo_RegisterCvars();
	Cmd_AddCommand( "record", CL_Record_f );
	Cmd_SetCommandCompletionFunc( "record", CL_CompleteRecordName );
	Cmd_AddCommand( "demo", CL_PlayDemo_f );
	Cmd_SetCommandCompletionFunc( "demo", CL_CompleteDemoName );
	Cmd_AddCommand( "stoprecord", CL_StopRecord_f );
	Cmd_AddCommand( "video", CL_Video_f );
	Cmd_AddCommand( "video-pipe", CL_Video_f );
	Cmd_SetCommandCompletionFunc( "video", CL_CompleteVideoName );
	Cmd_AddCommand( "stopvideo", CL_StopVideo_f );
}

void CL_Demo_Shutdown( void ) {
	Cmd_RemoveCommand( "record" );
	Cmd_RemoveCommand( "demo" );
	Cmd_RemoveCommand( "stoprecord" );
	Cmd_RemoveCommand( "video" );
	Cmd_RemoveCommand( "stopvideo" );
}

void CL_Demo_InitCommands( void ) {
	CL_Demo_Init();
}

void CL_Demo_ShutdownCommands( void ) {
	CL_Demo_Shutdown();
}
