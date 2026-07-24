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
// cl_tv.c -- TV demo playback

#include "client.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

tvPlayback_t tvPlay;

cvar_t *cl_tvViewpoint;
cvar_t *cl_tvTime;
cvar_t *cl_tvDuration;
cvar_t *cl_tvStreamClosed;
cvar_t *cl_tvLiveEnded;
cvar_t *cl_tvMapSerial;
cvar_t *cl_tvMapName;
cvar_t *cl_tvPendingMap;
cvar_t *cl_tvAssetsReady;

typedef enum { TVSEG_OK, TVSEG_STARVED, TVSEG_ENDED, TVSEG_NEWSTREAM } tvSegResult_t;

static void CL_TV_View_f( void );
static void CL_TV_ViewNext_f( void );
static void CL_TV_ViewPrev_f( void );
static void CL_TV_Seek_f( void );
static qboolean CL_TV_OpenLive( const char *filename );
static tvSegResult_t CL_TV_ReadStreamHeader( char *mapname, int mapnameSize );
static qboolean CL_TV_LiveMapChange( void );
static qboolean CL_TV_MapAvailable( const char *mapname );
#ifdef __EMSCRIPTEN__
static void CL_TV_OpenLive_f( void );
static void CL_TV_Resync_f( void );
#endif


#ifdef __EMSCRIPTEN__
/*
=====================================================================
Live byte-feed ring

The JS loader pushes the live HTTP stream here via CL_TV_FeedBytes; the live
reader (CL_TV_RawRead/Tell/Seek below) pulls from it. This replaces the growing
MEMFS file the live path used to read, so a held-open viewer session spanning
many matches no longer accumulates the whole stream in memory (consume-and-
discard). VOD playback still reads from a real file (the #else seam).

Flow control is backpressure, not eviction: CL_TV_FeedBytes returns the count it
accepted and the loader re-feeds the remainder, so the ring never overruns and
framing is never corrupted. A stalled consumer (e.g. a backgrounded tab) simply
blocks the feed until it drains; the catch-up in CL_TV_ReadLiveSegment then skips
the backlog.
=====================================================================
*/
#define TV_LIVE_RING_SIZE ( 4 * 1024 * 1024 )   // >= a few max segments (TVD_SEGIN_MAX)
static struct {
	byte   data[TV_LIVE_RING_SIZE];
	size_t head;    // total bytes fed (write edge)
	size_t tail;    // oldest retained byte (consumed bytes are reclaimed here)
	size_t cursor;  // tentative read position; tail <= cursor <= head
} tvRing;

// Copy up to len bytes at absolute position p without advancing; returns the
// count copied (< len when p+len runs past the write edge). Wrap-aware.
static int TVRing_PeekAt( size_t p, void *buf, int len ) {
	size_t avail = tvRing.head - p;
	int n = ( (size_t)len < avail ) ? len : (int)avail;
	size_t off = p % TV_LIVE_RING_SIZE;
	size_t first = TV_LIVE_RING_SIZE - off;
	if ( (size_t)n <= first ) {
		Com_Memcpy( buf, tvRing.data + off, n );
	} else {
		Com_Memcpy( buf, tvRing.data + off, first );
		Com_Memcpy( (byte *)buf + first, tvRing.data, (size_t)n - first );
	}
	return n;
}

// For catch-up: the end position of the COMPLETE data segment ("TVLs") starting
// at p, or 0 if p isn't a fully-buffered TVLs record (a TVLe/TVL1 boundary, a
// partial header, or a payload not yet fully fed). Segment wire header is
// magic(4)+keyframeTime(4)+payloadLen(4 LE).
static size_t TVRing_SegmentEnd( size_t p ) {
	byte hdr[12];
	unsigned int payloadLen;
	if ( TVRing_PeekAt( p, hdr, 12 ) < 12 ) {
		return 0;
	}
	if ( Q_strncmp( (char *)hdr, "TVLs", 4 ) ) {
		return 0;
	}
	payloadLen = (unsigned int)( hdr[8] | ( hdr[9] << 8 ) | ( hdr[10] << 16 ) | ( (unsigned int)hdr[11] << 24 ) );
	if ( p + 12 + payloadLen > tvRing.head ) {
		return 0;   // payload not fully buffered yet
	}
	return p + 12 + payloadLen;
}

// keyframeTime (i32 server msec) of the complete TVLs segment at p.
static int TVRing_SegmentKeyframeTime( size_t p ) {
	byte hdr[8];
	TVRing_PeekAt( p, hdr, 8 );   // "TVLs"(4) + keyframeTime(4 LE)
	return (int)( (unsigned int)hdr[4] | ( (unsigned int)hdr[5] << 8 )
		| ( (unsigned int)hdr[6] << 16 ) | ( (unsigned int)hdr[7] << 24 ) );
}

// Browser-side jitter cushion (server msec) the catch-up leaves buffered. The
// BASE of an adaptive range (tvPlay.effKeepMs): the steady-state clock controller
// (CL_TV_AdjustLiveClock) holds the render edge ~effKeepMs behind the newest
// buffered segment, and repeated underruns grow effKeepMs a segment at a time up
// to TV_CATCHUP_KEEP_MAX (relaxing back after a quiet spell). Deliberately
// decoupled from the relay's broadcast delay (which absorbs server-side jitter);
// this only covers client-side network/decode hitches, e.g. iOS radio/timer
// throttling. Must clear the keyframe interval (sv_tvLiveKeyframeMsec): TVRing_CatchUp
// keeps whole segments, so a base below the interval leaves zero slack past the
// playing segment. 2000 = ~2 segments at the 1s keyframe default.
#define TV_CATCHUP_KEEP_MS  2000   // base / floor of the adaptive cushion
#define TV_CATCHUP_KEEP_MAX 5000   // ceiling after repeated underruns
#define TV_KEEP_STEP_MS     1000   // grow/relax the cushion one ~keyframe at a time
#define TV_KEEP_DECAY_MS    30000  // relax one step after this long with no underrun

// One-shot catch-up after a stall: drop whole stale segments so the play cursor
// sits ~TV_CATCHUP_KEEP_MS behind the newest buffered segment, discarding the
// backlog a pk3 download accumulated (each TVL segment is a full keyframe, so the
// jump is lossless). Stops at the first non-TVLs boundary (TVLe/partial), so it
// never crosses a map change. Called one-shot at initial boot and the map-change
// asset-gate resume — NOT per read: segments are coarse and skipping one in steady
// play tears entity interpolation.
// Returns the number of segments skipped.
static int TVRing_CatchUp( int keepMs ) {
	size_t scan = tvRing.cursor, newest = tvRing.cursor;
	int haveNewest = 0, newestKf = 0, skipped = 0;
	for ( ;; ) {
		size_t end = TVRing_SegmentEnd( scan );
		if ( !end ) {
			break;
		}
		newest = scan;
		haveNewest = 1;
		scan = end;
	}
	if ( !haveNewest ) {
		return 0;
	}
	newestKf = TVRing_SegmentKeyframeTime( newest );
	for ( ;; ) {
		size_t end = TVRing_SegmentEnd( tvRing.cursor );
		if ( !end || tvRing.cursor == newest ) {
			break;   // cursor is (or reached) the newest complete segment
		}
		// Only advance if the NEXT segment is still >= keepMs behind newest;
		// otherwise moving there would leave < keepMs buffered, so stop.
		if ( newestKf - TVRing_SegmentKeyframeTime( end ) < keepMs ) {
			break;
		}
		tvRing.cursor = end;
		skipped++;
	}
	tvRing.tail = tvRing.cursor;
	return skipped;
}

// Keyframe serverTime of the newest complete segment buffered ahead of the play
// cursor (0 if none). The live edge the adaptive clock controller measures
// headroom against. Same forward scan as TVRing_CatchUp's first loop.
static int TVRing_NewestKeyframeTime( void ) {
	size_t scan = tvRing.cursor, newest = 0;
	int haveNewest = 0;
	for ( ;; ) {
		size_t end = TVRing_SegmentEnd( scan );
		if ( !end ) {
			break;
		}
		newest = scan;
		haveNewest = 1;
		scan = end;
	}
	return haveNewest ? TVRing_SegmentKeyframeTime( newest ) : 0;
}

/*
===============
CL_TV_FeedBytes

Loader entry: append as much of [ptr,len) as fits past the retained tail; return
the count accepted. The loader re-feeds any remainder on a later tick, which is
the backpressure that bounds the ring.
===============
*/
EMSCRIPTEN_KEEPALIVE
int CL_TV_FeedBytes( char *ptr, int len ) {
	size_t freeSpace = TV_LIVE_RING_SIZE - ( tvRing.head - tvRing.tail );
	int n = ( (size_t)len < freeSpace ) ? len : (int)freeSpace;
	int i = 0;
	while ( i < n ) {
		size_t off = tvRing.head % TV_LIVE_RING_SIZE;
		size_t span = TV_LIVE_RING_SIZE - off;
		size_t chunk = ( (size_t)( n - i ) < span ) ? (size_t)( n - i ) : span;
		Com_Memcpy( tvRing.data + off, ptr + i, chunk );
		tvRing.head += chunk;
		i += (int)chunk;
	}
	return n;
}
#endif


/*
===============
CL_TV_RawRead

Raw (pre-zstd) read of the live byte source: the byte-feed ring on the WASM
build, the open file otherwise (native/VOD). RawTell/RawSeek bracket a segment so
a short read can rewind and retry on a later frame (see CL_TV_ReadLiveSegment).
===============
*/
static int CL_TV_RawRead( void *buf, int len ) {
#ifdef __EMSCRIPTEN__
	int n = TVRing_PeekAt( tvRing.cursor, buf, len );
	tvRing.cursor += n;
	return n;
#else
	return FS_Read( buf, len, tvPlay.file );
#endif
}

static long CL_TV_RawTell( void ) {
#ifdef __EMSCRIPTEN__
	return (long)tvRing.cursor;
#else
	return FS_FTell( tvPlay.file );
#endif
}

static void CL_TV_RawSeek( long pos ) {
#ifdef __EMSCRIPTEN__
	tvRing.cursor = (size_t)pos;
#else
	FS_Seek( tvPlay.file, pos, FS_SEEK_SET );
#endif
}

// Read and discard n bytes from the raw source; returns bytes actually skipped
// (< n on starvation). Used to step over header fields we don't keep.
static int CL_TV_RawSkip( int n ) {
	byte scratch[256];
	int total = 0;
	while ( total < n ) {
		int want = n - total;
		int got;
		if ( want > (int)sizeof( scratch ) ) {
			want = sizeof( scratch );
		}
		got = CL_TV_RawRead( scratch, want );
		total += got;
		if ( got < want ) {
			break;
		}
	}
	return total;
}


/*
===============
CL_TV_WriteCommand

Write a server command into the standard reliable command ring buffer.
===============
*/
static void CL_TV_WriteCommand( const char *cmd ) {
	int index;
	clc.serverCommandSequence++;
	index = clc.serverCommandSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.serverCommands[index], cmd, MAX_STRING_CHARS );
	clc.serverCommandsIgnore[index] = qfalse;
}


/*
===============
CL_TV_SendConfigstring

Notify cgame of a configstring change. A value too large for a single "cs"
command is split into bcs0/1/2 parts (mirrors SV_SendConfigstring;
CL_GetServerCommand reassembles them) so it can't overflow MAX_STRING_CHARS.
===============
*/
static void CL_TV_SendConfigstring( int index, const char *data, int len ) {
	const int maxChunkSize = MAX_STRING_CHARS - 24;
	char cmd[MAX_STRING_CHARS];
	if ( len >= maxChunkSize ) {
		int sent = 0, remaining = len;
		char buf[MAX_STRING_CHARS];
		while ( remaining > 0 ) {
			const char *bcs = ( sent == 0 ) ? "bcs0" : ( remaining < maxChunkSize ? "bcs2" : "bcs1" );
			Q_strncpyz( buf, &data[sent], maxChunkSize );
			Com_sprintf( cmd, sizeof( cmd ), "%s %i \"%s\"", bcs, index, buf );
			CL_TV_WriteCommand( cmd );
			sent += ( maxChunkSize - 1 );
			remaining -= ( maxChunkSize - 1 );
		}
	} else {
		Com_sprintf( cmd, sizeof( cmd ), "cs %i \"%s\"", index, data );
		CL_TV_WriteCommand( cmd );
	}
}

