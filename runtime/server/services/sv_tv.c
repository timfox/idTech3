/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "server.h"

tvState_t tv;
cvar_t *sv_tvAuto;
cvar_t *sv_tvAutoMinPlayers;
cvar_t *sv_tvAutoMinPlayersSecs;
cvar_t *sv_tvpath;
cvar_t *sv_tvDownload;
cvar_t *sv_tvLive;
cvar_t *sv_tvLiveKeyframeMsec;


/*
===============
SV_TV_Init
===============
*/
void SV_TV_Init( void ) {
	sv_tvAuto = Cvar_Get( "sv_tvAuto", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_tvAuto, "Surf TV: automatically start TVD recording on map load." );

	sv_tvAutoMinPlayers = Cvar_Get( "sv_tvAutoMinPlayers", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_tvAutoMinPlayers, "Surf TV: min concurrent non-spectator human players to keep an auto-recording. 0 = always keep." );

	sv_tvAutoMinPlayersSecs = Cvar_Get( "sv_tvAutoMinPlayersSecs", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_tvAutoMinPlayersSecs, "Surf TV: seconds the player threshold must be continuously met. 0 = instantaneous." );

	sv_tvpath = Cvar_Get( "sv_tvpath", "demos", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_tvpath, "Surf TV: directory for TVD recordings." );

	sv_tvDownload = Cvar_Get( "sv_tvDownload", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_tvDownload, "Surf TV: notify clients to download TVD recordings via HTTP at end of match. Requires sv_dlURL." );

	sv_tvLive = Cvar_Get( "sv_tvLive", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_tvLive, "Surf TV: broadcast the in-progress match over a loopback TCP socket (127.0.0.1:net_port) for live spectating. 0 = off." );

	sv_tvLiveKeyframeMsec = Cvar_Get( "sv_tvLiveKeyframeMsec", "1000", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( sv_tvLiveKeyframeMsec, "Surf TV: live-stream keyframe interval in milliseconds. Lower = lower late-join latency, more bandwidth." );
}


/*
===============
SV_TV_FileWrite

Write data to TV file and track file offset.
===============
*/
static void SV_TV_FileWrite( const void *data, int len, fileHandle_t f ) {
	unsigned int newOffset;

	FS_Write( data, len, f );

	newOffset = tv.fileOffset + (unsigned int)len;
	if ( newOffset < tv.fileOffset ) {
		tv.fileOffsetHi++;
	}
	tv.fileOffset = newOffset;
}


/*
===============
SV_TV_CompressWrite

Feed data into the zstd compressor and flush output to file.
===============
*/
static void SV_TV_CompressWrite( const void *data, int len ) {
	ZSTD_inBuffer in = { data, (size_t)len, 0 };

	while ( in.pos < in.size ) {
		ZSTD_outBuffer out = { tv.zstdOutBuf, ZSTD_OUT_BUF_SIZE, 0 };
		ZSTD_compressStream2( tv.cstream, &out, &in, ZSTD_e_continue );
		if ( out.pos > 0 ) {
			SV_TV_FileWrite( tv.zstdOutBuf, (int)out.pos, tv.file );
		}
	}
}


/*
===============
SV_TV_LogEvent

Append a structured event line to the active g_log file (the same
log the game module writes Kill:/Award:/etc. lines to). Used by the
demo lifecycle to surface DemoSaved / DemoDiscarded events that
Surf's collector log parser keys on. Format matches the
existing log convention: "<ISO-local> <EventType>: <args>" — the QVM
uses localtime() in G_LogPrintf, so we follow suit to keep lines
chronologically interleaved.

No-op if g_log is empty (server isn't logging) or the file can't be
opened. The FS layer is single-threaded, so the brief append-and-close
won't collide with the QVM's G_LogPrintf writes.
===============
*/
static void SV_TV_LogEvent( const char *line ) {
	const char *gLog;
	fileHandle_t f;
	time_t now;
	struct tm *tm_info;
	char ts[32];

	gLog = Cvar_VariableString( "g_log" );
	if ( !gLog[0] ) {
		return;
	}
	f = FS_FOpenFileAppend( gLog );
	if ( !f ) {
		return;
	}
	now = time( NULL );
	tm_info = localtime( &now );
	strftime( ts, sizeof( ts ), "%Y-%m-%dT%H:%M:%S", tm_info );
	FS_Printf( f, "%s %s\n", ts, line );
	FS_FCloseFile( f );
}


/*
===============
SV_TV_DefaultName

Generate a default recording name: prefer g_matchUUID, fall back to timestamp.
===============
*/
static const char *SV_TV_DefaultName( char *buf, int bufSize ) {
	const char *uuid = Cvar_VariableString( "g_matchUUID" );
	if ( uuid[0] != '\0' ) {
		return uuid;
	}
	time_t now = time( NULL );
	struct tm *tm_info = localtime( &now );
	strftime( buf, bufSize, "%Y%m%d_%H%M%S", tm_info );
	return buf;
}


/*
===============
SV_TV_StartRecord
===============
*/
static void SV_TV_StartRecord( const char *filename ) {
	char path[MAX_QPATH];
	int i;
	int val;
	char timestamp[64];
	time_t now;
	struct tm *tm_info;

	if ( sv.state != SS_GAME ) {
		Com_Printf( "TV: Not recording, server not running.\n" );
		return;
	}

	if ( tv.recording ) {
		Com_Printf( "TV: Already recording.\n" );
		return;
	}

	Com_Memset( &tv, 0, sizeof( tv ) );

	// Store base path (without extension) for later rename
	Com_sprintf( tv.recordingPath, sizeof( tv.recordingPath ), "%s/%s", sv_tvpath->string, filename );

	// Open as .tvd.tmp (renamed to .tvd on successful finalization)
	Com_sprintf( path, sizeof( path ), "%s.tvd.tmp", tv.recordingPath );
	tv.file = FS_FOpenFileWrite( path );
	if ( !tv.file ) {
		Com_Printf( "TV: Could not open %s for writing.\n", path );
		return;
	}

	tv.fileOffset = 0;
	tv.fileOffsetHi = 0;

	// Write header: magic
	SV_TV_FileWrite( "TVD1", 4, tv.file );

	// Protocol version
	val = 1;
	SV_TV_FileWrite( &val, 4, tv.file );

	// sv_fps
	val = sv_fps->integer;
	SV_TV_FileWrite( &val, 4, tv.file );

	// maxclients
	val = sv.maxclients;
	SV_TV_FileWrite( &val, 4, tv.file );

	// Map name (null-terminated)
	SV_TV_FileWrite( sv_mapname->string, (int)strlen( sv_mapname->string ) + 1, tv.file );

	// Timestamp (null-terminated ISO 8601)
	now = time( NULL );
	tm_info = localtime( &now );
	strftime( timestamp, sizeof( timestamp ), "%Y-%m-%dT%H:%M:%S", tm_info );
	SV_TV_FileWrite( timestamp, (int)strlen( timestamp ) + 1, tv.file );

	// Write configstrings
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		int len;
		unsigned short idx, slen;

		if ( !sv.configstrings[i] || sv.configstrings[i][0] == '\0' ) {
			continue;
		}

		len = (int)strlen( sv.configstrings[i] );
		idx = (unsigned short)i;
		slen = (unsigned short)len;
		SV_TV_FileWrite( &idx, 2, tv.file );
		SV_TV_FileWrite( &slen, 2, tv.file );
		SV_TV_FileWrite( sv.configstrings[i], len, tv.file );
	}

	// Configstring terminator
	{
		unsigned short term = 0xFFFF;
		SV_TV_FileWrite( &term, 2, tv.file );
	}

	// Init zstd streaming compressor
	tv.cstream = ZSTD_createCStream();
	ZSTD_CCtx_setParameter( tv.cstream, ZSTD_c_compressionLevel, -3 );
	ZSTD_initCStream( tv.cstream, -3 );

	// Zero baselines
	Com_Memset( tv.prevEntities, 0, sizeof( tv.prevEntities ) );
	Com_Memset( tv.prevEntityBitmask, 0, sizeof( tv.prevEntityBitmask ) );
	Com_Memset( tv.prevPlayers, 0, sizeof( tv.prevPlayers ) );
	Com_Memset( tv.prevPlayerBitmask, 0, sizeof( tv.prevPlayerBitmask ) );

	// Clear per-frame state
	Com_Memset( tv.csChanged, 0, sizeof( tv.csChanged ) );
	tv.cmdCount = 0;
	tv.cmdBufUsed = 0;

	tv.recording = qtrue;
	tv.frameCount = 0;

	// Recording shares tv.prevEntities with the live stream, and the memset above
	// re-zeroed it. If a session is already live (warmup), force a keyframe so
	// viewers re-base cleanly off the zeroed baseline; otherwise start the session
	// now. Gate the start on sv_tvLive: with streaming off the listener was never
	// bound, so starting would only make SV_TVStream_IsActive() lie.
	if ( SV_TVStream_IsActive() ) {
		SV_TVStream_ForceKeyframe();
	} else if ( sv_tvLive->integer ) {
		SV_TVStream_StartStream();
	}

	Com_Printf( "TV: Recording to %s\n", path );
}


