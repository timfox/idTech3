/*
===========================================================================
Secure Message Functions Implementation

Enhanced network message handling with comprehensive bounds checking.
===========================================================================
*/

#include "msg_secure.h"
#include "msg.h"  // Include the original msg.h for Huffman functions

/*
=================
MSG_HasSpace

Check if message has enough space for the requested operation
=================
*/
qboolean MSG_HasSpace(const msg_t *msg, size_t bytes_needed, const char *context)
{
    if (!msg) {
        Com_Printf("ERROR: MSG_HasSpace - NULL message in %s\n", context ? context : "unknown");
        return qfalse;
    }

    if (msg->overflowed) {
        Com_Printf("WARNING: MSG_HasSpace - message already overflowed in %s\n", context ? context : "unknown");
        return qfalse;
    }

    size_t available_bytes = msg->maxsize - msg->cursize;
    if (bytes_needed > available_bytes) {
        Com_Printf("ERROR: MSG_HasSpace - insufficient space (%zu needed, %zu available) in %s\n",
                  bytes_needed, available_bytes, context ? context : "unknown");
        return qfalse;
    }

    return qtrue;
}

/*
=================
MSG_HasBits

Check if message has enough bits for the requested operation
=================
*/
qboolean MSG_HasBits(const msg_t *msg, int bits_needed, const char *context)
{
    if (!msg) {
        Com_Printf("ERROR: MSG_HasBits - NULL message in %s\n", context ? context : "unknown");
        return qfalse;
    }

    if (bits_needed < 0 || bits_needed > 32) {
        Com_Printf("ERROR: MSG_HasBits - invalid bits count %d in %s\n",
                  bits_needed, context ? context : "unknown");
        return qfalse;
    }

    if (msg->overflowed) {
        Com_Printf("WARNING: MSG_HasBits - message already overflowed in %s\n", context ? context : "unknown");
        return qfalse;
    }

    int available_bits = msg->maxbits - msg->bit;
    if (bits_needed > available_bits) {
        Com_Printf("ERROR: MSG_HasBits - insufficient bits (%d needed, %d available) in %s\n",
                  bits_needed, available_bits, context ? context : "unknown");
        return qfalse;
    }

    return qtrue;
}

/*
=================
MSG_ValidateState

Validate overall message state
=================
*/
qboolean MSG_ValidateState(const msg_t *msg, const char *context)
{
    if (!msg) {
        Com_Printf("ERROR: MSG_ValidateState - NULL message in %s\n", context ? context : "unknown");
        return qfalse;
    }

    if (msg->cursize > msg->maxsize) {
        Com_Printf("ERROR: MSG_ValidateState - cursize > maxsize (%d > %d) in %s\n",
                  msg->cursize, msg->maxsize, context ? context : "unknown");
        return qfalse;
    }

    if (msg->bit > msg->maxbits) {
        Com_Printf("ERROR: MSG_ValidateState - bit > maxbits (%d > %d) in %s\n",
                  msg->bit, msg->maxbits, context ? context : "unknown");
        return qfalse;
    }

    if (msg->readcount > msg->cursize) {
        Com_Printf("ERROR: MSG_ValidateState - readcount > cursize (%d > %d) in %s\n",
                  msg->readcount, msg->cursize, context ? context : "unknown");
        return qfalse;
    }

    return qtrue;
}

/*
=================
MSG_WriteBits_Secure

Enhanced MSG_WriteBits with bounds checking
=================
*/
void MSG_WriteBits_Secure(msg_t *msg, int value, int bits, const char *context)
{
    if (!MSG_ValidateState(msg, context)) {
        return;
    }

    if (bits == 0 || bits < -31 || bits > 32) {
        Com_Printf("ERROR: MSG_WriteBits_Secure - bad bits %d in %s\n",
                  bits, context ? context : "unknown");
        return;
    }

    // For OOB messages, check byte-level space
    if (msg->oob) {
        int bytes_needed = 0;
        if (bits == 8) bytes_needed = 1;
        else if (bits == 16) bytes_needed = 2;
        else if (bits == 32) bytes_needed = 4;
        else {
            Com_Printf("ERROR: MSG_WriteBits_Secure - unsupported OOB bits %d in %s\n",
                      bits, context ? context : "unknown");
            return;
        }

        if (!MSG_HasSpace(msg, bytes_needed, context)) {
            return;
        }
    } else {
        // For compressed messages, check bit-level space
        if (!MSG_HasBits(msg, bits < 0 ? -bits : bits, context)) {
            return;
        }
    }

    // Call the original function
    MSG_WriteBits(msg, value, bits);
}

