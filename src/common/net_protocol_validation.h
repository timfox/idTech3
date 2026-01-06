/*
=============================================================================
Network Protocol Validation Header

Comprehensive network protocol validation with bounds checking and security
=============================================================================
*/

#ifndef __NET_PROTOCOL_VALIDATION_H__
#define __NET_PROTOCOL_VALIDATION_H__

#include "q_shared.h"

// Protocol validation result codes
typedef enum {
    NET_PROTOCOL_VALID = 0,
    NET_PROTOCOL_INVALID_LENGTH,
    NET_PROTOCOL_INVALID_HEADER,
    NET_PROTOCOL_INVALID_SEQUENCE,
    NET_PROTOCOL_INVALID_COMMAND,
    NET_PROTOCOL_BUFFER_OVERFLOW,
    NET_PROTOCOL_CORRUPTION_DETECTED,
    NET_PROTOCOL_RATE_LIMIT_EXCEEDED,
    NET_PROTOCOL_INVALID_PAYLOAD,
    NET_PROTOCOL_MALFORMED_PACKET,
    NET_PROTOCOL_SIZE_MISMATCH
} net_protocol_result_t;

// Packet validation statistics
typedef struct {
    atomic_int_t total_packets_validated;
    atomic_int_t packets_rejected;
    atomic_int_t buffer_overflow_attempts;
    atomic_int_t corruption_detected;
    atomic_int_t rate_limit_hits;
    atomic_int_t invalid_sequences;
    atomic_int_t size_mismatches;
} net_validation_stats_t;

// Network protocol validation configuration
typedef struct {
    qboolean enable_validation;
    qboolean strict_mode;
    int max_packet_rate;      // packets per second
    int max_packet_size;      // maximum allowed packet size
    int min_packet_size;      // minimum allowed packet size
    qboolean validate_sequences;
    qboolean detect_corruption;
    qboolean enable_rate_limiting;
} net_validation_config_t;

// Protocol validation context
typedef struct {
    net_validation_stats_t stats;
    net_validation_config_t config;

    // Rate limiting
    int last_packet_time;
    int packet_count_this_second;

    // Sequence tracking
    int last_sequence_received;
    int expected_sequence;
} net_validation_context_t;

// Core validation functions
qboolean Net_ValidatePacketBounds(const void *packet_data, int packet_length, int max_allowed_size);
net_protocol_result_t Net_ValidateProtocolHeader(const byte *data, int length);
net_protocol_result_t Net_ValidateMessageIntegrity(msg_t *msg);
net_protocol_result_t Net_ValidateCommandData(int command, const byte *data, int length);
qboolean Net_CheckRateLimit(net_validation_context_t *ctx, int current_time);

// Enhanced validation functions
net_protocol_result_t Net_ValidateFragmentationData(const byte *data, int length, int fragment_size);
net_protocol_result_t Net_ValidateConnectionHandshake(const byte *data, int length);
net_protocol_result_t Net_ValidateGameStateData(const byte *data, int length);

// Bounds checking functions
qboolean Net_BoundsCheckArray(const void *array, int element_size, int element_count, int max_elements);
qboolean Net_BoundsCheckString(const char *str, int max_length);
qboolean Net_BoundsCheckPointer(const void *ptr, const void *buffer_start, int buffer_size);

// Protocol-specific validation
net_protocol_result_t Net_ValidateServerInfo(const byte *data, int length);
net_protocol_result_t Net_ValidateClientCommand(const byte *data, int length);
net_protocol_result_t Net_ValidateEntityState(const byte *data, int length);
net_protocol_result_t Net_ValidatePlayerState(const byte *data, int length);

// Security validation
qboolean Net_DetectBufferOverflow(const byte *buffer, int buffer_size, int write_offset, int write_size);
qboolean Net_ValidateMemoryAccess(const void *ptr, size_t access_size, const void *valid_region_start, size_t valid_region_size);

// Context management
void Net_ValidationInit(net_validation_context_t *ctx);
void Net_ValidationShutdown(net_validation_context_t *ctx);
void Net_ValidationResetStats(net_validation_context_t *ctx);

// Configuration
void Net_ValidationSetConfig(net_validation_context_t *ctx, const net_validation_config_t *config);
void Net_ValidationGetStats(const net_validation_context_t *ctx, net_validation_stats_t *stats);

// Utility functions
const char *Net_ProtocolResultToString(net_protocol_result_t result);
qboolean Net_IsCriticalProtocolError(net_protocol_result_t result);

#endif // __NET_PROTOCOL_VALIDATION_H__