/*
===============
SV_TV_StartRecord_f
===============
*/
void SV_TV_StartRecord_f( void ) {
	const char *filename;
	char defaultName[MAX_QPATH];

	if ( Cmd_Argc() >= 2 ) {
		filename = Cmd_Argv( 1 );
	} else {
		filename = SV_TV_DefaultName( defaultName, sizeof( defaultName ) );
	}

	SV_TV_StartRecord( filename );
}


/*
===============
SV_TV_WriteFrame
===============
*/
void SV_TV_WriteFrame( void ) {
	msg_t msg;
	int i;
	byte curEntityBitmask[MAX_GENTITIES/8];
	byte curPlayerBitmask[MAX_CLIENTS/8];
	int csCount;
	unsigned int frameSize;

	// Check for deferred auto-start
	if ( tv.autoPending ) {
		const char *matchState = Cvar_VariableString( "g_matchState" );
		qboolean started = qfalse;

		if ( matchState[0] != '\0' ) {
			// Match-state-aware mod (e.g. Surf): start on "active"
			if ( !Q_stricmp( matchState, "active" ) ) {
				char name[MAX_QPATH];

				tv.autoPending = qfalse;
				SV_TV_StartRecord( SV_TV_DefaultName( name, sizeof( name ) ) );
				tv.autoRecording = qtrue;
				started = qtrue;
			}
		} else {
			// Fallback: start when first client connects
			for ( i = 0; i < sv.maxclients; i++ ) {
				if ( svs.clients[i].state == CS_ACTIVE ) {
					char name[MAX_QPATH];

					tv.autoPending = qfalse;
					SV_TV_StartRecord( SV_TV_DefaultName( name, sizeof( name ) ) );
					tv.autoRecording = qtrue;
					started = qtrue;
					break;
				}
			}
		}
		// Recording just began: return so the first recorded frame is the NEXT
		// server frame (keeps the .tvd's first-frame timing identical to before).
		// Warmup (still pending): fall through to stream this frame live.
		if ( started ) {
			return;
		}
	}

	// Produce a frame if recording the .tvd OR a live session is active (warmup
	// streams before recording starts). File writes + duration below stay gated
	// on tv.recording; the encode + tap send run for both.
	if ( !tv.recording && !SV_TVStream_IsActive() ) {
		return;
	}

	if ( tv.recording ) {
		// Track whether player threshold has been met for auto-recordings
		if ( tv.autoRecording && !tv.keepRecording && sv_tvAutoMinPlayers->integer > 0 ) {
			int playerCount = 0;

			for ( i = 0; i < sv.maxclients; i++ ) {
				if ( svs.clients[i].state == CS_ACTIVE &&
					 svs.clients[i].netchan.remoteAddress.type != NA_BOT ) {
					playerState_t *ps = SV_GameClientNum( i );
					if ( ps->persistant[ PERS_TEAM ] != TEAM_SPECTATOR ) {
						playerCount++;
					}
				}
			}

			if ( playerCount >= sv_tvAutoMinPlayers->integer ) {
				if ( sv_tvAutoMinPlayersSecs->integer <= 0 ) {
					tv.keepRecording = qtrue;    // instantaneous check
				} else if ( tv.thresholdMetTime == 0 ) {
					tv.thresholdMetTime = sv.time;   // start timing
				} else if ( sv.time - tv.thresholdMetTime >= sv_tvAutoMinPlayersSecs->integer * 1000 ) {
					tv.keepRecording = qtrue;    // duration met
				}
			} else {
				tv.thresholdMetTime = 0;         // reset timer
			}
		}

		// Track server time range for duration (recorded frames only)
		if ( tv.frameCount == 0 ) {
			tv.firstServerTime = sv.time;
		}
		tv.lastServerTime = sv.time;
	}

	// Init message buffer
	MSG_Init( &msg, tv.msgBuf, MAX_TV_MSGLEN );
	MSG_Bitstream( &msg );

	// Write server time
	MSG_WriteLong( &msg, sv.time );

	// --- Entity encoding ---
	Com_Memset( curEntityBitmask, 0, sizeof( curEntityBitmask ) );

	// Build current entity bitmask. We run before SV_SendClientMessages, so stock's
	// "FIXING ENT->S.NUMBER" normalization (SV_BuildClientSnapshot) hasn't fixed up
	// entities the game left with s.number != slot yet. The wire identity is
	// s.number but our bitmask/baseline are slot-indexed and the reader keys by
	// number, so normalize to the slot here, exactly as stock does.
	for ( i = 0; i < sv.num_entities; i++ ) {
		sharedEntity_t *ent = SV_GentityNum( i );
		if ( !ent->r.linked ) {
			continue;
		}
		if ( ent->s.number != i ) {
			ent->s.number = i;
		}
		if ( ent->r.svFlags & SVF_NOCLIENT ) {
			continue;
		}
		curEntityBitmask[i >> 3] |= ( 1 << ( i & 7 ) );
	}

	// Write entity bitmask
	MSG_WriteData( &msg, curEntityBitmask, sizeof( curEntityBitmask ) );

	// Write delta-encoded entities. force=qtrue so a bitmask-flagged slot always
	// emits its number even when nothing changed: an empty linked entity (all
	// non-number fields zero, e.g. q3dm2 slot 119) would otherwise be omitted from
	// the wire yet still flagged, and the reader renders it as a zeroed phantom that
	// clobbers cg_entities[0].
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		entityState_t *es;

		if ( !( curEntityBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			continue;
		}

		es = &SV_GentityNum( i )->s;
		MSG_WriteDeltaEntity( &msg, &tv.prevEntities[i], es, qtrue );
	}

	// Entity end marker
	MSG_WriteBits( &msg, MAX_GENTITIES - 1, GENTITYNUM_BITS );

	// --- Player encoding ---
	Com_Memset( curPlayerBitmask, 0, sizeof( curPlayerBitmask ) );

	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state == CS_ACTIVE ) {
			curPlayerBitmask[i >> 3] |= ( 1 << ( i & 7 ) );
		}
	}

	// Write player bitmask
	MSG_WriteData( &msg, curPlayerBitmask, sizeof( curPlayerBitmask ) );

	// Write delta-encoded players
	for ( i = 0; i < sv.maxclients; i++ ) {
		playerState_t *ps;

		if ( !( curPlayerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			continue;
		}

		ps = SV_GameClientNum( i );
		MSG_WriteByte( &msg, i );
		MSG_WriteDeltaPlayerstate( &msg, &tv.prevPlayers[i], ps );
	}

	// --- Configstring changes ---
	csCount = 0;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( tv.csChanged[i] ) {
			csCount++;
		}
	}

	MSG_WriteShort( &msg, csCount );

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		int len;

		if ( !tv.csChanged[i] ) {
			continue;
		}

		len = sv.configstrings[i] ? (int)strlen( sv.configstrings[i] ) : 0;
		MSG_WriteShort( &msg, i );
		MSG_WriteShort( &msg, len );
		if ( len > 0 ) {
			MSG_WriteData( &msg, sv.configstrings[i], len );
		}
	}

	Com_Memset( tv.csChanged, 0, sizeof( tv.csChanged ) );

	// --- Server commands ---
	MSG_WriteShort( &msg, tv.cmdCount );

	for ( i = 0; i < tv.cmdCount; i++ ) {
		MSG_WriteByte( &msg, tv.cmds[i].target == -1 ? 255 : (byte)tv.cmds[i].target );
		MSG_WriteShort( &msg, tv.cmds[i].len );
		MSG_WriteData( &msg, tv.cmdBuf + tv.cmds[i].offset, tv.cmds[i].len );
	}

	tv.cmdCount = 0;
	tv.cmdBufUsed = 0;