// Read team from configstring rather than persistant[], which is unreliable
// for spectators in follow mode.
static int CL_TV_GetPlayerTeam( int clientNum ) {
	const char *cs = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS + clientNum];
	return atoi( Info_ValueForKey( cs, "t" ) );
}


/*
===============
CL_TV_Init
===============
*/
void CL_TV_Init( void ) {
	cl_tvViewpoint = Cvar_Get( "cl_tvViewpoint", "0", CVAR_ROM );
	cl_tvTime = Cvar_Get( "cl_tvTime", "0", CVAR_ROM );
	cl_tvDuration = Cvar_Get( "cl_tvDuration", "0", CVAR_ROM );
	// cl_tvStreamClosed: set by the JS loader at the fetch edge (stream closed).
	// cl_tvLiveEnded: set by the engine at the playback edge (buffer drained) —
	// ~delay-buffer later, and what the web client reconnects on.
	cl_tvStreamClosed = Cvar_Get( "cl_tvStreamClosed", "0", 0 );
	cl_tvLiveEnded = Cvar_Get( "cl_tvLiveEnded", "0", CVAR_ROM );
	// cl_tvMapSerial: bumped at the END of each in-place live map change (the
	// completion signal the web client polls to refresh the roster).
	cl_tvMapSerial = Cvar_Get( "cl_tvMapSerial", "0", CVAR_ROM );
	// cl_tvMapName: the current/target map. Set at the START of a map change (before
	// the asset gate) so the web client can raise the levelshot overlay covering the
	// download, and on first open. A change here brackets the transition with the
	// later cl_tvMapSerial bump.
	cl_tvMapName = Cvar_Get( "cl_tvMapName", "", CVAR_ROM );
	// cl_tvPendingMap: engine writes the next map's name when its pk3 isn't in the
	// VFS; the JS loader polls it, fetches the pk3, then sets cl_tvAssetsReady=1 so
	// the engine FS_Restarts and completes the in-place map change.
	cl_tvPendingMap = Cvar_Get( "cl_tvPendingMap", "", CVAR_ROM );
	cl_tvAssetsReady = Cvar_Get( "cl_tvAssetsReady", "0", 0 );

#ifdef __EMSCRIPTEN__
	// Browser live entry: the JS loader feeds the stream into the byte-feed ring,
	// then runs this (replacing "+demo live.tvd" — live no longer uses a file).
	Cmd_AddCommand( "tv_openlive", CL_TV_OpenLive_f );
	// Re-arm the one-shot catch-up: the React shell runs this on tab foreground so a
	// throttled-tab backlog snaps back to ~the cushion instead of replaying it.
	Cmd_AddCommand( "tv_resync", CL_TV_Resync_f );
#endif
}


/*
===============
CL_TV_DecompressRead

Read decompressed data from the zstd stream.
Returns number of bytes actually read (< len at stream end).
===============
*/
static int CL_TV_DecompressRead( void *buf, int len ) {
	byte *dst = (byte *)buf;
	int total = 0;

	while ( total < len ) {
		// Consume from decompressed output buffer first
		if ( tvPlay.zstdOutPos < tvPlay.zstdOutSize ) {
			size_t avail = tvPlay.zstdOutSize - tvPlay.zstdOutPos;
			size_t want = (size_t)( len - total );
			size_t copy = ( want < avail ) ? want : avail;
			Com_Memcpy( dst + total, tvPlay.zstdOutBuf + tvPlay.zstdOutPos, copy );
			tvPlay.zstdOutPos += copy;
			total += (int)copy;
			continue;
		}

		if ( tvPlay.zstdStreamEnded ) {
			break;
		}

		// Read more compressed data from file if input buffer exhausted
		if ( tvPlay.zstdInPos >= tvPlay.zstdInSize ) {
			int bytesRead = FS_Read( tvPlay.zstdInBuf, TVD_ZSTD_IN_BUF_SIZE, tvPlay.file );
			if ( bytesRead <= 0 ) {
				tvPlay.zstdStreamEnded = qtrue;
				break;
			}
			tvPlay.zstdInSize = (size_t)bytesRead;
			tvPlay.zstdInPos = 0;
		}

		// Decompress
		{
			ZSTD_inBuffer in = { tvPlay.zstdInBuf, tvPlay.zstdInSize, tvPlay.zstdInPos };
			ZSTD_outBuffer out = { tvPlay.zstdOutBuf, TVD_ZSTD_OUT_BUF_SIZE, 0 };
			size_t ret = ZSTD_decompressStream( tvPlay.dstream, &out, &in );
			tvPlay.zstdInPos = in.pos;
			tvPlay.zstdOutSize = out.pos;
			tvPlay.zstdOutPos = 0;

			if ( ret == 0 ) {
				// Zstd frame complete
				tvPlay.zstdStreamEnded = qtrue;
			}
			if ( ZSTD_isError( ret ) ) {
				tvPlay.zstdStreamEnded = qtrue;
			}
		}
	}

	return total;
}


/*
===============
CL_TV_ReadTrailer

Read k/v trailer from the end of the file.
Format: "TVDt" + repeated(key\0 + valueLen:2 + valueData) + \0 + trailerSize:4
Returns qtrue on success, qfalse on failure (sets totalDuration=0).
===============
*/
static qboolean CL_TV_ReadTrailer( void ) {
	long savedPos;
	int trailerSize;
	char magic[4];
	long fileLen;
	char key[64];
	unsigned short vlen;
	byte vbuf[256];

	tvPlay.totalDuration = 0;

	savedPos = FS_FTell( tvPlay.file );

	// Get file length by seeking to end
	FS_Seek( tvPlay.file, 0, FS_SEEK_END );
	fileLen = FS_FTell( tvPlay.file );

	// Minimum trailer: "TVDt"(4) + \0(1) + size(4) = 9
	if ( fileLen < 9 ) {
		FS_Seek( tvPlay.file, savedPos, FS_SEEK_SET );
		return qfalse;
	}

	// Read trailerSize from EOF-4
	FS_Seek( tvPlay.file, fileLen - 4, FS_SEEK_SET );
	if ( FS_Read( &trailerSize, 4, tvPlay.file ) != 4 || trailerSize < 9 || trailerSize > fileLen ) {
		FS_Seek( tvPlay.file, savedPos, FS_SEEK_SET );
		return qfalse;
	}

	// Seek to trailer start
	FS_Seek( tvPlay.file, fileLen - trailerSize, FS_SEEK_SET );

	// Read and validate magic "TVDt"
	if ( FS_Read( magic, 4, tvPlay.file ) != 4 ||
		 magic[0] != 'T' || magic[1] != 'V' || magic[2] != 'D' || magic[3] != 't' ) {
		FS_Seek( tvPlay.file, savedPos, FS_SEEK_SET );
		return qfalse;
	}

	// Read k/v pairs until empty key (terminator)
	while ( 1 ) {
		int i;

		// Read key (null-terminated)
		for ( i = 0; i < (int)sizeof( key ) - 1; i++ ) {
			if ( FS_Read( &key[i], 1, tvPlay.file ) != 1 ) {
				FS_Seek( tvPlay.file, savedPos, FS_SEEK_SET );
				return qfalse;
			}
			if ( key[i] == '\0' ) {
				break;
			}
		}
		key[sizeof( key ) - 1] = '\0';

		// Empty key = terminator
		if ( key[0] == '\0' ) {
			break;
		}

		// Read value length
		if ( FS_Read( &vlen, 2, tvPlay.file ) != 2 ) {
			FS_Seek( tvPlay.file, savedPos, FS_SEEK_SET );
			return qfalse;
		}

		// Read value data
		if ( vlen > sizeof( vbuf ) ) {
			// Skip unknown large value
			FS_Seek( tvPlay.file, vlen, FS_SEEK_CUR );
			continue;
		}
		if ( vlen > 0 && FS_Read( vbuf, vlen, tvPlay.file ) != vlen ) {
			FS_Seek( tvPlay.file, savedPos, FS_SEEK_SET );
			return qfalse;
		}

		// Match known keys
		if ( !Q_stricmp( key, "dur" ) && vlen == 4 ) {
			Com_Memcpy( &tvPlay.totalDuration, vbuf, 4 );
		}
	}

	// Restore file position
	FS_Seek( tvPlay.file, savedPos, FS_SEEK_SET );
	return qtrue;
}


/*
===============
CL_TV_ReadString

Read a null-terminated string from file, byte at a time.
===============
*/
static int CL_TV_ReadString( char *buf, int bufSize ) {
	int i;
	for ( i = 0; i < bufSize - 1; i++ ) {
		if ( FS_Read( &buf[i], 1, tvPlay.file ) != 1 ) {
			buf[i] = '\0';
			return -1;
		}
		if ( buf[i] == '\0' ) {
			return i;
		}
	}
	buf[bufSize - 1] = '\0';
	return i;
}


/*
===============
CL_TV_FindFirstActivePlayer

Return the first active player clientNum from the player bitmask.
Returns -1 if none found.
===============
*/
static int CL_TV_FindFirstActivePlayer( void ) {
	int i;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( ( tvPlay.playerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) )
				&& CL_TV_GetPlayerTeam( i ) != TEAM_SPECTATOR ) {
			return i;
		}
	}
	return -1;
}


/*
===============
CL_TV_UpdateConfigstring

Apply a configstring change to cl.gameState, rebuilding the string table.
===============
*/
static void CL_TV_UpdateConfigstring( int index, const char *data, int dataLen ) {
	gameState_t oldGs;
	int i, len;
	const char *dup;
	char modifiedInfo[BIG_INFO_STRING];

	// Ensure \tv\1 is always present in CS_SERVERINFO
	if ( index == CS_SERVERINFO && dataLen > 0 ) {
		Q_strncpyz( modifiedInfo, data, sizeof( modifiedInfo ) );
		Info_SetValueForKey( modifiedInfo, "tv", "1" );
		data = modifiedInfo;
		dataLen = (int)strlen( modifiedInfo );
	}

	oldGs = cl.gameState;
	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
	cl.gameState.dataCount = 1;

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( i == index ) {
			// use new data (may be empty string if dataLen == 0)
			if ( dataLen <= 0 ) {
				continue;
			}
			if ( dataLen + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
				Com_Error( ERR_DROP, "CL_TV_UpdateConfigstring: MAX_GAMESTATE_CHARS exceeded" );
			}
			cl.gameState.stringOffsets[i] = cl.gameState.dataCount;
			Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, data, dataLen );
			cl.gameState.stringData[cl.gameState.dataCount + dataLen] = '\0';
			cl.gameState.dataCount += dataLen + 1;
		} else {
			dup = oldGs.stringData + oldGs.stringOffsets[i];
			if ( !dup[0] ) {
				continue;
			}
			len = (int)strlen( dup );
			if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
				Com_Error( ERR_DROP, "CL_TV_UpdateConfigstring: MAX_GAMESTATE_CHARS exceeded" );
			}
			cl.gameState.stringOffsets[i] = cl.gameState.dataCount;
			Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
			cl.gameState.dataCount += len + 1;
		}
	}
}


/*
===============
CL_TV_ReadStreamHeader

Parse a TVL1 session header, assuming the raw source is positioned just past the
4-byte "TVL1" magic. Reads svFps/maxclients into tvPlay and the map name into the
caller's buffer; consumes (but discards) the timestamp + fs_game fields to stay
aligned with the first segment (fs_game is fixed per stream — see D5). Used by
both CL_TV_OpenLive (first session) and CL_TV_LiveMapChange (later sessions).

Returns TVSEG_STARVED if the header isn't fully buffered yet (a mid-stream header
can span feed chunks — the caller rewinds and retries next frame), TVSEG_ENDED on
an unsupported/oversized header, TVSEG_OK on success. Never closes the stream —
the caller decides cleanup.
===============
*/
static tvSegResult_t CL_TV_ReadStreamHeader( char *mapname, int mapnameSize ) {
	int version;
	unsigned short maplen, tslen, gamelen;

	if ( CL_TV_RawRead( &version, 4 ) != 4 ) {
		return TVSEG_STARVED;
	}
	if ( version != 1 ) {
		Com_Printf( S_COLOR_YELLOW "TV: unsupported TVL version %i\n", version );
		return TVSEG_ENDED;
	}

	if ( CL_TV_RawRead( &tvPlay.svFps, 4 ) != 4 ||
		 CL_TV_RawRead( &tvPlay.maxclients, 4 ) != 4 ) {
		return TVSEG_STARVED;
	}

	if ( CL_TV_RawRead( &maplen, 2 ) != 2 ) {
		return TVSEG_STARVED;
	}
	if ( maplen >= mapnameSize ) {
		Com_Printf( S_COLOR_YELLOW "TV: live mapName too long (%i)\n", maplen );
		return TVSEG_ENDED;
	}
	if ( maplen && CL_TV_RawRead( mapname, maplen ) != maplen ) {
		return TVSEG_STARVED;
	}
	mapname[maplen] = '\0';

	if ( CL_TV_RawRead( &tslen, 2 ) != 2 ) {
		return TVSEG_STARVED;
	}
	if ( tslen && CL_TV_RawSkip( tslen ) != tslen ) {  // skip the timestamp field exactly
		return TVSEG_STARVED;
	}

	if ( CL_TV_RawRead( &gamelen, 2 ) != 2 ) {
		return TVSEG_STARVED;
	}
	if ( gamelen && CL_TV_RawSkip( gamelen ) != gamelen ) {  // skip the fs_game field exactly
		return TVSEG_STARVED;
	}

	return TVSEG_OK;
}


