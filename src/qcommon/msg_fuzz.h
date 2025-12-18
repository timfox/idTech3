/*
===========================================================================
msg_fuzz.h - Minimal message definitions for fuzzing builds

This header provides only the absolutely necessary definitions from msg.h
for standalone fuzzing targets, avoiding engine-wide dependencies.
===========================================================================
*/

#ifndef __MSG_FUZZ_H__
#define __MSG_FUZZ_H__

#include "qcommon_fuzz.h"

// Max message length (must match msg.h)
#define MAX_MSGLEN              0x4000  // 16384

// Function prototypes used by fuzz_msg.c
extern void MSG_Init( msg_t *buf, byte *data, int length );
extern void MSG_BeginReading( msg_t *msg );
extern int MSG_ReadByte( msg_t *msg );
extern int MSG_ReadShort( msg_t *msg );
extern int MSG_ReadLong( msg_t *msg );
extern float MSG_ReadFloat( msg_t *msg );
extern const char *MSG_ReadStringLine( msg_t *msg );
extern void MSG_ReadData( msg_t *msg, void *buffer, int size );
extern int MSG_ReadBits( msg_t *msg, int bits );

#endif // __MSG_FUZZ_H__