#ifdef USE_VOIP
	// --- VOIP packets ---
	MSG_WriteShort( &msg, tv.voipCount );

	for ( i = 0; i < tv.voipCount; i++ ) {
		tvVoipPacket_t *tvp = &tv.voipPackets[i];
		MSG_WriteByte( &msg, tvp->sender );
		MSG_WriteByte( &msg, tvp->generation );
		MSG_WriteLong( &msg, tvp->sequence );
		MSG_WriteByte( &msg, tvp->frames );
		MSG_WriteByte( &msg, tvp->flags );
		MSG_WriteData( &msg, tvp->recips, sizeof( tvp->recips ) );
		MSG_WriteShort( &msg, tvp->len );
		MSG_WriteData( &msg, tv.voipBuf + tvp->offset, tvp->len );
	}

	tv.voipCount = 0;
	tv.voipBufUsed = 0;
#endif

	// A frame that won't fit can't be recorded or streamed coherently. Drop it;
	// when recording, abandon the file (matches prior behavior). When only
	// streaming (warmup), just skip this frame — the next one re-keyframes.
	if ( msg.overflowed ) {
		if ( tv.recording ) {
			Com_Printf( "TV: Frame %i overflowed message buffer, stopping recording.\n", tv.frameCount );
			SV_TV_StopRecord( qtrue );
		}
		return;
	}

	// --- Flush to file (recorded frames only) ---
	if ( tv.recording ) {
		frameSize = (unsigned int)msg.cursize;
		SV_TV_CompressWrite( &frameSize, 4 );
		SV_TV_CompressWrite( msg.data, msg.cursize );
	}

	// Send to the live tap (no-op if no stream session / listener).
	SV_TVStream_Frame( msg.data, msg.cursize, sv.time );

	// Save current state as previous for next delta.
	// Zero removed entities/players so reappearing ones get a full delta.
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		if ( curEntityBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) {
			tv.prevEntities[i] = SV_GentityNum( i )->s;
		} else {
			Com_Memset( &tv.prevEntities[i], 0, sizeof( entityState_t ) );
		}
	}
	Com_Memcpy( tv.prevEntityBitmask, curEntityBitmask, sizeof( curEntityBitmask ) );

	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( curPlayerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) {
			tv.prevPlayers[i] = *SV_GameClientNum( i );
		} else {
			Com_Memset( &tv.prevPlayers[i], 0, sizeof( playerState_t ) );
		}
	}
	Com_Memcpy( tv.prevPlayerBitmask, curPlayerBitmask, sizeof( curPlayerBitmask ) );

	if ( tv.recording ) {
		tv.frameCount++;
	}
}