/*
===============
CL_TV_OpenLive

Open a TVL1 live stream. Assumes tvPlay.file is positioned just past the
4-byte "TVL1" magic. Parses the header, then applies the first keyframe so
configstrings precede the cgame init that follows.
===============
*/
static qboolean CL_TV_OpenLive( const char *filename ) {
	char mapname[MAX_QPATH];

	(void)filename;

	if ( CL_TV_ReadStreamHeader( mapname, sizeof( mapname ) ) != TVSEG_OK ) {
		CL_TV_Close();
		return qfalse;
	}

	tvPlay.live = qtrue;
	tvPlay.bootstrapped = qfalse;
	tvPlay.atEnd = qfalse;
	tvPlay.segOutLen = 0;
	tvPlay.segCursor = 0;
	tvPlay.awaitingAssets = qfalse;
	tvPlay.awaitingHeader = qfalse;
#ifdef __EMSCRIPTEN__
	tvPlay.effKeepMs = TV_CATCHUP_KEEP_MS;   // adaptive cushion starts at the base
	tvPlay.lastKeepAdapt = cls.realtime;
	tvPlay.starved = qfalse;
	tvPlay.clockSkewAccum = 0.0f;
#endif
	Cvar_Set( "cl_tvMapName", mapname );
	Cvar_Set( "cl_tvStreamClosed", "0" );
	Cvar_Set( "cl_tvLiveEnded", "0" );
	Cvar_Set( "cl_tvPendingMap", "" );
	Cvar_Set( "cl_tvAssetsReady", "0" );

	// Init zstd decompressor
	tvPlay.dstream = ZSTD_createDStream();
	ZSTD_initDStream( tvPlay.dstream );

	// Zero entity/player buffers
	Com_Memset( tvPlay.entities, 0, sizeof( tvPlay.entities ) );
	Com_Memset( tvPlay.entityBitmask, 0, sizeof( tvPlay.entityBitmask ) );
	Com_Memset( tvPlay.players, 0, sizeof( tvPlay.players ) );
	Com_Memset( tvPlay.playerBitmask, 0, sizeof( tvPlay.playerBitmask ) );

	// Initialize standard ring buffer state
	cl.parseEntitiesNum = 0;
	clc.serverMessageSequence = 0;
	clc.serverCommandSequence = 0;
	clc.lastExecutedServerCommand = 0;

	// Set initial clientNum
	clc.clientNum = 0;

	// Apply the first keyframe NOW (mirrors VOD's CL_TV_Open) so cl.gameState
	// holds the configstrings before CL_InitCGame runs right after CL_TV_Open
	// returns. Without them the cgame init fails "Client/Server game mismatch".
	// The loader guarantees the first complete segment is in the initial file
	// write, so this read is satisfied synchronously.
	if ( !CL_TV_NextLiveFrame() ) {
		Com_Printf( S_COLOR_YELLOW "TV: live stream missing initial keyframe\n" );
		CL_TV_Close();
		return qfalse;
	}
	tvPlay.viewpoint = CL_TV_FindFirstActivePlayer();
	if ( tvPlay.viewpoint < 0 ) {
		tvPlay.viewpoint = 0;
	}
	clc.clientNum = tvPlay.viewpoint;
	VectorCopy( tvPlay.players[tvPlay.viewpoint].origin, tvPlay.viewOrigin );

	// Mark the bootstrap "cs" dump executed (CG_INIT reads the populated
	// gameState itself; replaying it post-init re-fires change handlers).
	// Before BuildSnapshot, so its injected scores command still executes.
	clc.lastExecutedServerCommand = clc.serverCommandSequence;
	CL_TV_BuildSnapshot();

#ifdef __EMSCRIPTEN__
	// Trim the boot-time backlog once steady play begins, else the lag stays
	// anchored at the click-time entry instead of the relay edge. See NextLiveFrame.
	tvPlay.needInitialCatchUp = qtrue;
#endif

	tvPlay.active = qtrue;

	// tv_seek intentionally NOT registered in live mode — can't seek a delayed-edge stream.
	Cmd_AddCommand( "tv_view", CL_TV_View_f );
	Cmd_AddCommand( "tv_view_next", CL_TV_ViewNext_f );
	Cmd_AddCommand( "tv_view_prev", CL_TV_ViewPrev_f );

	Cvar_SetIntegerValue( "cl_tvViewpoint", 0 );
	Cvar_SetIntegerValue( "cl_tvTime", 0 );
	Cvar_SetIntegerValue( "cl_tvDuration", 0 );

	Com_Printf( "TV: live stream open (map %s, svFps %i)\n", mapname, tvPlay.svFps );
	return qtrue;
}


#ifdef __EMSCRIPTEN__
/*
===============
CL_TV_OpenLive_f

Browser live entry (the "tv_openlive" command). The JS loader has already fed the
stream's first bytes (magic + header + first segment) into the byte-feed ring,
then runs this — replacing the old "+demo live.tvd" (live no longer uses a MEMFS
file). Mirrors CL_PlayDemo_f's .tvd prologue, then opens the live stream off the
ring.
===============
*/
static void CL_TV_OpenLive_f( void ) {
	char magic[4];

	Cvar_Set( "sv_killserver", "2" );
	CL_Disconnect( qtrue );
	clc.demoplaying = qtrue;
	Con_Close();

	Com_Memset( &tvPlay, 0, sizeof( tvPlay ) );
	// Consume + verify the TVL1 magic from the ring; CL_TV_OpenLive picks up just
	// past it, exactly as the file path leaves CL_TV_Open positioned.
	if ( CL_TV_RawRead( magic, 4 ) != 4 || Q_strncmp( magic, "TVL1", 4 ) ) {
		Com_Printf( S_COLOR_YELLOW "TV: live feed missing TVL1 magic\n" );
		clc.demoplaying = qfalse;
		return;
	}
	if ( !CL_TV_OpenLive( NULL ) ) {
		Com_Printf( S_COLOR_YELLOW "TV: couldn't open live feed\n" );
		clc.demoplaying = qfalse;
		return;
	}

	clc.lastPacketTime = cls.realtime;
	Q_strncpyz( clc.demoName, "live", sizeof( clc.demoName ) );
	Q_strncpyz( cls.servername, "live", sizeof( cls.servername ) );
	cls.state = CA_CONNECTED;
	clc.firstDemoFrameSkipped = qfalse;
	CL_InitDownloads();
}

// Re-arm the one-shot backlog catch-up. The React shell runs `tv_resync` on tab
// foreground: a throttled tab accumulates a ring backlog, and this trims it back
// to ~the cushion (and snaps the clock) on the next frame rather than grinding
// through it. Inert unless a live stream is playing and not mid map-change.
static void CL_TV_Resync_f( void ) {
	if ( !tvPlay.active || !tvPlay.live ) {
		return;
	}
	if ( tvPlay.awaitingAssets || tvPlay.awaitingHeader ) {
		return;   // a map change owns the clock right now; don't disturb it
	}
	tvPlay.needInitialCatchUp = qtrue;
}
#endif


/*
===============
CL_TV_Open
===============
*/
qboolean CL_TV_Open( const char *filename ) {
	char magic[4];
	int protocol;
	char mapname[MAX_QPATH];
	char timestamp[64];
	char csData[BIG_INFO_STRING];
	unsigned short csIdx, csLen;

	Com_Memset( &tvPlay, 0, sizeof( tvPlay ) );

	if ( FS_FOpenFileRead( filename, &tvPlay.file, qtrue ) == -1 ) {
		return qfalse;
	}

	// Read magic
	if ( FS_Read( magic, 4, tvPlay.file ) != 4 ) {
		Com_Printf( S_COLOR_YELLOW "TV: Invalid magic\n" );
		FS_FCloseFile( tvPlay.file );
		return qfalse;
	}

	// Route by magic: TVL1 is a live stream, handled separately
	if ( !Q_strncmp( magic, "TVL1", 4 ) ) {
		return CL_TV_OpenLive( filename );
	}

	// Validate magic (TVD1 VOD)
	if ( magic[0] != 'T' || magic[1] != 'V' || magic[2] != 'D' || magic[3] != '1' ) {
		Com_Printf( S_COLOR_YELLOW "TV: Invalid magic\n" );
		FS_FCloseFile( tvPlay.file );
		return qfalse;
	}

	// Protocol version
	if ( FS_Read( &protocol, 4, tvPlay.file ) != 4 || protocol != 1 ) {
		Com_Printf( S_COLOR_YELLOW "TV: Unsupported protocol %i\n", protocol );
		FS_FCloseFile( tvPlay.file );
		return qfalse;
	}

	// sv_fps
	if ( FS_Read( &tvPlay.svFps, 4, tvPlay.file ) != 4 ) {
		FS_FCloseFile( tvPlay.file );
		return qfalse;
	}

	// maxclients
	if ( FS_Read( &tvPlay.maxclients, 4, tvPlay.file ) != 4 ) {
		FS_FCloseFile( tvPlay.file );
		return qfalse;
	}

	// Map name
	if ( CL_TV_ReadString( mapname, sizeof( mapname ) ) < 0 ) {
		FS_FCloseFile( tvPlay.file );
		return qfalse;
	}

	// Timestamp
	if ( CL_TV_ReadString( timestamp, sizeof( timestamp ) ) < 0 ) {
		FS_FCloseFile( tvPlay.file );
		return qfalse;
	}

	// Populate cl.gameState with configstrings
	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
	cl.gameState.dataCount = 1;

	while ( 1 ) {
		if ( FS_Read( &csIdx, 2, tvPlay.file ) != 2 ) {
			FS_FCloseFile( tvPlay.file );
			return qfalse;
		}

		if ( csIdx == 0xFFFF ) {
			break;  // terminator
		}

		if ( FS_Read( &csLen, 2, tvPlay.file ) != 2 ) {
			FS_FCloseFile( tvPlay.file );
			return qfalse;
		}

		if ( csLen >= sizeof( csData ) ) {
			Com_Printf( S_COLOR_YELLOW "TV: Configstring %i too long (%i)\n", csIdx, csLen );
			FS_FCloseFile( tvPlay.file );
			return qfalse;
		}

		if ( csLen > 0 ) {
			if ( FS_Read( csData, csLen, tvPlay.file ) != csLen ) {
				FS_FCloseFile( tvPlay.file );
				return qfalse;
			}
		}
		csData[csLen] = '\0';

		if ( (unsigned)csIdx >= MAX_CONFIGSTRINGS ) {
			continue;
		}

		// Store in gameState
		if ( csLen + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Printf( S_COLOR_YELLOW "TV: MAX_GAMESTATE_CHARS exceeded loading configstrings\n" );
			FS_FCloseFile( tvPlay.file );
			return qfalse;
		}

		cl.gameState.stringOffsets[csIdx] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, csData, csLen + 1 );
		cl.gameState.dataCount += csLen + 1;
	}

	// Inject \tv\1 into CS_SERVERINFO (CL_TV_UpdateConfigstring auto-injects for CS_SERVERINFO)
	{
		const char *si = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
		CL_TV_UpdateConfigstring( CS_SERVERINFO, si, (int)strlen( si ) );
#ifdef USE_VOIP
		clc.svVoipVersion = atoi( Info_ValueForKey( si, "sv_voipVersion" ) );
#endif
	}

	// Read trailer for duration (before saving frame offset)
	CL_TV_ReadTrailer();

	// Print header info
	{
		if ( tvPlay.totalDuration > 0 ) {
			int secs = tvPlay.totalDuration / 1000;
			int h = secs / 3600;
			int m = ( secs % 3600 ) / 60;
			int s = secs % 60;
			Com_Printf( "TV: %s recorded %s, %i fps, %i maxclients, %02i:%02i:%02i\n",
				mapname, timestamp, tvPlay.svFps, tvPlay.maxclients, h, m, s );
		} else {
			Com_Printf( "TV: %s recorded %s, %i fps, %i maxclients, unknown duration\n",
				mapname, timestamp, tvPlay.svFps, tvPlay.maxclients );
		}
	}

	// Save initial gameState and first frame offset for seeking
	tvPlay.initialGameState = cl.gameState;
	tvPlay.firstFrameOffset = FS_FTell( tvPlay.file );

	// Init zstd decompressor
	tvPlay.dstream = ZSTD_createDStream();
	ZSTD_initDStream( tvPlay.dstream );
	tvPlay.zstdInSize = 0;
	tvPlay.zstdInPos = 0;
	tvPlay.zstdOutSize = 0;
	tvPlay.zstdOutPos = 0;
	tvPlay.zstdStreamEnded = qfalse;

	// Zero entity/player buffers
	Com_Memset( tvPlay.entities, 0, sizeof( tvPlay.entities ) );
	Com_Memset( tvPlay.entityBitmask, 0, sizeof( tvPlay.entityBitmask ) );
	Com_Memset( tvPlay.players, 0, sizeof( tvPlay.players ) );
	Com_Memset( tvPlay.playerBitmask, 0, sizeof( tvPlay.playerBitmask ) );

	// Initialize standard ring buffer state
	cl.parseEntitiesNum = 0;
	clc.serverMessageSequence = 0;
	clc.serverCommandSequence = 0;
	clc.lastExecutedServerCommand = 0;

	// Set initial clientNum
	clc.clientNum = 0;

	// Read first frame
	CL_TV_ReadFrame();
	if ( tvPlay.atEnd ) {
		Com_Printf( S_COLOR_YELLOW "TV: No frames in file\n" );
		FS_FCloseFile( tvPlay.file );
		return qfalse;
	}

	tvPlay.firstServerTime = tvPlay.serverTime;

	// Set viewpoint to first active player
	tvPlay.viewpoint = CL_TV_FindFirstActivePlayer();
	if ( tvPlay.viewpoint < 0 ) {
		tvPlay.viewpoint = 0;
	}
	clc.clientNum = tvPlay.viewpoint;
	VectorCopy( tvPlay.players[tvPlay.viewpoint].origin, tvPlay.viewOrigin );

	// Build first snapshot into standard ring buffer
	CL_TV_BuildSnapshot();

	// Read second frame — data stays in tvPlay for post-init snapshot build
	CL_TV_ReadFrame();

	tvPlay.active = qtrue;

