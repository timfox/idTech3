/*
 * Live TV stream tap — see docs. Broadcasts the in-progress match in the
 * "TVL1" wire format over a loopback TCP socket. Single-threaded,
 * non-blocking, polled from the server frame. Never blocks the game loop.
 */
#include "server.h"

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
typedef int socklen_t;
#	define TVS_INVALID_SOCKET (-1)
#	define tvs_close closesocket
#	define tvs_wouldblock(e) ((e) == WSAEWOULDBLOCK)
#	define TVS_SEND_FLAGS 0
static int tvs_lasterr( void ) { return WSAGetLastError(); }
static void tvs_nonblock( int fd ) { u_long one = 1; ioctlsocket( fd, FIONBIO, &one ); }
#else
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <arpa/inet.h>
#	include <sys/ioctl.h>
#	include <errno.h>
#	include <unistd.h>
#	include <fcntl.h>
#	define TVS_INVALID_SOCKET (-1)
#	define tvs_close close
#	define tvs_wouldblock(e) ((e) == EAGAIN || (e) == EWOULDBLOCK)
#	ifdef MSG_NOSIGNAL
#		define TVS_SEND_FLAGS MSG_NOSIGNAL
#	else
#		define TVS_SEND_FLAGS 0 // macOS/BSD: rely on SO_NOSIGPIPE set per-socket
#	endif
static int tvs_lasterr( void ) { return errno; }
static void tvs_nonblock( int fd ) { int one = 1; ioctl( fd, FIONBIO, &one ); }
#endif

tvStreamState_t tvs;

static void SV_TVStream_DropConsumer( tvConsumer_t *c ) {
	if ( c->fd != TVS_INVALID_SOCKET ) {
		tvs_close( c->fd );
	}
	c->fd = TVS_INVALID_SOCKET;
	c->headerSent = qfalse;
	c->started = qfalse;
	c->outHead = c->outTail = 0;
}

// Bind the loopback listener. Called once at process init and again from every
// SV_SpawnServer, so it must be idempotent: a normal map rotation never runs
// SV_Shutdown, so the listener (and its connected consumers) is still live —
// leave it. Only the 12h SV_Restart's full SV_Shutdown closes it, after which
// the next spawn rebinds. listenFd is >0 only while a socket is open: zero-init
// is 0, SV_TVStream_Shutdown resets to TVS_INVALID_SOCKET (-1).
void SV_TVStream_Init( void ) {
	struct sockaddr_in addr;
	int fd, i, one = 1;
	int port;

	if ( tvs.listenFd > 0 ) {
		return; // already listening (re-entry on a normal map rotation)
	}

	tvs.listenFd = TVS_INVALID_SOCKET;
	for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
		tvs.consumers[i].fd = TVS_INVALID_SOCKET;
	}
	tvs.cstream = NULL;
	tvs.segActive = qfalse;

	if ( !sv_tvLive->integer ) {
		return;
	}

	port = (int)Cvar_VariableValue( "net_port" );
	if ( port <= 0 ) {
		port = 27960;
	}

	fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( fd == TVS_INVALID_SOCKET ) {
		Com_Printf( "TVStream: socket() failed: %d\n", tvs_lasterr() );
		return;
	}
	setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof( one ) );
	tvs_nonblock( fd );

	Com_Memset( &addr, 0, sizeof( addr ) );
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK ); // 127.0.0.1 only
	addr.sin_port = htons( (unsigned short)port );

	if ( bind( fd, (struct sockaddr *)&addr, sizeof( addr ) ) != 0 ) {
		Com_Printf( "TVStream: bind(127.0.0.1:%d) failed: %d\n", port, tvs_lasterr() );
		tvs_close( fd );
		return;
	}
	if ( listen( fd, MAX_TV_STREAM_CONSUMERS ) != 0 ) {
		Com_Printf( "TVStream: listen() failed: %d\n", tvs_lasterr() );
		tvs_close( fd );
		return;
	}

	tvs.listenFd = fd;
	tvs.listenPort = port;
	Com_Printf( "TVStream: listening on 127.0.0.1:%d (TCP)\n", port );
}