/*
===============
SV_TV_StopRecord
===============
*/
void SV_TV_StopRecord( qboolean discard ) {
	char tmpPath[MAX_QPATH];
	float duration;

	tv.autoPending = qfalse;
	tv.lastRecordedFile[0] = '\0';
	tv.lastRecordedMap[0] = '\0';

	if ( !tv.recording ) {
		return;
	}

	// NB: the live stream session is NOT ended here — it spans the whole map
	// (warmup..rotation) and is torn down by SV_TV_StreamEnd at the map
	// boundary. Stopping a recording (e.g. manual tvstop mid-match) leaves the
	// live feed running.

	Com_sprintf( tmpPath, sizeof( tmpPath ), "%s.tvd.tmp", tv.recordingPath );

	if ( discard ) {
		const char *uuid;

		// Free compressor, close and delete the file without finalizing
		if ( tv.cstream ) {
			ZSTD_freeCStream( tv.cstream );
			tv.cstream = NULL;
		}
		FS_FCloseFile( tv.file );
		FS_HomeRemove( tmpPath );
		Com_Printf( "TV: Recording discarded, file deleted.\n" );

		uuid = Cvar_VariableString( "g_matchUUID" );
		if ( uuid[0] ) {
			SV_TV_LogEvent( va( "DemoDiscarded: %s", uuid ) );
		}
	} else {
		char finalPath[MAX_QPATH];
		int durationMsec;
		int val;
		size_t ret;

		// Flush and end the zstd stream
		if ( tv.cstream ) {
			ZSTD_inBuffer in = { NULL, 0, 0 };
			do {
				ZSTD_outBuffer out = { tv.zstdOutBuf, ZSTD_OUT_BUF_SIZE, 0 };
				ret = ZSTD_compressStream2( tv.cstream, &out, &in, ZSTD_e_end );
				if ( out.pos > 0 ) {
					SV_TV_FileWrite( tv.zstdOutBuf, (int)out.pos, tv.file );
				}
			} while ( ret != 0 );
			ZSTD_freeCStream( tv.cstream );
			tv.cstream = NULL;
		}

		// Write uncompressed k/v trailer:
		//   "TVDt"  magic
		//   repeated: key\0 + valueLen:2 + valueData
		//   \0       terminator (empty key)
		//   size:4   trailer total size (for EOF-4 seeking)
		{
			unsigned int trailerStart = tv.fileOffset;
			unsigned short vlen;

			durationMsec = tv.lastServerTime - tv.firstServerTime;

			SV_TV_FileWrite( "TVDt", 4, tv.file );

			SV_TV_FileWrite( "dur", 4, tv.file );       // key including \0
			vlen = 4;
			SV_TV_FileWrite( &vlen, 2, tv.file );
			SV_TV_FileWrite( &durationMsec, 4, tv.file );

			SV_TV_FileWrite( "", 1, tv.file );           // terminator

			val = (int)( tv.fileOffset - trailerStart + 4 ); // +4 for this field itself
			SV_TV_FileWrite( &val, 4, tv.file );
		}

		FS_FCloseFile( tv.file );

		// Rename .tvd.tmp to .tvd
		Com_sprintf( finalPath, sizeof( finalPath ), "%s.tvd", tv.recordingPath );
		FS_Rename( tmpPath, finalPath );

		Q_strncpyz( tv.lastRecordedFile, finalPath, sizeof( tv.lastRecordedFile ) );
		Q_strncpyz( tv.lastRecordedMap, sv_mapname->string, sizeof( tv.lastRecordedMap ) );

		duration = (float)durationMsec / 1000.0f;

		Com_Printf( "TV: Recording stopped. %i frames (%.1f seconds), %u bytes.\n",
			tv.frameCount, duration, tv.fileOffset );

		{
			const char *uuid = Cvar_VariableString( "g_matchUUID" );
			if ( uuid[0] ) {
				SV_TV_LogEvent( va( "DemoSaved: %s frames=%d duration_ms=%d bytes=%u",
					uuid, tv.frameCount, durationMsec, tv.fileOffset ) );
			}
		}
	}

	tv.recording = qfalse;
	tv.file = 0;
}


