/*
===========================================================================
msg_fuzz.c - Minimal message implementation for fuzzing builds

This file provides a stripped-down implementation of message functions
sufficient for the fuzzing harness, avoiding full engine dependencies.
===========================================================================
*/

#include "msg_fuzz.h"
#include <string.h> // For memset, memcpy

// Mock Sys_Milliseconds needed by msg.c
long long Sys_Milliseconds( void ) {
    return 0; // Return a constant value for fuzzing
}

// These are minimal implementations, potentially simplified from original msg.c

// Shared state for reading bits (from msg.c)
static int      msg_readbit = 0;
static int      msg_readbyte = 0;

void MSG_Init( msg_t *buf, byte *data, int length ) {
    memset(buf, 0, sizeof(*buf));
    buf->data = data;
    buf->maxsize = length;
}

void MSG_BeginReading( msg_t *msg ) {
    msg->readcount = 0;
    msg->bit = 0;
    msg->allowoverflow = qfalse;
    msg->overflowed = qfalse;
}

// Simplified MSG_ReadBits (from msg.c)
int MSG_ReadBits( msg_t *msg, int bits ) {
    int     value;
    int     get;
    int     i;

    if (msg->readcount + bits > msg->cursize * 8) {
        // Com_Error(ERR_DROP, "MSG_ReadBits: read beyond end of message");
        msg->readcount = msg->cursize * 8; // Advance readcount to end
        msg->overflowed = qtrue;
        return -1; // Indicate error
    }

    value = 0;
    for (i = 0; i < bits; i++) {
        get = (msg->data[msg->readcount >> 3] >> (msg->readcount & 7)) & 1;
        value |= (get << i);
        msg->readcount++;
    }

    return value;
}

int MSG_ReadByte( msg_t *msg ) {
    return MSG_ReadBits(msg, 8);
}

int MSG_ReadShort( msg_t *msg ) {
    return (short)MSG_ReadBits(msg, 16);
}

int MSG_ReadLong( msg_t *msg ) {
    return MSG_ReadBits(msg, 32);
}

float MSG_ReadFloat( msg_t *msg ) {
    union {
        int i;
        float f;
    } conv;
    conv.i = MSG_ReadLong(msg);
    return conv.f;
}

const char *MSG_ReadStringLine( msg_t *msg ) {
    static char string[2048]; // Max string length for line
    int         l;
    int         c;

    l = 0;
    while (1) {
        c = MSG_ReadByte(msg);
        if (c == -1) { // Check for overflow/end of message
            string[l] = 0;
            return string;
        }

        if (c == '\n') {
            string[l] = 0;
            return string;
        }

        if (c == '\r') {
            string[l] = 0;
            return string;
        }
        
        if (l < sizeof(string) - 1) {
            string[l++] = c;
        }
    }
    return string;
}

void MSG_ReadData( msg_t *msg, void *buffer, int size ) {
    int     i;
    byte    *buf = (byte *)buffer;

    if (msg->readcount + (size * 8) > msg->cursize * 8) {
        // Com_Error(ERR_DROP, "MSG_ReadData: read beyond end of message");
        msg->readcount = msg->cursize * 8; // Advance readcount to end
        msg->overflowed = qtrue;
        memset(buffer, 0, size); // Zero out buffer to avoid uninitialized data
        return;
    }

    for (i = 0; i < size; i++) {
        buf[i] = MSG_ReadByte(msg);
    }
}