void SV_TVStream_Shutdown( void ) {
	int i;
	for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
		SV_TVStream_DropConsumer( &tvs.consumers[i] );
	}
	if ( tvs.listenFd != TVS_INVALID_SOCKET ) {
		tvs_close( tvs.listenFd );
		tvs.listenFd = TVS_INVALID_SOCKET;
	}
	if ( tvs.cstream ) {
		ZSTD_freeCStream( tvs.cstream );
		tvs.cstream = NULL;
	}
	tvs.segActive = qfalse;
}

// Queue bytes for a consumer. Returns qfalse (and the caller drops the
// consumer) if the bounded buffer can't hold them — gameplay never waits.
static qboolean SV_TVStream_Enqueue( tvConsumer_t *c, const void *data, int len ) {
	if ( c->outHead == c->outTail ) {
		c->outHead = c->outTail = 0;
	}
	if ( c->outTail + len > TV_STREAM_OUTBUF_SIZE ) {
		int pending = c->outTail - c->outHead;
		if ( pending + len > TV_STREAM_OUTBUF_SIZE ) {
			return qfalse; // overflow: slow consumer, drop it
		}
		memmove( c->out, c->out + c->outHead, pending );
		c->outHead = 0;
		c->outTail = pending;
	}
	Com_Memcpy( c->out + c->outTail, data, len );
	c->outTail += len;
	return qtrue;
}

// Send as much queued data as the socket will take without blocking.
// Returns qfalse on a fatal socket error (caller drops the consumer).
static qboolean SV_TVStream_PumpConsumer( tvConsumer_t *c ) {
	while ( c->outHead < c->outTail ) {
		int n = (int)send( c->fd, (const char *)( c->out + c->outHead ),
			c->outTail - c->outHead, TVS_SEND_FLAGS );
		if ( n > 0 ) {
			c->outHead += n;
			continue;
		}
		if ( n < 0 && tvs_wouldblock( tvs_lasterr() ) ) {
			return qtrue; // socket full for now; try again next frame
		}
		return qfalse; // 0 (peer closed) or real error
	}
	if ( c->outHead == c->outTail ) {
		c->outHead = c->outTail = 0;
	}
	return qtrue;
}

void SV_TVStream_RunListener( void ) {
	int i;

	if ( tvs.listenFd == TVS_INVALID_SOCKET ) {
		return;
	}

	for ( ;; ) {
		int fd = (int)accept( tvs.listenFd, NULL, NULL );
		if ( fd == TVS_INVALID_SOCKET ) {
			break; // EWOULDBLOCK (no more pending) or error: stop accepting
		}
		{
			int slot = -1, j;
			for ( j = 0; j < MAX_TV_STREAM_CONSUMERS; j++ ) {
				if ( tvs.consumers[j].fd == TVS_INVALID_SOCKET ) { slot = j; break; }
			}
			if ( slot < 0 ) {
				tvs_close( fd ); // full
				continue;
			}
			tvs_nonblock( fd );
#ifdef TCP_NODELAY
			{ int one = 1; setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof( one ) ); }
#endif
#ifdef SO_NOSIGPIPE
			{ int one = 1; setsockopt( fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&one, sizeof( one ) ); }
#endif
			tvs.consumers[slot].fd = fd;
			tvs.consumers[slot].headerSent = qfalse;
			tvs.consumers[slot].started = qfalse;
			tvs.consumers[slot].outHead = tvs.consumers[slot].outTail = 0;
		}
	}

	// Pump queued output; drop dead/slow consumers (gameplay never waits).
	for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
		tvConsumer_t *c = &tvs.consumers[i];
		if ( c->fd == TVS_INVALID_SOCKET ) continue;
		if ( !SV_TVStream_PumpConsumer( c ) ) {
			SV_TVStream_DropConsumer( c );
		}
	}
}

/*
==================
TVL1 wire-format writers

Byte-for-byte contract with the Go decoder (internal/livestream).
All multi-byte fields are little-endian.
==================
*/