#if defined(USE_VOIP) && !defined(DEDICATED)
	// Initialize VOIP codecs for TVD playback
	CL_InitVoip();
#endif

	// Set up command sequence for cgame init
	clc.lastExecutedServerCommand = clc.serverCommandSequence;

	// Register commands
	Cmd_AddCommand( "tv_view", CL_TV_View_f );
	Cmd_AddCommand( "tv_view_next", CL_TV_ViewNext_f );
	Cmd_AddCommand( "tv_view_prev", CL_TV_ViewPrev_f );
	Cmd_AddCommand( "tv_seek", CL_TV_Seek_f );

	// Set duration cvar from header (written by server on recording close)
	Cvar_SetIntegerValue( "cl_tvDuration", tvPlay.totalDuration );
	Cvar_SetIntegerValue( "cl_tvTime", 0 );
	Cvar_SetIntegerValue( "cl_tvViewpoint", tvPlay.viewpoint );

	return qtrue;
}


/*
===============
CL_TV_Close
===============
*/
void CL_TV_Close( void ) {
	if ( tvPlay.dstream ) {
		ZSTD_freeDStream( tvPlay.dstream );
		tvPlay.dstream = NULL;
	}

	if ( tvPlay.file ) {
		FS_FCloseFile( tvPlay.file );
	}

#if defined(USE_VOIP) && !defined(DEDICATED)
	CL_ShutdownVoip();
#endif

	Cmd_RemoveCommand( "tv_view" );
	Cmd_RemoveCommand( "tv_view_next" );
	Cmd_RemoveCommand( "tv_view_prev" );
	Cmd_RemoveCommand( "tv_seek" );

	Com_Memset( &tvPlay, 0, sizeof( tvPlay ) );
}


/*
===============
CL_TV_ParseFrame

Parse one frame from an in-memory buffer. Shared by VOD playback
(CL_TV_ReadFrame) and the live-streaming path.

isKeyframe (live only): a segment-leading keyframe lists ALL non-empty
configstrings, so it is authoritative — clear any we still hold that it omits.
Else a configstring cleared on a keyframe-coincident frame (its delta is dropped
for the keyframe) stays stale, e.g. CS_WARMUP replays the fight countdown on the
next re-init. VOD has no keyframes and passes qfalse.
===============
*/
static void CL_TV_ParseFrame( byte *data, int len, qboolean isKeyframe ) {
	msg_t msg;
	int serverTime;
	int num;
	entityState_t tempEntity;
	playerState_t tempPlayer;
	byte oldEntityBitmask[MAX_GENTITIES/8];
	byte oldPlayerBitmask[MAX_CLIENTS/8];
	byte csSeen[(MAX_CONFIGSTRINGS + 7) / 8]; // keyframe: indices the keyframe asserted
	int csCount, cmdCount;
	int i;
	char csData[BIG_INFO_STRING];

	if ( isKeyframe ) {
		Com_Memset( csSeen, 0, sizeof( csSeen ) );
	}

	// Set up message for reading
	MSG_Init( &msg, data, len );
	msg.cursize = len;
	MSG_BeginReading( &msg );

	// Server time
	serverTime = MSG_ReadLong( &msg );

	// --- Entity section ---

	// Save old bitmask for cleanup
	Com_Memcpy( oldEntityBitmask, tvPlay.entityBitmask, sizeof( oldEntityBitmask ) );

	// Read new entity bitmask
	MSG_ReadData( &msg, tvPlay.entityBitmask, MAX_GENTITIES / 8 );

	// Read delta-encoded entities from the bitstream
	while ( 1 ) {
		num = MSG_ReadEntitynum( &msg );
		if ( num == MAX_GENTITIES - 1 ) {
			break;  // end marker
		}

		if ( num < 0 ) {
			// MSG_ReadEntitynum returns -1 when the message buffer is
			// exhausted.  This is normal at the end of a demo file where
			// the final frame may be truncated.
			tvPlay.atEnd = qtrue;
			return;
		}

		if ( num >= MAX_GENTITIES - 1 ) {
			Com_Printf( S_COLOR_YELLOW "TV: Bad entity number %i\n", num );
			tvPlay.atEnd = qtrue;
			return;
		}

		// Delta frame: read into temp, then copy back
		MSG_ReadDeltaEntity( &msg, &tvPlay.entities[num], &tempEntity, num );
		if ( tempEntity.number == MAX_GENTITIES - 1 ) {
			// Entity removed
			Com_Memset( &tvPlay.entities[num], 0, sizeof( entityState_t ) );
		} else {
			tvPlay.entities[num] = tempEntity;
		}
	}

	// Zero entities that left the bitmask to match the writer's baseline.
	// The writer zeroes prevEntities for removed entities (sv_tv.c),
	// so our running state must also be zeroed for correct delta decoding.
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		if ( ( oldEntityBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) &&
			 !( tvPlay.entityBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			Com_Memset( &tvPlay.entities[i], 0, sizeof( entityState_t ) );
		}
	}

	// --- Player section ---
	Com_Memcpy( oldPlayerBitmask, tvPlay.playerBitmask, sizeof( oldPlayerBitmask ) );
	MSG_ReadData( &msg, tvPlay.playerBitmask, MAX_CLIENTS / 8 );

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		int clientNum;

		if ( !( tvPlay.playerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			continue;
		}

		clientNum = MSG_ReadByte( &msg );
		if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
			Com_Printf( S_COLOR_YELLOW "TV: Bad player clientNum %i\n", clientNum );
			tvPlay.atEnd = qtrue;
			return;
		}

		MSG_ReadDeltaPlayerstate( &msg, &tvPlay.players[clientNum], &tempPlayer );
		tvPlay.players[clientNum] = tempPlayer;
	}

	// Zero players that left the bitmask to match the writer's baseline.
	// The writer zeroes prevPlayers for removed players (sv_tv.c),
	// so our running state must also be zeroed for correct delta decoding.
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( ( oldPlayerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) &&
			 !( tvPlay.playerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			Com_Memset( &tvPlay.players[i], 0, sizeof( playerState_t ) );
		}
	}

	// Auto-switch viewpoint if current player disconnected or became spectator
	// Skip during seek: early replay frames may not yet contain the followed player
	if ( !tvPlay.seeking &&
		 ( !( tvPlay.playerBitmask[tvPlay.viewpoint >> 3] & ( 1 << ( tvPlay.viewpoint & 7 ) ) ) ||
		   CL_TV_GetPlayerTeam( tvPlay.viewpoint ) == TEAM_SPECTATOR ) ) {
		int newVp = CL_TV_FindFirstActivePlayer();
		if ( newVp >= 0 ) {
			tvPlay.viewpoint = newVp;
			clc.clientNum = newVp;
			Cvar_SetIntegerValue( "cl_tvViewpoint", newVp );
		}
	}

	// --- Configstring changes ---
	csCount = MSG_ReadShort( &msg );
	for ( i = 0; i < csCount; i++ ) {
		int csIndex = MSG_ReadShort( &msg );
		int csLen = MSG_ReadShort( &msg );

		if ( csLen > 0 && csLen < (int)sizeof( csData ) ) {
			MSG_ReadData( &msg, csData, csLen );
			csData[csLen] = '\0';
		} else {
			// Out-of-range length: still consume the declared bytes (when
			// positive) so the read cursor stays aligned for the rest of the
			// frame instead of desyncing the parse on the next field.
			int skip;
			for ( skip = 0; skip < csLen; skip++ ) MSG_ReadByte( &msg );
			csData[0] = '\0';
		}

		if ( (unsigned)csIndex < MAX_CONFIGSTRINGS ) {
			// Mark present before the dedup continue (an unchanged value is still asserted).
			if ( isKeyframe ) {
				csSeen[csIndex >> 3] |= 1 << ( csIndex & 7 );
			}

			// Dedup: a keyframe re-asserts every non-empty configstring each time,
			// but we already hold them. Skip unchanged ones so we neither rebuild
			// gameState nor flood the 64-slot reliable-command ring with redundant
			// "cs" notifications. This mirrors a real client: the bulk set arrives
			// once (svc_gamestate / first keyframe), and "cs" commands fire only on
			// an actual change. A genuine change (incl. a clear) still falls through.
			const char *cur = cl.gameState.stringData + cl.gameState.stringOffsets[csIndex];
			if ( !strcmp( cur, csData ) ) {
				continue;
			}

			CL_TV_UpdateConfigstring( csIndex, csData, csLen );

			// Notify cgame so it registers new models/sounds/etc. Skip during
			// seek and post-catch-up reconcile (tv_seek_sync bulk re-syncs).
			if ( !tvPlay.seeking && !tvPlay.reconcileSilent ) {
				CL_TV_SendConfigstring( csIndex, csData, csLen );
			}
		}
	}

	// Keyframe authoritative: clear configstrings it omitted (see header note).
	if ( isKeyframe ) {
		for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
			if ( csSeen[i >> 3] & ( 1 << ( i & 7 ) ) ) {
				continue; // asserted by this keyframe
			}
			if ( cl.gameState.stringOffsets[i] == 0 ) {
				continue; // already empty
			}
			CL_TV_UpdateConfigstring( i, "", 0 );
			if ( !tvPlay.seeking && !tvPlay.reconcileSilent ) {
				CL_TV_SendConfigstring( i, "", 0 );
			}
		}
	}

	// --- Server commands ---
	cmdCount = MSG_ReadShort( &msg );
	for ( i = 0; i < cmdCount; i++ ) {
		int target = MSG_ReadByte( &msg );
		int cmdLen = MSG_ReadShort( &msg );

		if ( cmdLen > 0 && cmdLen < (int)sizeof( csData ) ) {
			MSG_ReadData( &msg, csData, cmdLen );
			csData[cmdLen] = '\0';
		} else {
			// Out-of-range length: consume the declared bytes (when positive) to
			// keep the read cursor aligned before discarding.
			int skip;
			for ( skip = 0; skip < cmdLen; skip++ ) MSG_ReadByte( &msg );
			csData[0] = '\0';
			cmdLen = 0;
		}

		// Queue if broadcast (255) or targeted at our viewpoint
		// Skip during seek to avoid overflowing the 64-command buffer
		if ( !tvPlay.seeking && ( target == 255 || target == tvPlay.viewpoint ) ) {
			CL_TV_WriteCommand( csData );
		}
	}