/*
===============
SV_TV_FinalizeRecording

Stop an active recording, discarding auto-recordings that never met the
player threshold. Shared boundary for map change, shutdown, and
map_restart (tournament match rotation).
===============
*/
void SV_TV_FinalizeRecording( void ) {
	if ( tv.recording && tv.autoRecording && !tv.keepRecording
		 && sv_tvAutoMinPlayers->integer > 0 ) {
		Com_Printf( "TV: Auto-recording did not meet player threshold, discarding.\n" );
		SV_TV_StopRecord( qtrue );
	} else {
		SV_TV_StopRecord( qfalse );
	}
}


/*
===============
SV_TV_StopRecord_f
===============
*/
void SV_TV_StopRecord_f( void ) {
	if ( !tv.recording ) {
		Com_Printf( "TV: Not recording.\n" );
		return;
	}

	SV_TV_StopRecord( qfalse );
}


/*
===============
SV_TV_ConfigstringChanged
===============
*/
void SV_TV_ConfigstringChanged( int index ) {
	if ( index >= 0 && index < MAX_CONFIGSTRINGS ) {
		tv.csChanged[index] = qtrue;
	}
}


/*
===============
SV_TV_CaptureServerCommand
===============
*/
void SV_TV_CaptureServerCommand( int target, const char *cmd ) {
	int len;

	if ( tv.cmdCount >= MAX_TV_CMDS ) {
		return;
	}

	len = (int)strlen( cmd );
	if ( tv.cmdBufUsed + len > MAX_TV_CMDBUF ) {
		return;
	}

	tv.cmds[tv.cmdCount].target = target;
	tv.cmds[tv.cmdCount].offset = tv.cmdBufUsed;
	tv.cmds[tv.cmdCount].len = len;
	Com_Memcpy( tv.cmdBuf + tv.cmdBufUsed, cmd, len );
	tv.cmdBufUsed += len;
	tv.cmdCount++;
}