static void SV_TVStream_PutU16( byte *b, unsigned short v ) { b[0] = v & 0xff; b[1] = ( v >> 8 ) & 0xff; }
static void SV_TVStream_PutU32( byte *b, unsigned int v ) {
	b[0] = v & 0xff; b[1] = ( v >> 8 ) & 0xff; b[2] = ( v >> 16 ) & 0xff; b[3] = ( v >> 24 ) & 0xff;
}

// Queue bytes to one consumer; drop it on overflow. A no-op once the
// consumer has been dropped, so paired sends (e.g. segment header + payload)
// can't re-fill a closed slot.
static void SV_TVStream_Send( tvConsumer_t *c, const void *data, int len ) {
	if ( c->fd == TVS_INVALID_SOCKET ) {
		return;
	}
	if ( !SV_TVStream_Enqueue( c, data, len ) ) {
		SV_TVStream_DropConsumer( c );
	}
}

// Build the "TVL1" StreamHeader for the current map into buf; returns length.
static int SV_TVStream_BuildHeader( byte *buf, int bufSize ) {
	const char *mapName = sv_mapname->string;
	const char *ts = ""; // timestamp is informational for the live header; empty is valid
	// fs_game in clear text so the browser can load the right paks before boot —
	// the live analog of the demo header's CS_SYSTEMINFO\fs_game, which the
	// browser can't read here because the keyframe carrying it is compressed.
	const char *gameName = Cvar_VariableString( "fs_game" );
	int mapLen = (int)strlen( mapName );
	int tsLen = (int)strlen( ts );
	int gameLen = (int)strlen( gameName );
	int p = 0;

	// "TVL1" + version u32 + svFps u32 + maxClients u32 + mapName String + ts String + fs_game String
	if ( 4 + 4 + 4 + 4 + 2 + mapLen + 2 + tsLen + 2 + gameLen > bufSize ) {
		return 0;
	}
	Com_Memcpy( buf + p, "TVL1", 4 ); p += 4;
	SV_TVStream_PutU32( buf + p, 1 ); p += 4;                          // version
	SV_TVStream_PutU32( buf + p, (unsigned)sv_fps->integer ); p += 4;  // svFps
	SV_TVStream_PutU32( buf + p, (unsigned)sv.maxclients ); p += 4;    // maxClients
	SV_TVStream_PutU16( buf + p, (unsigned short)mapLen ); p += 2;
	Com_Memcpy( buf + p, mapName, mapLen ); p += mapLen;
	SV_TVStream_PutU16( buf + p, (unsigned short)tsLen ); p += 2;
	Com_Memcpy( buf + p, ts, tsLen ); p += tsLen;
	SV_TVStream_PutU16( buf + p, (unsigned short)gameLen ); p += 2;
	Com_Memcpy( buf + p, gameName, gameLen ); p += gameLen;
	return p;
}

// Emit the just-finalized segment (tvs.segBuf[0..tvs.segLen)) to one consumer.
static void SV_TVStream_SendSegment( tvConsumer_t *c, int keyframeTime ) {
	byte hdr[12];
	Com_Memcpy( hdr, "TVLs", 4 );
	SV_TVStream_PutU32( hdr + 4, (unsigned)keyframeTime ); // i32 reinterpreted; matches Go int32(uint32)
	SV_TVStream_PutU32( hdr + 8, (unsigned)tvs.segLen );
	SV_TVStream_Send( c, hdr, 12 );
	SV_TVStream_Send( c, tvs.segBuf, tvs.segLen );
}

static void SV_TVStream_SendEnd( tvConsumer_t *c ) {
	SV_TVStream_Send( c, "TVLe", 4 );
}

/*
==================
Keyframe builder + per-segment zstd streaming

A keyframe is byte-identical to a normal .tvd frame (see SV_TV_WriteFrame),
except every entity/player is delta'd from a zeroed baseline and ALL non-empty
configstrings are dumped — so the browser's .tvd frame decoder parses it with
zero new logic. Each segment is its own independent zstd stream.
==================
*/