#if defined(USE_VOIP) && !defined(DEDICATED)
	// --- VOIP packets (optional section, absent in older TVD files) ---
	if ( msg.readcount < msg.cursize ) {
		int voipCount = MSG_ReadShort( &msg );
		for ( i = 0; i < voipCount; i++ ) {
			int sender = MSG_ReadByte( &msg );
			int generation = MSG_ReadByte( &msg );
			int sequence = MSG_ReadLong( &msg );
			int frames = MSG_ReadByte( &msg );
			int flags = MSG_ReadByte( &msg );
			uint8_t recips[(MAX_CLIENTS + 7) / 8];
			int voipLen;
			byte voipData[4000];

			MSG_ReadData( &msg, recips, sizeof( recips ) );
			voipLen = MSG_ReadShort( &msg );

			if ( voipLen > 0 && voipLen <= (int)sizeof( voipData ) ) {
				MSG_ReadData( &msg, voipData, voipLen );
			} else {
				// skip invalid packet
				if ( voipLen > 0 ) {
					msg.readcount += voipLen;
				}
				continue;
			}

			// Skip during seek to avoid audio artifacts
			if ( tvPlay.seeking )
				continue;

			// Viewpoint-aware filtering:
			// - Always play sender's own voice when spectating them
			// - Play spatial audio (proximity-based, handled by audio system)
			// - Play direct/targeted audio only if viewpoint is a recipient
			if ( sender != tvPlay.viewpoint
				&& !( flags & VOIP_SPATIAL )
				&& !Com_IsVoipTarget( recips, sizeof( recips ), tvPlay.viewpoint ) ) {
				continue;
			}

			// Build a synthetic svc_voipOpus message and parse it
			{
				msg_t voipMsg;
				byte voipMsgData[4096];

				MSG_Init( &voipMsg, voipMsgData, sizeof( voipMsgData ) );
				MSG_WriteShort( &voipMsg, sender );
				MSG_WriteByte( &voipMsg, generation );
				MSG_WriteLong( &voipMsg, sequence );
				MSG_WriteByte( &voipMsg, frames );
				MSG_WriteShort( &voipMsg, voipLen );
				MSG_WriteBits( &voipMsg, flags,
					(clc.svVoipVersion >= 2) ? VOIP_FLAGCNT : VOIP_FLAGCNT_V1 );
				MSG_WriteData( &voipMsg, voipData, voipLen );

				MSG_BeginReading( &voipMsg );
				CL_ParseVoip( &voipMsg, qfalse );
			}
		}
	}
#endif

	tvPlay.serverTime = serverTime;

	// Track last server time for seek clamping
	if ( serverTime > tvPlay.lastServerTime ) {
		tvPlay.lastServerTime = serverTime;
	}
}


/*
===============
CL_TV_ReadFrame

Read one frame from the current file position.
===============
*/
void CL_TV_ReadFrame( void ) {
	unsigned int frameSize;

	// Read frame size (4 bytes from compressed stream)
	if ( CL_TV_DecompressRead( &frameSize, 4 ) != 4 || frameSize == 0 ) {
		tvPlay.atEnd = qtrue;
		return;
	}

	if ( frameSize > sizeof( tvPlay.msgBuf ) ) {
		Com_Printf( S_COLOR_YELLOW "TV: Frame too large (%u)\n", frameSize );
		tvPlay.atEnd = qtrue;
		return;
	}

	// Read Huffman-encoded payload from compressed stream
	if ( CL_TV_DecompressRead( tvPlay.msgBuf, frameSize ) != (int)frameSize ) {
		tvPlay.atEnd = qtrue;
		return;
	}

	CL_TV_ParseFrame( tvPlay.msgBuf, frameSize, qfalse ); // VOD: no keyframes
}


/*
===============
CL_TV_ReadLiveSegment

Decode ONE live segment into segOut. The segment is the retry unit: if the whole
compressed payload isn't present yet, rewind and report STARVED (caller retries
next engine frame). Returns ENDED on TVLe/corruption, OK after a full segment
decompresses.

NOTE: this never skips segments. A TVL segment is a coarse multi-second unit
(keyframe + many deltas), and in healthy steady state the next whole segment is
normally buffered ahead — so per-read skipping would drop live content and the
cgame would lerp entities across the gap (through walls). Backlog catch-up is done
out-of-band by TVRing_CatchUp at two one-shot sites — the initial boot
(CL_TV_NextLiveFrame) and the map-change asset-gate resume (CL_TV_LiveMapChange) —
where jumping the coarse backlog is exactly what's wanted.
===============
*/
static tvSegResult_t CL_TV_ReadLiveSegment( void ) {
	long segStart;
	byte magic[4];
	byte hdr[8];           // keyframeServerTime(4) + payloadLen(4), follows a "TVLs" magic
	unsigned int payloadLen;
	int got;

#ifdef __EMSCRIPTEN__
	tvRing.tail = tvRing.cursor;   // reclaim bytes the consumer has moved past
#endif

	segStart = CL_TV_RawTell();

	// The writer frames a segment as "TVLs" + 8-byte header, but ends a session
	// with a BARE 4-byte "TVLe" marker (sv_tvstream.c). So read the 4-byte magic
	// first and dispatch — a fixed 12-byte read would over-read 8 bytes past TVLe
	// and swallow the next session's "TVL1" header on a map change.
	got = CL_TV_RawRead( magic, 4 );
	if ( got < 4 ) { CL_TV_RawSeek( segStart ); return TVSEG_STARVED; }

	if ( !Q_strncmp( (char *)magic, "TVLe", 4 ) ) {
		// End of this session. Peek the next 4 bytes: a "TVL1" means the stream
		// continues into the next match (in-place map change); anything else — or
		// bytes not here yet — is a true end (closed) or a wait (still live).
		byte next[4];
		got = CL_TV_RawRead( next, 4 );
		if ( got < 4 ) {
			// Next session's header not here yet. Rewind PAST the TVLe (to segStart)
			// so a STARVED retry re-reads the marker and re-peeks — otherwise the
			// retry would read the later-arriving "TVL1" as a segment magic and err.
			CL_TV_RawSeek( segStart );
			return cl_tvStreamClosed->integer ? TVSEG_ENDED : TVSEG_STARVED;
		}
		if ( !Q_strncmp( (char *)next, "TVL1", 4 ) ) {
			// Positioned just past the new session's "TVL1" magic — exactly what
			// CL_TV_ReadStreamHeader expects. Caller drives CL_TV_LiveMapChange.
			return TVSEG_NEWSTREAM;
		}
		return TVSEG_ENDED;  // TVLe followed by non-TVL1: genuine end
	}
	if ( Q_strncmp( (char *)magic, "TVLs", 4 ) ) {
		Com_Printf( S_COLOR_YELLOW "TV: bad live segment magic\n" );
		return TVSEG_ENDED;
	}

	got = CL_TV_RawRead( hdr, 8 );
	if ( got < 8 ) { CL_TV_RawSeek( segStart ); return TVSEG_STARVED; }

	// payloadLen = little-endian u32 at hdr+4 (keyframeServerTime at hdr+0 is unused for playback).
	payloadLen = (unsigned int)( hdr[4] | ( hdr[5] << 8 ) | ( hdr[6] << 16 ) | ( (unsigned int)hdr[7] << 24 ) );
	if ( payloadLen == 0 || payloadLen > sizeof( tvPlay.segIn ) ) {
		Com_Printf( S_COLOR_YELLOW "TV: live segment payload %u out of range\n", payloadLen );
		return TVSEG_ENDED;
	}

	got = CL_TV_RawRead( tvPlay.segIn, payloadLen );
	if ( got < (int)payloadLen ) { CL_TV_RawSeek( segStart ); return TVSEG_STARVED; }

	// Decompress the whole independent segment (fresh session per segment).
	ZSTD_DCtx_reset( tvPlay.dstream, ZSTD_reset_session_only );
	{
		ZSTD_inBuffer in = { tvPlay.segIn, payloadLen, 0 };
		ZSTD_outBuffer out = { tvPlay.segOut, sizeof( tvPlay.segOut ), 0 };
		size_t ret;
		do {
			ret = ZSTD_decompressStream( tvPlay.dstream, &out, &in );
			if ( ZSTD_isError( ret ) ) {
				Com_Printf( S_COLOR_YELLOW "TV: live segment zstd error\n" );
				return TVSEG_ENDED;
			}
			// Output buffer full but frame not finished => segment decompresses larger
			// than segOut. Fail loudly rather than silently dropping the tail.
			if ( out.pos == out.size && ret != 0 ) {
				Com_Printf( S_COLOR_YELLOW "TV: live segment too large (decompressed > %u)\n", (unsigned)sizeof( tvPlay.segOut ) );
				return TVSEG_ENDED;
			}
		} while ( in.pos < in.size );
		// ret == 0 means the independent segment frame is complete; otherwise truncated.
		if ( ret != 0 ) {
			Com_Printf( S_COLOR_YELLOW "TV: live segment truncated\n" );
			return TVSEG_ENDED;
		}
		tvPlay.segOutLen = out.pos;
		tvPlay.segCursor = 0;
	}
	return TVSEG_OK;
}


/*
===============
CL_TV_LiveBootstrap

Re-assert the \tv\1 TV marker after the first live keyframe. The keyframe's
configstring section overwrites CS_SERVERINFO with the server's real value, so
mirror CL_TV_Open's injection (read current CS_SERVERINFO, re-apply through
CL_TV_UpdateConfigstring, which auto-injects \tv\1) to keep cgame in TV mode.
===============
*/
static void CL_TV_LiveBootstrap( void ) {
	const char *si = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
	CL_TV_UpdateConfigstring( CS_SERVERINFO, si, (int)strlen( si ) );
#ifdef USE_VOIP
	clc.svVoipVersion = atoi( Info_ValueForKey( si, "sv_voipVersion" ) );
#endif
}


/*
===============
CL_TV_ResyncLiveClock

Called from the live pump after the initial catch-up jumped serverTime forward.
Snaps the client clock to the new edge (mirrors CL_TV_Seek's tail); without it
cl.serverTime trails the jump by the whole backlog and cgame lerps for seconds. A
duplicate snapshot neutralizes the pre-jump one so the transition doesn't glide.
===============
*/
void CL_TV_ResyncLiveClock( void ) {
	CL_TV_BuildSnapshot();
	cl.snap = cl.snapshots[clc.serverMessageSequence & PACKET_MASK];
	cl.newSnapshots = qtrue;
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
	cl.serverTime = cl.snap.serverTime;
	cl.oldServerTime = cl.snap.serverTime;
	cl.oldFrameServerTime = cl.snap.serverTime;
}

#ifdef __EMSCRIPTEN__
// Adaptive clock-rate limits for CL_TV_AdjustLiveClock.
#define TV_CLOCK_MAX_RATE 0.05f   // ±5% clock skew: below the perceptual threshold
#define TV_CLOCK_DEADBAND 200     // ms of headroom error tolerated before correcting
#define TV_CLOCK_GAIN     0.0005f // skew rate per ms of error past the deadband