/*
===============
SV_TV_AutoStart
===============
*/
void SV_TV_AutoStart( void ) {
	if ( !sv_tvAuto->integer || tv.recording || tv.autoPending ) {
		return;
	}

	tv.autoPending = qtrue;
	Com_Printf( "TV: Auto-record pending, waiting for first client.\n" );
}


/*
===============
SV_TV_StreamBegin

Begin a live stream session for the current map (called at map go-live). The
stream runs through warmup and the match, decoupled from .tvd recording (which
starts later at matchState "active"). No-op if streaming is disabled or a
session is already live (which is the case once recording has started, since
StartRecord keeps the session active).
===============
*/
void SV_TV_StreamBegin( void ) {
	if ( !sv_tvLive->integer || SV_TVStream_IsActive() ) {
		return;
	}

	// Zero the shared delta-encoder state so warmup frames stream from a clean
	// baseline — mirrors what SV_TV_StartRecord does for the file. Recording/
	// file fields are left untouched; recording begins later via autoPending.
	Com_Memset( tv.prevEntities, 0, sizeof( tv.prevEntities ) );
	Com_Memset( tv.prevEntityBitmask, 0, sizeof( tv.prevEntityBitmask ) );
	Com_Memset( tv.prevPlayers, 0, sizeof( tv.prevPlayers ) );
	Com_Memset( tv.prevPlayerBitmask, 0, sizeof( tv.prevPlayerBitmask ) );
	Com_Memset( tv.csChanged, 0, sizeof( tv.csChanged ) );
	tv.cmdCount = 0;
	tv.cmdBufUsed = 0;
#ifdef USE_VOIP
	tv.voipCount = 0;
	tv.voipBufUsed = 0;
#endif

	SV_TVStream_StartStream();
}


/*
===============
SV_TV_StreamEnd

End the current map's live stream session (called at map rotation / shutdown),
even when no .tvd was recorded — so a warmup-only stream is finalized and
consumers get a clean TVLe.
===============
*/
void SV_TV_StreamEnd( void ) {
	if ( SV_TVStream_IsActive() ) {
		SV_TVStream_EndStream();
	}
}