// Build a full-state keyframe into buf; returns the frame length, or 0 on overflow.
static int SV_TVStream_BuildKeyframe( byte *buf, int bufSize ) {
	static const entityState_t nullEntity;   // zero-initialized
	static const playerState_t nullPlayer;   // zero-initialized
	msg_t msg;
	byte entityBitmask[MAX_GENTITIES/8];
	byte playerBitmask[MAX_CLIENTS/8];
	int i, csCount;

	MSG_Init( &msg, buf, bufSize );
	MSG_Bitstream( &msg );

	MSG_WriteLong( &msg, sv.time );

	// Entities
	Com_Memset( entityBitmask, 0, sizeof( entityBitmask ) );
	for ( i = 0; i < sv.num_entities; i++ ) {
		sharedEntity_t *ent = SV_GentityNum( i );
		if ( !ent->r.linked ) {
			continue;
		}
		// Normalize s.number to the slot before emitting (see SV_TV_WriteFrame).
		if ( ent->s.number != i ) {
			ent->s.number = i;
		}
		if ( ent->r.svFlags & SVF_NOCLIENT ) {
			continue;
		}
		entityBitmask[i >> 3] |= ( 1 << ( i & 7 ) );
	}
	MSG_WriteData( &msg, entityBitmask, sizeof( entityBitmask ) );
	// force=qtrue: a flagged slot must emit its number even when it equals the null
	// baseline, else an empty linked entity (e.g. q3dm2 slot 119) is omitted and the
	// keyframe never populates it -- the reader keeps a zeroed phantom on number 0.
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		if ( !( entityBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) continue;
		MSG_WriteDeltaEntity( &msg, &nullEntity, &SV_GentityNum( i )->s, qtrue );
	}
	MSG_WriteBits( &msg, MAX_GENTITIES - 1, GENTITYNUM_BITS ); // end marker

	// Players
	Com_Memset( playerBitmask, 0, sizeof( playerBitmask ) );
	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state == CS_ACTIVE ) {
			playerBitmask[i >> 3] |= ( 1 << ( i & 7 ) );
		}
	}
	MSG_WriteData( &msg, playerBitmask, sizeof( playerBitmask ) );
	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( !( playerBitmask[i >> 3] & ( 1 << ( i & 7 ) ) ) ) continue;
		MSG_WriteByte( &msg, i );
		MSG_WriteDeltaPlayerstate( &msg, &nullPlayer, SV_GameClientNum( i ) );
	}

	// Configstrings: ALL non-empty (full state for a fresh consumer).
	csCount = 0;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( sv.configstrings[i] && sv.configstrings[i][0] ) csCount++;
	}
	MSG_WriteShort( &msg, csCount );
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		int len;
		if ( !sv.configstrings[i] || !sv.configstrings[i][0] ) continue;
		len = (int)strlen( sv.configstrings[i] );
		MSG_WriteShort( &msg, i );
		MSG_WriteShort( &msg, len );
		MSG_WriteData( &msg, sv.configstrings[i], len );
	}

	MSG_WriteShort( &msg, 0 ); // 0 server commands
#ifdef USE_VOIP
	MSG_WriteShort( &msg, 0 ); // 0 voip packets
#endif

	if ( msg.overflowed ) {
		return 0;
	}
	return msg.cursize;
}

static void SV_TVStream_BeginSegment( int keyframeTime ) {
	if ( !tvs.cstream ) {
		tvs.cstream = ZSTD_createCStream();
		ZSTD_CCtx_setParameter( tvs.cstream, ZSTD_c_compressionLevel, -3 );
	}
	ZSTD_CCtx_reset( tvs.cstream, ZSTD_reset_session_only );
	ZSTD_initCStream( tvs.cstream, -3 );
	tvs.segLen = 0;
	tvs.segUncompressed = 0;
	tvs.segKeyframeTime = keyframeTime;
	tvs.segActive = qtrue;
}