/*
===============
CL_TV_AdjustLiveClock

Steady-state live clock controller (WASM live only). Holds the render edge
~tvPlay.effKeepMs behind the newest buffered segment by nudging serverTimeDelta a
few percent per frame — an imperceptible speed change that drains or refills the
jitter cushion smoothly, instead of the visible keyframe snap a hard reseek
causes. Only smooths sub-cushion drift; large backlogs are left to the one-shot
segment-skip catch-up (boot / map change / foreground re-arm). Also relaxes the
adaptive cushion back toward the base after a quiet spell. Called once per applied
live frame from CL_SetCGameTime (CL_AdjustTimeDelta is inert for demo-backed
playback, so there is no incumbent drift to fight).
===============
*/
void CL_TV_AdjustLiveClock( void ) {
	int newestKf, headroom, err;
	float rate;

	if ( !tvPlay.bootstrapped ) {
		return;
	}

	// Relax the cushion one step once underruns have stopped for a while.
	if ( tvPlay.effKeepMs > TV_CATCHUP_KEEP_MS
		&& cls.realtime - tvPlay.lastKeepAdapt > TV_KEEP_DECAY_MS ) {
		tvPlay.effKeepMs -= TV_KEEP_STEP_MS;
		if ( tvPlay.effKeepMs < TV_CATCHUP_KEEP_MS ) {
			tvPlay.effKeepMs = TV_CATCHUP_KEEP_MS;
		}
		tvPlay.lastKeepAdapt = cls.realtime;
	}

	newestKf = TVRing_NewestKeyframeTime();
	if ( !newestKf ) {
		return;   // nothing buffered ahead to measure against
	}
	headroom = newestKf - cl.serverTime;   // >0: live edge ahead of us (slack)
	err = headroom - tvPlay.effKeepMs;     // >0: too much slack -> speed up to drain
	if ( err > TV_CLOCK_DEADBAND ) {
		rate = ( err - TV_CLOCK_DEADBAND ) * TV_CLOCK_GAIN;
	} else if ( err < -TV_CLOCK_DEADBAND ) {
		rate = ( err + TV_CLOCK_DEADBAND ) * TV_CLOCK_GAIN;
	} else {
		return;   // within deadband: run at wall-clock rate
	}
	if ( rate > TV_CLOCK_MAX_RATE ) {
		rate = TV_CLOCK_MAX_RATE;
	} else if ( rate < -TV_CLOCK_MAX_RATE ) {
		rate = -TV_CLOCK_MAX_RATE;
	}
	// serverTime = realtime + serverTimeDelta, so a small per-frame delta nudge
	// makes serverTime advance rate-fraction faster/slower than the wall clock.
	// Accumulate the fractional milliseconds; apply whole ms to the integer delta.
	tvPlay.clockSkewAccum += rate * (float)cls.frametime;
	if ( tvPlay.clockSkewAccum >= 1.0f || tvPlay.clockSkewAccum <= -1.0f ) {
		int whole = (int)tvPlay.clockSkewAccum;
		cl.serverTimeDelta += whole;
		tvPlay.clockSkewAccum -= (float)whole;
	}
}

// Record a steady-state ring underrun: grow the adaptive cushion one step (capped)
// so the next refill aims deeper. Edge-triggered via tvPlay.starved — one bump per
// starvation episode — and ignored during boot / map-change gaps, which aren't
// jitter underruns.
static void CL_TV_NoteUnderrun( void ) {
	if ( tvPlay.starved ) {
		return;   // already counted this episode
	}
	tvPlay.starved = qtrue;
	if ( !tvPlay.bootstrapped || tvPlay.needInitialCatchUp
		|| tvPlay.awaitingAssets || tvPlay.awaitingHeader ) {
		return;
	}
	if ( tvPlay.effKeepMs < TV_CATCHUP_KEEP_MAX ) {
		tvPlay.effKeepMs += TV_KEEP_STEP_MS;
		if ( tvPlay.effKeepMs > TV_CATCHUP_KEEP_MAX ) {
			tvPlay.effKeepMs = TV_CATCHUP_KEEP_MAX;
		}
		Com_DPrintf( "TV: underrun, cushion -> %dms\n", tvPlay.effKeepMs );
	}
	tvPlay.lastKeepAdapt = cls.realtime;
}
#endif


/*
===============
CL_TV_LiveMapChange

Drive an in-place map change when the live stream rolls from one match's session
(ended with TVLe) into the next (a fresh TVL1 header). Reached from
CL_TV_NextLiveFrame when CL_TV_ReadLiveSegment returns TVSEG_NEWSTREAM, with the
file positioned just past the new session's "TVL1" magic.

Unlike the React reboot it replaces, this keeps the WASM module — and therefore
the AudioContext and pointer lock — alive: CL_FlushMemory takes the
REF_KEEP_CONTEXT branch (fs_game is fixed per stream, D5), so only the registered
asset set swaps, not the GL/audio devices. Mirrors CL_TV_OpenLive's session
bootstrap, then runs the Phase-0-proven reload recipe. Returns qtrue once the new
session's first keyframe is applied and cgame re-inited; qfalse (atEnd set) on a
truncated/corrupt new header.
===============
*/
static qboolean CL_TV_LiveMapChange( void ) {
	// 1. Consume the new session's header ONCE, recording the target map. Done in
	//    its own phase because the change may then pause for an async asset fetch;
	//    re-reading the header on each wait-frame would desync the byte stream.
	if ( !tvPlay.awaitingAssets ) {
		char mapname[MAX_QPATH];
		long hdrStart = CL_TV_RawTell();
		tvSegResult_t hr = CL_TV_ReadStreamHeader( mapname, sizeof( mapname ) );
		if ( hr == TVSEG_STARVED ) {
			// The next session's header spans feed chunks not all arrived yet. Rewind
			// and re-enter next frame (awaitingHeader); the OLD cgame keeps rendering
			// meanwhile. A closed connection here is a genuine truncated end.
			CL_TV_RawSeek( hdrStart );
			if ( cl_tvStreamClosed->integer ) {
				tvPlay.awaitingHeader = qfalse;
				tvPlay.atEnd = qtrue;
				return qfalse;
			}
			tvPlay.awaitingHeader = qtrue;
			return qfalse;
		}
		tvPlay.awaitingHeader = qfalse;
		if ( hr != TVSEG_OK ) {
			Com_Printf( S_COLOR_YELLOW "TV: live map change header truncated\n" );
			tvPlay.atEnd = qtrue;
			return qfalse;
		}
		Q_strncpyz( tvPlay.pendingMap, mapname, sizeof( tvPlay.pendingMap ) );
		// Signal the transition START so the web client raises the levelshot overlay
		// (covering the download); cl_tvMapSerial bumps at the END (completion).
		Cvar_Set( "cl_tvMapName", mapname );
		tvPlay.awaitingAssets = qtrue;
		// Source now sits at the new session's first TVLs (its keyframe); it's read in
		// the reload below once assets are present. Until then the OLD cgame keeps
		// rendering session A — we deliberately don't reset any state yet.
	}

	// 2. Asset gate: the new map's BSP must be loadable before CL_InitCGame. If it
	//    isn't, hand the map name to the JS loader (curl is compiled out of the WASM
	//    build, so JS fetch is the only download transport) and wait; NextLiveFrame
	//    re-enters here each frame via tvPlay.awaitingAssets until JS signals ready.
	if ( !CL_TV_MapAvailable( tvPlay.pendingMap ) ) {
		if ( !cl_tvAssetsReady->integer ) {
			Cvar_Set( "cl_tvPendingMap", tvPlay.pendingMap );
			return qfalse;
		}
		// JS finished the fetch: rescan so the new pk3's BSP is found. FS_Restart
		// uses closemfp=qfalse, so our open live file handle (tvPlay.file) survives.
		FS_Restart( clc.checksumFeed );
		Cvar_Set( "cl_tvAssetsReady", "0" );
	}
	Cvar_Set( "cl_tvPendingMap", "" );
	tvPlay.awaitingAssets = qfalse;

#ifdef __EMSCRIPTEN__
	// Catch up the backlog the pk3 download accumulated: the relay kept feeding the
	// new session's segments while we waited, so trim to the small jitter cushion
	// rather than replaying from the now-stale first keyframe. Keeps the lag from
	// compounding across maps without dropping to the jittery live edge. (The other
	// catch-up site is the initial boot — see CL_TV_NextLiveFrame.)
	{
		int keepMs = tvPlay.effKeepMs > 0 ? tvPlay.effKeepMs : TV_CATCHUP_KEEP_MS;
		int skipped = TVRing_CatchUp( keepMs );
		if ( skipped ) {
			Com_DPrintf( "TV: map-change catch-up skipped %d segment(s), kept ~%dms buffered\n", skipped, keepMs );
		}
	}
#endif

	// 3. Reset per-session parse state, mirroring CL_TV_OpenLive. The new keyframe
	//    is a full I-frame, so clear the entity/player delta baselines and the
	//    sequence/segment counters before applying it. cl.gameState is left intact
	//    here and reconciled by the keyframe-authoritative reader in
	//    CL_TV_ParseFrame (stale configstrings the new keyframe omits are cleared).
	tvPlay.bootstrapped = qfalse;
	tvPlay.segOutLen = 0;
	tvPlay.segCursor = 0;
	Com_Memset( tvPlay.entities, 0, sizeof( tvPlay.entities ) );
	Com_Memset( tvPlay.entityBitmask, 0, sizeof( tvPlay.entityBitmask ) );
	Com_Memset( tvPlay.players, 0, sizeof( tvPlay.players ) );
	Com_Memset( tvPlay.playerBitmask, 0, sizeof( tvPlay.playerBitmask ) );
	cl.parseEntitiesNum = 0;
	clc.serverMessageSequence = 0;
	clc.serverCommandSequence = 0;
	clc.lastExecutedServerCommand = 0;
	clc.clientNum = 0;

	// 4. Apply the new session's first keyframe (re-baselines firstServerTime and
	//    re-asserts the TV marker via the bootstrap block inside NextLiveFrame).
	if ( !CL_TV_NextLiveFrame() ) {
		Com_Printf( S_COLOR_YELLOW "TV: live map change missing initial keyframe\n" );
		tvPlay.atEnd = qtrue;
		return qfalse;
	}
	tvPlay.viewpoint = CL_TV_FindFirstActivePlayer();
	if ( tvPlay.viewpoint < 0 ) {
		tvPlay.viewpoint = 0;
	}
	clc.clientNum = tvPlay.viewpoint;
	VectorCopy( tvPlay.players[tvPlay.viewpoint].origin, tvPlay.viewOrigin );

	// Swallow the keyframe's "cs" diff dump — see CL_TV_OpenLive's bootstrap.
	clc.lastExecutedServerCommand = clc.serverCommandSequence;

	// 5. Build snapshot #1 BEFORE the cgame re-init, mirroring CL_TV_OpenLive's
	//    pre-return build. CG_INIT must see serverMessageSequence already past this
	//    snapshot (it sets processedSnapshotNum to it) and serverCommandSequence at
	//    the keyframe's value; building only after CG_INIT leaves both at 0 and the
	//    cgame requests a command number it never received.
	CL_TV_BuildSnapshot();

	// 6. Reload the map in place (keep-context). Leaves cls.state = CA_PRIMED; the
	//    pump in CL_SetCGameTime breaks on that so the main loop's next pass runs
	//    CL_FirstSnapshot and re-baselines serverTime to the new session cleanly.
	cls.state = CA_LOADING;
	CL_FlushMemory();
	cls.cgameStarted = qtrue;
	CL_InitCGame();
	// Snapshot #2, readable by the freshly-inited cgame (CG_INIT set
	// processedSnapshotNum = snapshot #1's messageNum). Mirrors cl_main.c:2143.
	CL_TV_BuildSnapshot();

	// 7. Signal the web client (no reboot now) to refresh roster + levelshot.
	Cvar_SetIntegerValue( "cl_tvMapSerial", cl_tvMapSerial->integer + 1 );
	Cvar_SetIntegerValue( "cl_tvViewpoint", tvPlay.viewpoint );

	Com_DPrintf( "TV: live map change -> %s (svFps %i)\n", tvPlay.pendingMap, tvPlay.svFps );
	return qtrue;
}


/*
===============
CL_TV_MapAvailable

Is mapname's BSP loadable from the current filesystem? False until the map's pk3
is mounted (the live map-change gate fetches it via JS, then FS_Restarts).
===============
*/
static qboolean CL_TV_MapAvailable( const char *mapname ) {
	fileHandle_t h = FS_INVALID_HANDLE;
	int len;

	if ( !mapname[0] ) {
		return qfalse;
	}
	len = FS_FOpenFileRead( va( "maps/%s.bsp", mapname ), &h, qfalse );
	if ( h != FS_INVALID_HANDLE ) {
		FS_FCloseFile( h );
	}
	return ( len > 0 ) ? qtrue : qfalse;
}


