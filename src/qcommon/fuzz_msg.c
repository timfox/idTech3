/*
===========================================================================
Network Message Fuzzing Harness

Feeds fuzzed data to network message parsing functions to detect crashes,
memory errors, and other vulnerabilities.
===========================================================================
*/

#include "q_shared_fuzz.h"
#include "qcommon_fuzz.h"
#include "msg_fuzz.h"
#include <string.h> // For memcpy

// Minimal Com_Error / Com_Printf for fuzzing context
void Com_Error(errorParm_t level, const char *format, ...) {
    (void)level;
    (void)format;
    // In fuzzing, we don't want to exit, just let the fuzzer catch the crash
    // _exit(1) or similar if you want to explicitly mark an error as a crash
}

void Com_Printf(const char *fmt, ...) {
    (void)fmt;
    // Suppress output during fuzzing
}

// Fuzzing entry point for libFuzzer
extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    msg_t msg;
    byte buffer[MAX_MSGLEN];

    // Initialize message buffer with fuzzed data
    if (Size > MAX_MSGLEN) {
        Size = MAX_MSGLEN;
    }
    MSG_Init(&msg, buffer, (int)Size);
    memcpy(msg.data, Data, Size);
    msg.cursize = (int)Size;
    MSG_BeginReading(&msg);

    // Attempt to read various message types
    while (msg.readcount < msg.cursize * 8) {
        // Read a byte, a short, a long, a float
        MSG_ReadByte(&msg);
        if (msg.readcount + 16 <= msg.cursize * 8) {
            MSG_ReadShort(&msg);
        }
        if (msg.readcount + 32 <= msg.cursize * 8) {
            MSG_ReadLong(&msg);
        }
        if (msg.readcount + 32 <= msg.cursize * 8) {
            MSG_ReadFloat(&msg);
        }

        // Read a string (with a reasonable max length to avoid excessive memory allocation)
        if (msg.readcount < msg.cursize * 8) {
            char temp_string[256];
            MSG_ReadStringLine(&msg);
        }

        // Read data block
        if (msg.readcount + 64 <= msg.cursize * 8) {
            byte temp_data[8];
            MSG_ReadData(&msg, temp_data, sizeof(temp_data));
        }

        // Introduce some variability in reading bits
        if (msg.readcount + 1 <= msg.cursize * 8) {
            MSG_ReadBits(&msg, 1);
        }
    }

    return 0;
}