static void SV_TVStream_CompressToSegment( const void *data, int len ) {
	ZSTD_inBuffer in = { data, (size_t)len, 0 };
	while ( in.pos < in.size ) {
		ZSTD_outBuffer out = { tvs.zstdOut, TV_STREAM_ZSTD_OUT, 0 };
		size_t ret = ZSTD_compressStream2( tvs.cstream, &out, &in, ZSTD_e_continue );
		if ( ZSTD_isError( ret ) ) {
			tvs.segActive = qfalse; // compressor error; abandon (will keyframe again next tick)
			return;
		}
		if ( out.pos > 0 ) {
			if ( tvs.segLen + (int)out.pos > TV_STREAM_SEGBUF_SIZE ) {
				// Backstop: the per-frame path forces a graceful keyframe before
				// segLen gets within one frame of this cap, so this should never
				// fire in normal operation. If it does (pathological compressor
				// expansion), abandon → keyframe again next tick.
				tvs.segActive = qfalse;
				return;
			}
			Com_Memcpy( tvs.segBuf + tvs.segLen, tvs.zstdOut, out.pos );
			tvs.segLen += (int)out.pos;
		}
	}
}

// Add one [frameSize u32][frameData] record to the current segment.
static void SV_TVStream_AddFrameToSegment( const byte *frameData, int frameLen ) {
	byte sizeBuf[4];
	if ( !tvs.segActive ) return;
	SV_TVStream_PutU32( sizeBuf, (unsigned)frameLen );
	SV_TVStream_CompressToSegment( sizeBuf, 4 );
	SV_TVStream_CompressToSegment( frameData, frameLen );
	// Track the running UNCOMPRESSED size so SV_TVStream_Frame can force a
	// keyframe before a segment would exceed the client's segOut buffer.
	tvs.segUncompressed += 4 + frameLen;
}

// Flush the zstd stream end-of-frame so segBuf holds a complete, independently
// decodable zstd stream. Returns qfalse if the segment was abandoned.
static qboolean SV_TVStream_FinalizeSegment( void ) {
	size_t ret;
	ZSTD_inBuffer in = { NULL, 0, 0 };
	if ( !tvs.segActive ) return qfalse;
	do {
		ZSTD_outBuffer out = { tvs.zstdOut, TV_STREAM_ZSTD_OUT, 0 };
		ret = ZSTD_compressStream2( tvs.cstream, &out, &in, ZSTD_e_end );
		if ( ZSTD_isError( ret ) ) {
			tvs.segActive = qfalse; // compressor error; abandon segment
			return qfalse;
		}
		if ( out.pos > 0 ) {
			if ( tvs.segLen + (int)out.pos > TV_STREAM_SEGBUF_SIZE ) {
				tvs.segActive = qfalse;
				return qfalse;
			}
			Com_Memcpy( tvs.segBuf + tvs.segLen, tvs.zstdOut, out.pos );
			tvs.segLen += (int)out.pos;
		}
	} while ( ret != 0 );
	tvs.segActive = qfalse;
	return qtrue;
}

/*
==================
Stream orchestration

Tie the wire writers, keyframe builder, and per-segment compressor together,
driven by the disk recorder's lifecycle (start / per-frame / end).
==================
*/

// Begin a stream session at map go-live, decoupled from .tvd recording so warmup
// streams. The next frame keyframes and connected consumers re-receive the header.
void SV_TVStream_StartStream( void ) {
	int i;
	tvs.active = qtrue;
	tvs.forceKeyframe = qfalse;
	tvs.segActive = qfalse;
	tvs.segLen = 0;
	tvs.lastKeyframeTime = 0;
	for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
		if ( tvs.consumers[i].fd != TVS_INVALID_SOCKET ) {
			tvs.consumers[i].headerSent = qfalse;
			tvs.consumers[i].started = qfalse;
		}
	}
}

qboolean SV_TVStream_IsActive( void ) {
	return tvs.active;
}

// Request a keyframe on the next frame. Used at record-start, where the shared
// delta baseline is re-zeroed: a fresh keyframe re-bases connected consumers
// (BuildKeyframe ignores the baseline), avoiding a desync. Stream-only.
void SV_TVStream_ForceKeyframe( void ) {
	tvs.forceKeyframe = qtrue;
}