/*
=================
MSG_ReadBits_Secure

Enhanced MSG_ReadBits with bounds checking
=================
*/
int MSG_ReadBits_Secure(msg_t *msg, int bits, const char *context)
{
    if (!MSG_ValidateState(msg, context)) {
        return 0;
    }

    if (bits == 0 || bits < -31 || bits > 32) {
        Com_Printf("ERROR: MSG_ReadBits_Secure - bad bits %d in %s\n",
                  bits, context ? context : "unknown");
        return 0;
    }

    // Check if we have enough bits to read
    int bits_to_read = bits < 0 ? -bits : bits;
    if (!MSG_HasBits(msg, bits_to_read, context)) {
        return 0;
    }

    // Call the original function
    return MSG_ReadBits(msg, bits);
}

/*
=================
MSG_ReadString_Secure

Enhanced MSG_ReadString with bounds checking and dynamic buffer
=================
*/
qboolean MSG_ReadString_Secure(msg_t *msg, char *buffer, size_t buffer_size, const char *context)
{
    if (!MSG_ValidateState(msg, context)) {
        return qfalse;
    }

    if (!buffer || buffer_size < 1) {
        Com_Printf("ERROR: MSG_ReadString_Secure - invalid buffer (size=%zu) in %s\n",
                  buffer_size, context ? context : "unknown");
        return qfalse;
    }

    size_t i = 0;
    int c;

    // Read characters one by one with bounds checking
    while (i < buffer_size - 1) {
        c = MSG_ReadByte(msg);

        if (c <= 0) {  // End of string or error
            break;
        }

        // Translate format specifiers to prevent crashes
        if (c == '%') {
            c = '.';
        }
        // Don't allow higher ASCII values
        if (c > 127) {
            c = '.';
        }

        buffer[i++] = (char)c;
    }

    buffer[i] = '\0';  // Null terminate

    // Check if we truncated the string
    if (c > 0) {
        Com_Printf("WARNING: MSG_ReadString_Secure - string truncated (buffer size %zu) in %s\n",
                  buffer_size, context ? context : "unknown");
    }

    return qtrue;
}

/*
=================
MSG_ReadData_Secure

Enhanced MSG_ReadData with bounds checking
=================
*/
qboolean MSG_ReadData_Secure(msg_t *msg, void *data, size_t length, const char *context)
{
    if (!MSG_ValidateState(msg, context)) {
        return qfalse;
    }

    if (!data) {
        Com_Printf("ERROR: MSG_ReadData_Secure - NULL data pointer in %s\n",
                  context ? context : "unknown");
        return qfalse;
    }

    if (length == 0) {
        return qtrue;  // Nothing to read
    }

    // Check if we have enough bytes to read
    if (!MSG_HasSpace(msg, length, context)) {
        return qfalse;
    }

    // Use the original MSG_ReadData
    MSG_ReadData(msg, data, length);
    return qtrue;
}

/*
=================
MSG_WriteData_Secure

Enhanced MSG_WriteData with bounds checking
=================
*/
void MSG_WriteData_Secure(msg_t *msg, const void *data, size_t length, const char *context)
{
    if (!MSG_ValidateState(msg, context)) {
        return;
    }

    if (!data && length > 0) {
        Com_Printf("ERROR: MSG_WriteData_Secure - NULL data pointer with length %zu in %s\n",
                  length, context ? context : "unknown");
        return;
    }

    if (length == 0) {
        return;  // Nothing to write
    }

    // Check if we have enough space to write
    if (!MSG_HasSpace(msg, length, context)) {
        return;
    }

    // Use the original MSG_WriteData
    MSG_WriteData(msg, data, length);
}

/*
=================
MSG_WriteString_Secure

Enhanced MSG_WriteString with bounds checking
=================
*/
void MSG_WriteString_Secure(msg_t *msg, const char *string, const char *context)
{
    if (!MSG_ValidateState(msg, context)) {
        return;
    }

    if (!string) {
        Com_Printf("ERROR: MSG_WriteString_Secure - NULL string in %s\n",
                  context ? context : "unknown");
        return;
    }

    size_t len = strlen(string);

    // Check space for string + null terminator
    if (!MSG_HasSpace(msg, len + 1, context)) {
        return;
    }

    // Use the original MSG_WriteString
    MSG_WriteString(msg, string);
}