/*
===============
CL_TV_NextLiveFrame

Pull and apply the next live frame, decoding the next segment when the current
one is exhausted. Render-paced (one frame per call). Returns qtrue if a frame was
applied, qfalse on starved/ended. Sets tvPlay.atEnd on clean end (TVLe) or a
dropped connection. Captures firstServerTime + asserts the TV marker on the first
applied frame. On a TVLe->TVL1 boundary drives CL_TV_LiveMapChange (in-place
reload) instead of ending.
===============
*/
qboolean CL_TV_NextLiveFrame( void ) {
	// A map change is paused — either waiting on the next session's header bytes
	// (awaitingHeader) or on the next map's assets (awaitingAssets); see the gates
	// in CL_TV_LiveMapChange. Re-enter it each frame until it can proceed; don't
	// pull live frames meanwhile (the old session keeps rendering).
	if ( tvPlay.awaitingHeader || tvPlay.awaitingAssets ) {
		return CL_TV_LiveMapChange();
	}
#ifdef __EMSCRIPTEN__
	// Boot analogue of the map-change catch-up: once the boot-time backlog reaches
	// the ring, trim to the jitter cushion and drop the stale bootstrap segment so
	// the caught-up keyframe re-syncs. Retries each frame until it skips (backlog
	// lands a frame or two in); the TVSEG_OK path clears it if there's nothing to skip.
	if ( tvPlay.needInitialCatchUp ) {
		int keepMs = tvPlay.effKeepMs > 0 ? tvPlay.effKeepMs : TV_CATCHUP_KEEP_MS;
		int skipped = TVRing_CatchUp( keepMs );
		if ( skipped ) {
			tvPlay.needInitialCatchUp = qfalse;
			tvPlay.liveClockResync = qtrue;        // serverTime jumps below — snap the clock after the pump
			tvPlay.reconcileSilent = qtrue;        // the reconciling keyframe replays skipped history — no per-cs events
			tvPlay.segCursor = tvPlay.segOutLen;   // abandon the stale bootstrap segment
			Com_DPrintf( "TV: initial catch-up skipped %d segment(s), kept ~%dms buffered\n", skipped, keepMs );
		}
	}
#endif
	for ( ;; ) {
		// A complete record available in the current segment?
		if ( tvPlay.segCursor + 4 <= tvPlay.segOutLen ) {
			unsigned int fsz = (unsigned int)( tvPlay.segOut[tvPlay.segCursor]
				| ( tvPlay.segOut[tvPlay.segCursor+1] << 8 )
				| ( tvPlay.segOut[tvPlay.segCursor+2] << 16 )
				| ( (unsigned int)tvPlay.segOut[tvPlay.segCursor+3] << 24 ) );
			if ( fsz != 0 && (size_t)fsz <= tvPlay.segOutLen - ( tvPlay.segCursor + 4 ) ) {
				qboolean isKeyframe = tvPlay.segFirstRecord;
				tvPlay.segFirstRecord = qfalse;
				tvPlay.segCursor += 4;
				CL_TV_ParseFrame( tvPlay.segOut + tvPlay.segCursor, (int)fsz, isKeyframe );
				tvPlay.segCursor += fsz;
				if ( isKeyframe && tvPlay.reconcileSilent ) {
					// deliver the suppressed changes as state, not events
					tvPlay.reconcileSilent = qfalse;
					CL_TV_WriteCommand( va( "tv_seek_sync %i", tvPlay.viewpoint ) );
				}
				if ( !tvPlay.bootstrapped ) {
					tvPlay.firstServerTime = tvPlay.serverTime;
					tvPlay.bootstrapped = qtrue;
					CL_TV_LiveBootstrap();
				}
#ifdef __EMSCRIPTEN__
				tvPlay.starved = qfalse;   // a frame applied: end any underrun episode
#endif
				return qtrue;
			}
			// malformed / trailing padding: treat segment as exhausted
			tvPlay.segCursor = tvPlay.segOutLen;
		}
		// Current segment exhausted: decode the next one.
		{
			tvSegResult_t r = CL_TV_ReadLiveSegment();
			if ( r == TVSEG_ENDED ) { tvPlay.atEnd = qtrue; return qfalse; }
			if ( r == TVSEG_STARVED ) {
				if ( cl_tvStreamClosed->integer ) { tvPlay.atEnd = qtrue; }
#ifdef __EMSCRIPTEN__
				else { CL_TV_NoteUnderrun(); }   // ring ran dry mid-play: grow the cushion
#endif
				return qfalse;
			}
			if ( r == TVSEG_NEWSTREAM ) {
				// TVLe -> TVL1: the next match's session. Reload in place rather
				// than ending the stream; returns with the new keyframe applied.
				return CL_TV_LiveMapChange();
			}
			// TVSEG_OK: segOut refilled (segCursor=0). Every segment begins with a
			// full keyframe (delta-from-zero), so reset the running delta baseline
			// before applying it — the keyframe must rebuild state from scratch
			// (a true I-frame) rather than merge onto our prior state. Without
			// this, fields that are zero in the keyframe but non-zero in our state
			// (e.g. scores/powerups reset at warmup->active) would never clear.
			// Scope is the entity/player delta baseline only; configstrings are
			// reconciled in CL_TV_ParseFrame (isKeyframe), not here.
			Com_Memset( tvPlay.entities, 0, sizeof( tvPlay.entities ) );
			Com_Memset( tvPlay.entityBitmask, 0, sizeof( tvPlay.entityBitmask ) );
			Com_Memset( tvPlay.players, 0, sizeof( tvPlay.players ) );
			Com_Memset( tvPlay.playerBitmask, 0, sizeof( tvPlay.playerBitmask ) );
			tvPlay.segFirstRecord = qtrue; // first record of the new segment is its keyframe
#ifdef __EMSCRIPTEN__
			tvPlay.needInitialCatchUp = qfalse;   // bootstrap drained: nothing to catch up
#endif
		}
	}
}


/*
===============
CL_TV_InjectScores

Synthesize a "scores" server command from playerState data
so cgame's scoreboard always has up-to-date information.
===============
*/

// playerState_t.persistant[] indices (from game/bg_public.h)
#define TV_PERS_SCORE					0
#define TV_PERS_RANK					2
#define TV_PERS_KILLED					8
#define TV_PERS_IMPRESSIVE_COUNT		9
#define TV_PERS_EXCELLENT_COUNT		10
#define TV_PERS_DEFEND_COUNT			11
#define TV_PERS_ASSIST_COUNT			12
#define TV_PERS_GAUNTLET_FRAG_COUNT	13
#define TV_PERS_CAPTURES				14

static void CL_TV_InjectScores( void ) {
	char buf[MAX_STRING_CHARS];
	int len, count, i;
	playerState_t *ps;
	int perfect, powerups;

	// Count active players
	count = 0;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( tvPlay.playerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) {
			count++;
		}
	}

	// "scores <count> <redScore> <blueScore>"
	len = Com_sprintf( buf, sizeof( buf ), "scores %i 0 0", count );

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( !( tvPlay.playerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			continue;
		}

		ps = &tvPlay.players[i];
		perfect = ( ps->persistant[TV_PERS_RANK] == 0 &&
					ps->persistant[TV_PERS_KILLED] == 0 ) ? 1 : 0;
		powerups = tvPlay.entities[i].powerups;

		len += Com_sprintf( buf + len, sizeof( buf ) - len,
			" %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
			i,
			ps->persistant[TV_PERS_SCORE],
			0,		// ping
			0,		// time
			0,		// scoreFlags
			powerups,
			0,		// accuracy
			ps->persistant[TV_PERS_IMPRESSIVE_COUNT],
			ps->persistant[TV_PERS_EXCELLENT_COUNT],
			ps->persistant[TV_PERS_GAUNTLET_FRAG_COUNT],
			ps->persistant[TV_PERS_DEFEND_COUNT],
			ps->persistant[TV_PERS_ASSIST_COUNT],
			perfect,
			ps->persistant[TV_PERS_CAPTURES] );
	}

	CL_TV_WriteCommand( buf );
}


/*
===============
CL_TV_BuildSnapshot

Build tvPlay.snapshots[which] from current running state.
When more than MAX_ENTITIES_IN_SNAPSHOT entities are active,
keeps the nearest ones by distance from the current view origin.
===============
*/

/*
===============
CL_TV_SkipEventEntity

Returns qtrue if a freestanding event entity should be excluded from the
snapshot because it targets a player other than the one being followed.
Events like score plums and voice chats are only meaningful for the
player they belong to.
===============
*/
static qboolean CL_TV_SkipEventEntity( const entityState_t *es ) {
	if ( es->eType == ET_EVENTS + EV_SCOREPLUM &&
		 es->otherEntityNum != tvPlay.viewpoint ) {
		return qtrue;
	}

	return qfalse;
}


typedef struct {
	int		entityNum;
	float	distSq;
} tvEntDist_t;

static int TV_EntDistCompare( const void *a, const void *b ) {
	float da = ((const tvEntDist_t *)a)->distSq;
	float db = ((const tvEntDist_t *)b)->distSq;
	if ( da < db ) return -1;
	if ( da > db ) return 1;
	return 0;
}

void CL_TV_BuildSnapshot( void ) {
	clSnapshot_t *snap;
	int count, i, total, msgNum;

	// Inject synthetic scores so cgame scoreboard is always current
	CL_TV_InjectScores();

	// Advance message sequence and write into standard ring buffer
	msgNum = ++clc.serverMessageSequence;
	snap = &cl.snapshots[msgNum & PACKET_MASK];
	Com_Memset( snap, 0, sizeof( *snap ) );

	snap->valid = qtrue;
	snap->serverTime = tvPlay.serverTime;
	snap->messageNum = msgNum;
	snap->deltaNum = msgNum - 1;
	snap->snapFlags = 0;
	snap->ping = 0;
	snap->serverCommandNum = clc.serverCommandSequence;

	// All areas visible (0 = visible, 1 = blocked)
	snap->areabytes = MAX_MAP_AREA_BYTES;
	Com_Memset( snap->areamask, 0x00, sizeof( snap->areamask ) );

	// Player state from followed viewpoint
	snap->ps = tvPlay.players[tvPlay.viewpoint];
	snap->ps.clientNum = tvPlay.viewpoint;

	// Mark entity start position in standard circular buffer
	snap->parseEntitiesNum = cl.parseEntitiesNum;

	// Count active entities (excluding viewpoint and filtered events)
	total = 0;
	for ( i = 0; i < MAX_GENTITIES - 1; i++ ) {
		if ( !( tvPlay.entityBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) )
			continue;
		if ( i == tvPlay.viewpoint )
			continue;
		// Backstop for the writer invariant: entities are stored by number, so a
		// real slot has entities[i].number == i. A flagged slot that doesn't is a
		// zeroed phantom; emitting it routes number 0 onto cg_entities[0]. Skip it.
		if ( tvPlay.entities[i].number != i )
			continue;
		if ( CL_TV_SkipEventEntity( &tvPlay.entities[i] ) )
			continue;
		total++;
	}

	if ( total <= MAX_ENTITIES_IN_SNAPSHOT ) {
		// All fit — simple copy, no sorting needed
		count = 0;
		for ( i = 0; i < MAX_GENTITIES - 1; i++ ) {
			if ( !( tvPlay.entityBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) )
				continue;
			if ( i == tvPlay.viewpoint )
				continue;
			if ( tvPlay.entities[i].number != i )   // skip zeroed phantom (see count loop)
				continue;
			if ( CL_TV_SkipEventEntity( &tvPlay.entities[i] ) )
				continue;
			cl.parseEntities[cl.parseEntitiesNum & (MAX_PARSE_ENTITIES-1)] =
				tvPlay.entities[i];
			cl.parseEntitiesNum++;
			count++;
		}
	} else {
		// Too many entities — keep the nearest MAX_ENTITIES_IN_SNAPSHOT
		tvEntDist_t candidates[MAX_GENTITIES];
		int n = 0;

		for ( i = 0; i < MAX_GENTITIES - 1; i++ ) {
			if ( !( tvPlay.entityBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) )
				continue;
			if ( i == tvPlay.viewpoint )
				continue;
			if ( tvPlay.entities[i].number != i )   // skip zeroed phantom (see count loop)
				continue;
			if ( CL_TV_SkipEventEntity( &tvPlay.entities[i] ) )
				continue;
			candidates[n].entityNum = i;
			candidates[n].distSq = DistanceSquared(
				tvPlay.viewOrigin, tvPlay.entities[i].pos.trBase );
			n++;
		}

		qsort( candidates, n, sizeof( candidates[0] ), TV_EntDistCompare );

		count = ( n < MAX_ENTITIES_IN_SNAPSHOT ) ? n : MAX_ENTITIES_IN_SNAPSHOT;
		for ( i = 0; i < count; i++ ) {
			cl.parseEntities[cl.parseEntitiesNum & (MAX_PARSE_ENTITIES-1)] =
				tvPlay.entities[candidates[i].entityNum];
			cl.parseEntitiesNum++;
		}
	}

	snap->numEntities = count;

	// Update cl.snap and signal new snapshot
	cl.snap = *snap;
	cl.newSnapshots = qtrue;
}


