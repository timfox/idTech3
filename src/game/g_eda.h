/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Lightweight in-process event bus (event-driven architecture) for game
systems. String-typed channels, fixed queue of string payloads. Optional
Lua is served by Engine.Events in g_lua_bindings.c.
===========================================================================
*/

#ifndef G_EDA_H
#define G_EDA_H

#include "../qcommon/q_shared.h"

#define EDA_MAX_CHANNELS   32
#define EDA_MAX_QUEUE      256
#define EDA_MAX_NAME       48
#define EDA_MAX_PAYLOAD    256

typedef struct {
	char channel[EDA_MAX_NAME];
	char payload[EDA_MAX_PAYLOAD];
} edaEventRecord_t;

void    EDA_Init( void );
void    EDA_Shutdown( void );
void    EDA_Frame( void );

qboolean EDA_IsEnabled( void );

/* Returns qfalse if channel table is full. */
qboolean EDA_RegisterChannel( const char *name );

/* Pushes a copy of payload (truncated to EDA_MAX_PAYLOAD-1). qfalse if queue full. */
qboolean EDA_Publish( const char *channel, const char *payload );

/* Pop one event: returns qtrue and writes channel name + payload. */
qboolean EDA_Pop( char *channelOut, int channelLen, char *payloadOut, int payloadLen );

/* Inspect head of queue without consuming (for Lua/debug). */
qboolean EDA_Peek( char *channelOut, int channelLen, char *payloadOut, int payloadLen );

int     EDA_QueueDepth( void );
void    EDA_Clear( void );

/* Drain up to maxOut events into out[0..]; returns count written. */
int     EDA_Drain( edaEventRecord_t *out, int maxOut );

#endif /* G_EDA_H */
