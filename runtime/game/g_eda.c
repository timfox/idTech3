/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

In-process event bus: register channel names, publish string payloads,
drain in EDA_Frame (or EDA_Pop) for decoupled systems (AIML, ECS, Lua).
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "g_eda.h"
#include "g_engine_systems.h"
#include <string.h>

typedef struct {
	char name[EDA_MAX_NAME];
	qboolean used;
} edaChannel_t;

typedef struct {
	int ch; /* index into s_channels, or -1 if invalid */
	char payload[EDA_MAX_PAYLOAD];
} edaEvent_t;

static edaChannel_t s_channels[EDA_MAX_CHANNELS];
static int s_numChannels;
static edaEvent_t s_queue[EDA_MAX_QUEUE];
static int s_qHead; /* read */
static int s_qTail; /* next write */
static int s_qCount;
static cvar_t *g_eda;
static cvar_t *g_edaLog;

qboolean EDA_IsEnabled( void ) {
	return ( g_eda && g_eda->integer ) ? qtrue : qfalse;
}

void EDA_Init( void ) {
	Com_Memset( s_channels, 0, sizeof( s_channels ) );
	s_numChannels = 0;
	Com_Memset( s_queue, 0, sizeof( s_queue ) );
	s_qHead = 0;
	s_qTail = 0;
	s_qCount = 0;
	g_eda = Cvar_Get( "g_eda", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( g_eda, "Enable in-process event bus (EDA: Engine.Events, EDA_Publish/Pop)." );
	g_edaLog = Cvar_Get( "g_edaLog", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( g_edaLog, "EDA: log every publish to console (0=off, 1=on)." );
	Com_Printf( "EDA: event bus ready (cvar g_eda=%d, g_edaLog=%d, max queue %d)\n",
		g_eda ? g_eda->integer : 1, g_edaLog ? g_edaLog->integer : 0, EDA_MAX_QUEUE );
}

void EDA_Shutdown( void ) {
	EDA_Clear();
	s_numChannels = 0;
	Com_Memset( s_channels, 0, sizeof( s_channels ) );
}

static int EDA_ChannelIndex( const char *name ) {
	int i;
	if ( !name || !name[0] ) {
		return -1;
	}
	for ( i = 0; i < s_numChannels; i++ ) {
		if ( s_channels[i].used && !Q_stricmp( s_channels[i].name, name ) ) {
			return i;
		}
	}
	return -1;
}

qboolean EDA_RegisterChannel( const char *name ) {
	if ( !g_eda || !g_eda->integer ) {
		return qfalse;
	}
	if ( !name || !name[0] ) {
		return qfalse;
	}
	if ( EDA_ChannelIndex( name ) >= 0 ) {
		return qtrue;
	}
	if ( s_numChannels >= EDA_MAX_CHANNELS ) {
		Com_Printf( S_COLOR_YELLOW "EDA: max channels %d\n", EDA_MAX_CHANNELS );
		return qfalse;
	}
	Q_strncpyz( s_channels[s_numChannels].name, name, sizeof( s_channels[0].name ) );
	s_channels[s_numChannels].used = qtrue;
	s_numChannels++;
	return qtrue;
}

qboolean EDA_Publish( const char *channel, const char *payload ) {
	int idx;
	if ( !g_eda || !g_eda->integer ) {
		return qfalse;
	}
	if ( !channel || !channel[0] ) {
		return qfalse;
	}
	idx = EDA_ChannelIndex( channel );
	if ( idx < 0 ) {
		if ( !EDA_RegisterChannel( channel ) ) {
			return qfalse;
		}
		idx = EDA_ChannelIndex( channel );
	}
	if ( idx < 0 ) {
		return qfalse;
	}
	if ( s_qCount >= EDA_MAX_QUEUE ) {
		Com_DPrintf( "EDA: queue full, drop %s\n", channel );
		return qfalse;
	}
	if ( g_edaLog && g_edaLog->integer ) {
		Com_Printf( "EDA: publish %s = %.200s\n", channel, payload && payload[0] ? payload : "" );
	}
	s_queue[s_qTail].ch = idx;
	if ( payload && payload[0] ) {
		Q_strncpyz( s_queue[s_qTail].payload, payload, sizeof( s_queue[0].payload ) );
	} else {
		s_queue[s_qTail].payload[0] = '\0';
	}
	s_qTail = ( s_qTail + 1 ) % EDA_MAX_QUEUE;
	s_qCount++;
	return qtrue;
}

qboolean EDA_Pop( char *channelOut, int channelLen, char *payloadOut, int payloadLen ) {
	edaEvent_t *ev;
	if ( s_qCount <= 0 || s_qHead == s_qTail ) {
		return qfalse;
	}
	ev = &s_queue[s_qHead];
	if ( ev->ch < 0 || ev->ch >= s_numChannels || !s_channels[ev->ch].used ) {
		s_qHead = ( s_qHead + 1 ) % EDA_MAX_QUEUE;
		s_qCount--;
		return EDA_Pop( channelOut, channelLen, payloadOut, payloadLen );
	}
	if ( channelOut && channelLen > 0 ) {
		Q_strncpyz( channelOut, s_channels[ev->ch].name, channelLen );
	}
	if ( payloadOut && payloadLen > 0 ) {
		Q_strncpyz( payloadOut, ev->payload, payloadLen );
	}
	s_qHead = ( s_qHead + 1 ) % EDA_MAX_QUEUE;
	s_qCount--;
	return qtrue;
}

qboolean EDA_Peek( char *channelOut, int channelLen, char *payloadOut, int payloadLen ) {
	edaEvent_t *ev;
	if ( s_qCount <= 0 || s_qHead == s_qTail ) {
		return qfalse;
	}
	ev = &s_queue[s_qHead];
	if ( ev->ch < 0 || ev->ch >= s_numChannels || !s_channels[ev->ch].used ) {
		return qfalse;
	}
	if ( channelOut && channelLen > 0 ) {
		Q_strncpyz( channelOut, s_channels[ev->ch].name, channelLen );
	}
	if ( payloadOut && payloadLen > 0 ) {
		Q_strncpyz( payloadOut, ev->payload, payloadLen );
	}
	return qtrue;
}

int EDA_QueueDepth( void ) {
	return s_qCount;
}

void EDA_Clear( void ) {
	s_qHead = 0;
	s_qTail = 0;
	s_qCount = 0;
	Com_Memset( s_queue, 0, sizeof( s_queue ) );
}

int EDA_Drain( edaEventRecord_t *out, int maxOut ) {
	int n;
	if ( !out || maxOut <= 0 ) {
		return 0;
	}
	for ( n = 0; n < maxOut; n++ ) {
		if ( !EDA_Pop( out[n].channel, (int)sizeof( out[n].channel ), out[n].payload, (int)sizeof( out[n].payload ) ) ) {
			break;
		}
	}
	return n;
}

void EDA_Frame( void ) {
	if ( g_eda && g_eda->integer && s_qCount > 0 ) {
		EngineTelemetry_Record( "eda_queue_depth", (double)s_qCount );
	}
}