void SV_TVStream_Frame( const byte *deltaData, int deltaLen, int serverTime ) {
	int i;
	qboolean needKeyframe;

	if ( tvs.listenFd == TVS_INVALID_SOCKET ) {
		return;
	}

	for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
		tvConsumer_t *c = &tvs.consumers[i];
		if ( c->fd == TVS_INVALID_SOCKET || c->headerSent ) continue;
		{
			byte hdr[8 + MAX_QPATH + 64];
			int n = SV_TVStream_BuildHeader( hdr, sizeof( hdr ) );
			if ( n > 0 ) {
				SV_TVStream_Send( c, hdr, n );
				c->headerSent = qtrue; // becomes "started" at the next keyframe
			}
		}
	}

	// Two graceful size caps, one per buffer axis: cut a large segment by starting
	// a clean keyframe rather than via the lossy abandon-on-overflow backstop in
	// CompressToSegment. They don't reduce to one — highly-compressible state grows
	// the DECOMPRESSED size (client segOut) while compressed stays small;
	// incompressible state grows the COMPRESSED size (segBuf) first. Cut on
	// whichever is approached first; with both here the abandon is a true backstop
	// that should never fire in normal operation.
	needKeyframe = tvs.forceKeyframe || !tvs.segActive ||
		( serverTime - tvs.lastKeyframeTime >= sv_tvLiveKeyframeMsec->integer ) ||
		( tvs.segActive && tvs.segUncompressed + 4 + deltaLen > TV_SEG_UNCOMPRESSED_MAX ) ||
		( tvs.segActive && tvs.segLen > TV_STREAM_SEGBUF_SIZE - MAX_TV_MSGLEN );
	tvs.forceKeyframe = qfalse;

	if ( needKeyframe ) {
		if ( tvs.segActive && SV_TVStream_FinalizeSegment() ) {
			for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
				tvConsumer_t *c = &tvs.consumers[i];
				if ( c->fd != TVS_INVALID_SOCKET && c->started ) {
					SV_TVStream_SendSegment( c, tvs.segKeyframeTime );
				}
			}
		}
		// Header-sent consumers become eligible for the upcoming segment.
		for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
			if ( tvs.consumers[i].fd != TVS_INVALID_SOCKET && tvs.consumers[i].headerSent ) {
				tvs.consumers[i].started = qtrue;
			}
		}
		{
			int kfLen = SV_TVStream_BuildKeyframe( tvs.frameBuf, sizeof( tvs.frameBuf ) );
			if ( kfLen > 0 ) {
				SV_TVStream_BeginSegment( serverTime );
				SV_TVStream_AddFrameToSegment( tvs.frameBuf, kfLen );
				tvs.lastKeyframeTime = serverTime;
			} else {
				// The keyframe overflowed MAX_TV_MSGLEN, so no segment can start
				// and every subsequent frame would retry this silently — a black
				// live feed with no operator signal. Warn, rate-limited.
				static int lastOverflowWarn;
				if ( lastOverflowWarn == 0 || serverTime - lastOverflowWarn >= 5000 ) {
					Com_Printf( S_COLOR_YELLOW "TVStream: keyframe exceeds %d bytes; live stalled until server state shrinks\n", MAX_TV_MSGLEN );
					lastOverflowWarn = serverTime;
				}
			}
		}
	} else {
		// Append the delta frame the disk recorder already built.
		SV_TVStream_AddFrameToSegment( deltaData, deltaLen );
	}
}

// End a stream session at map rotation, decoupled from .tvd recording. Consumers
// stay connected for the next match.
void SV_TVStream_EndStream( void ) {
	int i;
	if ( tvs.segActive && SV_TVStream_FinalizeSegment() ) {
		for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
			tvConsumer_t *c = &tvs.consumers[i];
			if ( c->fd != TVS_INVALID_SOCKET && c->started ) {
				SV_TVStream_SendSegment( c, tvs.segKeyframeTime );
			}
		}
	}
	for ( i = 0; i < MAX_TV_STREAM_CONSUMERS; i++ ) {
		tvConsumer_t *c = &tvs.consumers[i];
		if ( c->fd != TVS_INVALID_SOCKET && c->headerSent ) {
			SV_TVStream_SendEnd( c );
		}
		tvs.consumers[i].started = qfalse;
		tvs.consumers[i].headerSent = qfalse;
	}
	tvs.segActive = qfalse;
	tvs.active = qfalse;
}
