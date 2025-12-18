/*
===========================================================================
Secure Message Functions

Enhanced network message handling with comprehensive bounds checking.
===========================================================================
*/

#ifndef __MSG_SECURE_H__
#define __MSG_SECURE_H__

#include "q_shared.h"
#include "qcommon.h"

// Enhanced MSG_WriteBits with bounds checking
void MSG_WriteBits_Secure(msg_t *msg, int value, int bits, const char *context);

// Enhanced MSG_ReadBits with bounds checking
int MSG_ReadBits_Secure(msg_t *msg, int bits, const char *context);

// Enhanced MSG_ReadString with bounds checking and dynamic buffer
qboolean MSG_ReadString_Secure(msg_t *msg, char *buffer, size_t buffer_size, const char *context);

// Enhanced MSG_ReadData with bounds checking
qboolean MSG_ReadData_Secure(msg_t *msg, void *data, size_t length, const char *context);

// Enhanced MSG_WriteData with bounds checking
void MSG_WriteData_Secure(msg_t *msg, const void *data, size_t length, const char *context);

// Check if message has enough space for operation
qboolean MSG_HasSpace(const msg_t *msg, size_t bytes_needed, const char *context);

// Check if message has enough bits for operation
qboolean MSG_HasBits(const msg_t *msg, int bits_needed, const char *context);

// Enhanced MSG_WriteString with bounds checking
void MSG_WriteString_Secure(msg_t *msg, const char *string, const char *context);

// Validate message state
qboolean MSG_ValidateState(const msg_t *msg, const char *context);

#endif // __MSG_SECURE_H__