/*
===============
CL_TV_Seek
===============
*/
void CL_TV_Seek( int targetTime ) {
	int prevMsgNum;

	if ( !tvPlay.active ) {
		return;
	}

	// Seeking is meaningless on a delayed-edge live stream; hard-disable it
	// even though tv_seek isn't registered in live mode (defense in depth).
	if ( tvPlay.live ) {
		Com_Printf( "TV: seeking is disabled in live mode\n" );
		return;
	}

	// Clamp
	if ( targetTime < tvPlay.firstServerTime ) {
		targetTime = tvPlay.firstServerTime;
	}
	if ( tvPlay.totalDuration > 0 && targetTime > tvPlay.firstServerTime + tvPlay.totalDuration ) {
		targetTime = tvPlay.firstServerTime + tvPlay.totalDuration;
	}

	if ( targetTime >= tvPlay.serverTime && !tvPlay.atEnd ) {
		// Forward seek: continue streaming from current position
		// Entity/player delta state and configstrings are already correct
		tvPlay.seeking = qtrue;

		while ( tvPlay.serverTime < targetTime && !tvPlay.atEnd ) {
			CL_TV_ReadFrame();
		}

		tvPlay.seeking = qfalse;
	} else {
		int j;

		// Backward seek: full reset required

		// Restore initial gameState (configstrings are delta-encoded from header)
		cl.gameState = tvPlay.initialGameState;

		// Seek to the first frame and reset all running state
		FS_Seek( tvPlay.file, tvPlay.firstFrameOffset, FS_SEEK_SET );
		Com_Memset( tvPlay.entities, 0, sizeof( tvPlay.entities ) );
		Com_Memset( tvPlay.entityBitmask, 0, sizeof( tvPlay.entityBitmask ) );
		Com_Memset( tvPlay.players, 0, sizeof( tvPlay.players ) );
		Com_Memset( tvPlay.playerBitmask, 0, sizeof( tvPlay.playerBitmask ) );
		tvPlay.serverTime = 0;
		tvPlay.atEnd = qfalse;

		// Reset entity cursor (snapshot ring keeps incrementing to avoid
		// cgame's latestSnapshotNum going backward)
		cl.parseEntitiesNum = 0;
		clc.lastExecutedServerCommand = clc.serverCommandSequence;

		// Invalidate all snapshot ring buffer entries
		for ( j = 0; j < PACKET_BACKUP; j++ ) {
			cl.snapshots[j].valid = qfalse;
		}

		// Reset zstd decompressor session (without freeing context)
		ZSTD_DCtx_reset( tvPlay.dstream, ZSTD_reset_session_only );
		tvPlay.zstdInSize = 0;
		tvPlay.zstdInPos = 0;
		tvPlay.zstdOutSize = 0;
		tvPlay.zstdOutPos = 0;
		tvPlay.zstdStreamEnded = qfalse;

		// Skip command queueing during seek to avoid buffer overflow
		tvPlay.seeking = qtrue;

		// Read ALL frames from the beginning to ensure configstrings are correct
		while ( tvPlay.serverTime < targetTime && !tvPlay.atEnd ) {
			CL_TV_ReadFrame();
		}

		tvPlay.seeking = qfalse;
	}

	// Inject sync command BEFORE building snapshots so both include it
	{
		char syncCmd[MAX_STRING_CHARS];
		Com_sprintf( syncCmd, sizeof( syncCmd ), "tv_seek_sync %i",
			tvPlay.viewpoint );
		CL_TV_WriteCommand( syncCmd );
	}

	// Build both snapshots into standard ring buffer
	// (both will have serverCommandNum including tv_seek_sync)
	CL_TV_BuildSnapshot();

	if ( !tvPlay.atEnd ) {
		CL_TV_ReadFrame();
		CL_TV_BuildSnapshot();
	} else {
		// Duplicate: build again with same data but new messageNum
		CL_TV_BuildSnapshot();
	}

	// Update client state
	cl.snap = cl.snapshots[clc.serverMessageSequence & PACKET_MASK];
	cl.newSnapshots = qtrue;
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;

	// Find the previous snapshot for oldServerTime/oldFrameServerTime
	prevMsgNum = clc.serverMessageSequence - 1;
	if ( prevMsgNum > 0 && cl.snapshots[prevMsgNum & PACKET_MASK].valid ) {
		cl.oldServerTime = cl.snapshots[prevMsgNum & PACKET_MASK].serverTime;
		cl.oldFrameServerTime = cl.snapshots[prevMsgNum & PACKET_MASK].serverTime;
	} else {
		cl.oldServerTime = cl.snap.serverTime;
		cl.oldFrameServerTime = cl.snap.serverTime;
	}

	Cvar_SetIntegerValue( "cl_tvTime",
		tvPlay.serverTime - tvPlay.firstServerTime );

	// Snap cl.serverTime to the seek target so the view updates immediately,
	// even when paused (timescale 0) — the frozen branch of CL_SetCGameTime
	// never recomputes cl.serverTime, so without this it would stay stale.
	cl.serverTime = cl.snap.serverTime;
}


/*
===============
CL_TV_RebuildSnapshots

Rebuild both snapshots after a viewpoint change.
===============
*/
static void CL_TV_RebuildSnapshots( void ) {
	int prevMsgNum;

	// Update viewOrigin so CL_TV_BuildSnapshot culls entities relative
	// to the new viewpoint, not the old one.
	VectorCopy( tvPlay.players[tvPlay.viewpoint].origin, tvPlay.viewOrigin );

	// Build two NEW snapshots (no rollback — incrementing sequence so
	// cgame sees them as genuinely new via trap_GetCurrentSnapshotNumber).
	// CG_SetNextSnap detects the clientNum change and sets nextFrameTeleport,
	// and CG_TransitionSnapshot updates cg.clientNum automatically.
	CL_TV_BuildSnapshot();
	CL_TV_BuildSnapshot();

	clc.clientNum = tvPlay.viewpoint;
	Cvar_SetIntegerValue( "cl_tvViewpoint", tvPlay.viewpoint );

	// Update client timing state so the view updates immediately,
	// even when paused (timescale 0) — same pattern as CL_TV_Seek.
	// The frozen branch of CL_SetCGameTime never recomputes
	// cl.serverTime, so without this it would stay stale.
	cl.snap = cl.snapshots[clc.serverMessageSequence & PACKET_MASK];
	cl.newSnapshots = qtrue;
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;

	prevMsgNum = clc.serverMessageSequence - 1;
	if ( prevMsgNum > 0 && cl.snapshots[prevMsgNum & PACKET_MASK].valid ) {
		cl.oldServerTime = cl.snapshots[prevMsgNum & PACKET_MASK].serverTime;
		cl.oldFrameServerTime = cl.snapshots[prevMsgNum & PACKET_MASK].serverTime;
	} else {
		cl.oldServerTime = cl.snap.serverTime;
		cl.oldFrameServerTime = cl.snap.serverTime;
	}

	cl.serverTime = cl.snap.serverTime;
}


/*
===============
CL_TV_View_f
===============
*/
static void CL_TV_View_f( void ) {
	int n;

	if ( !tvPlay.active ) {
		Com_Printf( "Not playing a TV demo.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "tv_view <clientnum>\n" );
		return;
	}

	n = atoi( Cmd_Argv( 1 ) );
	if ( n < 0 || n >= MAX_CLIENTS ) {
		Com_Printf( "Invalid client number %i\n", n );
		return;
	}

	if ( !( tvPlay.playerBitmask[n >> 3] & ( 1 << ( n & 7 ) ) ) ) {
		Com_Printf( "Client %i is not active\n", n );
		return;
	}

	if ( CL_TV_GetPlayerTeam( n ) == TEAM_SPECTATOR ) {
		Com_Printf( "Client %i is a spectator\n", n );
		return;
	}

	tvPlay.viewpoint = n;
	CL_TV_RebuildSnapshots();
}


/*
===============
CL_TV_ViewNext_f
===============
*/
static void CL_TV_ViewNext_f( void ) {
	int i, next;

	if ( !tvPlay.active ) {
		Com_Printf( "Not playing a TV demo.\n" );
		return;
	}

	next = tvPlay.viewpoint;
	for ( i = 1; i <= MAX_CLIENTS; i++ ) {
		int candidate = ( tvPlay.viewpoint + i ) % MAX_CLIENTS;
		if ( ( tvPlay.playerBitmask[candidate >> 3] & ( 1 << ( candidate & 7 ) ) )
				&& CL_TV_GetPlayerTeam( candidate ) != TEAM_SPECTATOR ) {
			next = candidate;
			break;
		}
	}

	if ( next != tvPlay.viewpoint ) {
		tvPlay.viewpoint = next;
		CL_TV_RebuildSnapshots();
	}
}


/*
===============
CL_TV_ViewPrev_f
===============
*/
static void CL_TV_ViewPrev_f( void ) {
	int i, prev;

	if ( !tvPlay.active ) {
		Com_Printf( "Not playing a TV demo.\n" );
		return;
	}

	prev = tvPlay.viewpoint;
	for ( i = 1; i <= MAX_CLIENTS; i++ ) {
		int candidate = ( tvPlay.viewpoint - i + MAX_CLIENTS ) % MAX_CLIENTS;
		if ( ( tvPlay.playerBitmask[candidate >> 3] & ( 1 << ( candidate & 7 ) ) )
				&& CL_TV_GetPlayerTeam( candidate ) != TEAM_SPECTATOR ) {
			prev = candidate;
			break;
		}
	}

	if ( prev != tvPlay.viewpoint ) {
		tvPlay.viewpoint = prev;
		CL_TV_RebuildSnapshots();
	}
}


/*
===============
CL_TV_Seek_f
===============
*/
static void CL_TV_Seek_f( void ) {
	int seconds;

	if ( !tvPlay.active ) {
		Com_Printf( "Not playing a TV demo.\n" );
		return;
	}

	// Defense in depth: tv_seek isn't registered in live mode, so this is unreachable.
	if ( tvPlay.live ) {
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "tv_seek <seconds>\n" );
		return;
	}

	seconds = atoi( Cmd_Argv( 1 ) );
	CL_TV_Seek( tvPlay.firstServerTime + seconds * 1000 );
}


/*
===============
CL_TV_GetPlayerList

Returns a tab/newline-delimited string of active players for the web UI.
Format: "<viewpoint>\n<clientnum>\t<name>\t<team>\n..."
===============
*/
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char *CL_TV_GetPlayerList( void ) {
	static char buf[4096];
	int len, i;
	const char *cs;

	if ( !tvPlay.active ) {
		buf[0] = '\0';
		return buf;
	}

	// First line: current viewpoint
	len = Com_sprintf( buf, sizeof( buf ), "%i\n", tvPlay.viewpoint );

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		char nameBuf[MAX_QPATH], modelBuf[MAX_QPATH];
		int isVR;

		if ( !( tvPlay.playerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			continue;
		}
		cs = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS + i];

		// Info_ValueForKey uses a 2-entry static buffer, so copy results
		// before making more than 2 calls
		Q_strncpyz( nameBuf, Info_ValueForKey( cs, "n" ), sizeof( nameBuf ) );
		// Prefer the head model: in team gametypes "model" is the shared
		// team body, and the tracker's portraits are head-based site-wide.
		Q_strncpyz( modelBuf, Info_ValueForKey( cs, "hmodel" ), sizeof( modelBuf ) );
		if ( !modelBuf[0] ) {
			Q_strncpyz( modelBuf, Info_ValueForKey( cs, "model" ), sizeof( modelBuf ) );
		}
		isVR = atoi( Info_ValueForKey( cs, "vr" ) );

		len += Com_sprintf( buf + len, sizeof( buf ) - len, "%i\t%s\t%i\t%s\t%i\n",
			i, nameBuf, CL_TV_GetPlayerTeam( i ), modelBuf, isVR );
	}

	return buf;
}
