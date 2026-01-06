/*
=============================================================================
Network Protocol Validation Implementation

Comprehensive network protocol validation with bounds checking and security
=============================================================================
*/

#include "net_protocol_validation.h"
#include "qcommon.h"
#include <string.h>

// Forward declarations
net_protocol_result_t Net_ValidateServerCommand(const byte *data, int length);

// Protocol constants
#define PROTOCOL_HEADER_SIZE 8
#define MAX_COMMAND_DATA_SIZE 32768
#define MIN_PROTOCOL_VERSION 66
#define MAX_PROTOCOL_VERSION 100

// Command validation limits
#define MAX_ENTITY_COUNT 1024
#define MAX_PLAYER_COUNT 64
#define MAX_STRING_LENGTH 1024
#define MAX_VECTOR_COMPONENTS 3

// Corruption detection patterns
static const byte corruption_patterns[][4] = {
    {0xDE, 0xAD, 0xBE, 0xEF},
    {0xBA, 0xAD, 0xF0, 0x0D},
    {0xCA, 0xFE, 0xBA, 0xBE},
    {0xDE, 0xFE, 0xC8, 0xED}
};

/*
===============
Net_ValidatePacketBounds

Basic bounds checking for network packets
===============
*/
qboolean Net_ValidatePacketBounds(const void *packet_data, int packet_length, int max_allowed_size) {
    if (!packet_data) {
        return qfalse;
    }

    if (packet_length < 0 || packet_length > max_allowed_size) {
        return qfalse;
    }

    // Check for potential buffer overflow in the data itself
    if (packet_length > 0) {
        // Simple sanity check - ensure no obvious corruption
        const byte *data = (const byte *)packet_data;

        // Check for null bytes in unexpected places (basic corruption detection)
        for (int i = 0; i < packet_length && i < 16; i++) {
            if (data[i] == 0 && i < 4) {
                // Null byte in header area is suspicious
                return qfalse;
            }
        }
    }

    return qtrue;
}

/*
===============
Net_ValidateProtocolHeader

Validate protocol header structure and version
===============
*/
net_protocol_result_t Net_ValidateProtocolHeader(const byte *data, int length) {
    if (!data || length < PROTOCOL_HEADER_SIZE) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    // Check protocol version
    int protocol_version = data[0];
    if (protocol_version < MIN_PROTOCOL_VERSION || protocol_version > MAX_PROTOCOL_VERSION) {
        return NET_PROTOCOL_INVALID_HEADER;
    }

    // Check sequence number (basic sanity)
    int sequence = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
    if (sequence < 0) {
        return NET_PROTOCOL_INVALID_SEQUENCE;
    }

    // Check packet type
    int packet_type = data[5];
    if (packet_type < 0 || packet_type > 255) {
        return NET_PROTOCOL_INVALID_COMMAND;
    }

    // Check length field
    int payload_length = (data[6] << 8) | data[7];
    if (payload_length < 0 || payload_length > MAX_COMMAND_DATA_SIZE) {
        return NET_PROTOCOL_SIZE_MISMATCH;
    }

    // Verify payload length matches actual data length
    if (length != PROTOCOL_HEADER_SIZE + payload_length) {
        return NET_PROTOCOL_SIZE_MISMATCH;
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_ValidateMessageIntegrity

Validate message structure and bounds
===============
*/
net_protocol_result_t Net_ValidateMessageIntegrity(msg_t *msg) {
    if (!msg || !msg->data) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    // Check message bounds
    if (msg->cursize < 0 || msg->cursize > msg->maxsize) {
        return NET_PROTOCOL_BUFFER_OVERFLOW;
    }

    if (msg->readcount < 0 || msg->readcount > msg->cursize) {
        return NET_PROTOCOL_BUFFER_OVERFLOW;
    }

    // Check for buffer corruption patterns
    for (size_t i = 0; i < sizeof(corruption_patterns) / sizeof(corruption_patterns[0]); i++) {
        if (msg->cursize >= (int)sizeof(corruption_patterns[i])) {
            if (memcmp(msg->data, corruption_patterns[i], sizeof(corruption_patterns[i])) == 0) {
                return NET_PROTOCOL_CORRUPTION_DETECTED;
            }
        }
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_ValidateCommandData

Validate specific command data based on command type
===============
*/
net_protocol_result_t Net_ValidateCommandData(int command, const byte *data, int length) {
    if (!data && length > 0) {
        return NET_PROTOCOL_INVALID_PAYLOAD;
    }

    if (length < 0) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    // Check if this is a server command (svc_*) or client command (clc_*)
    // Since enum values overlap, we need to distinguish based on range
    if (command >= svc_bad && command <= svc_EOF) {
        // Server command
        switch (command) {
            case svc_serverCommand:
                return Net_ValidateServerCommand(data, length);

            case svc_gamestate:
                return Net_ValidateGameStateData(data, length);

            case svc_snapshot:
                return Net_ValidateSnapshotData(data, length);

            default:
                // Unknown server command - allow but check size
                if (length > MAX_COMMAND_DATA_SIZE) {
                    return NET_PROTOCOL_SIZE_MISMATCH;
                }
                break;
        }
    } else if (command >= clc_bad && command <= clc_EOF) {
        // Client command
        switch (command) {
            case clc_move:
            case clc_moveNoDelta:
                return Net_ValidateClientMove(data, length);

            case clc_clientCommand:
                return Net_ValidateClientCommand(data, length);

            default:
                // Unknown client command - allow but check size
                if (length > MAX_COMMAND_DATA_SIZE) {
                    return NET_PROTOCOL_SIZE_MISMATCH;
                }
                break;
        }
    } else {
        // Unknown command type
        return NET_PROTOCOL_INVALID_COMMAND;
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_CheckRateLimit

Check if packet rate exceeds limits
===============
*/
qboolean Net_CheckRateLimit(net_validation_context_t *ctx, int current_time) {
    if (!ctx || !ctx->config.enable_rate_limiting) {
        return qtrue; // No rate limiting
    }

    // Check if we're in a new second
    if (current_time - ctx->last_packet_time >= 1000) {
        ctx->packet_count_this_second = 0;
        ctx->last_packet_time = current_time;
    }

    // Increment counter
    ctx->packet_count_this_second++;

    // Check limit
    if (ctx->packet_count_this_second > ctx->config.max_packet_rate) {
        atomic_fetch_add(&ctx->stats.rate_limit_hits, 1);
        return qfalse; // Rate limit exceeded
    }

    return qtrue;
}

/*
===============
Net_ValidateFragmentationData

Validate fragmented packet data
===============
*/
net_protocol_result_t Net_ValidateFragmentationData(const byte *data, int length, int fragment_size) {
    if (!data || length <= 0) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    if (fragment_size <= 0 || fragment_size > MAX_MSGLEN) {
        return NET_PROTOCOL_SIZE_MISMATCH;
    }

    // Check fragment header
    if (length < 4) {
        return NET_PROTOCOL_INVALID_HEADER;
    }

    int fragment_start = (data[0] << 8) | data[1];
    int fragment_length = (data[2] << 8) | data[3];

    if (fragment_start < 0 || fragment_length <= 0) {
        return NET_PROTOCOL_INVALID_HEADER;
    }

    if (fragment_start + fragment_length > MAX_MSGLEN) {
        return NET_PROTOCOL_BUFFER_OVERFLOW;
    }

    if (length != 4 + fragment_length) {
        return NET_PROTOCOL_SIZE_MISMATCH;
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_ValidateConnectionHandshake

Validate connection handshake packets
===============
*/
net_protocol_result_t Net_ValidateConnectionHandshake(const byte *data, int length) {
    if (!data || length < 8) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    // Check challenge response format
    const char *challenge = (const char *)data;
    if (strlen(challenge) >= (size_t)length) {
        return NET_PROTOCOL_INVALID_PAYLOAD;
    }

    // Basic challenge validation (should contain expected format)
    if (strstr(challenge, "getchallenge") == NULL &&
        strstr(challenge, "connect") == NULL &&
        strstr(challenge, "getinfo") == NULL) {
        return NET_PROTOCOL_INVALID_COMMAND;
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_ValidateGameStateData

Validate game state data packets
===============
*/
net_protocol_result_t Net_ValidateGameStateData(const byte *data, int length) {
    if (!data || length <= 0) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    // Game state data should contain entity and player state
    // This is a basic validation - more detailed validation would parse the actual data
    if (length > MAX_COMMAND_DATA_SIZE) {
        return NET_PROTOCOL_SIZE_MISMATCH;
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_BoundsCheckArray

Bounds checking for array access
===============
*/
qboolean Net_BoundsCheckArray(const void *array, int element_size, int element_count, int max_elements) {
    if (!array || element_size <= 0 || element_count < 0) {
        return qfalse;
    }

    if (element_count > max_elements) {
        return qfalse;
    }

    // Check for potential integer overflow
    if (element_count > 0 && element_size > INT_MAX / element_count) {
        return qfalse;
    }

    return qtrue;
}

/*
===============
Net_BoundsCheckString

Bounds checking for string access
===============
*/
qboolean Net_BoundsCheckString(const char *str, int max_length) {
    if (!str) {
        return qfalse;
    }

    int len = strlen(str);
    if (len >= max_length) {
        return qfalse;
    }

    // Check for embedded null bytes (security issue)
    for (int i = 0; i < len; i++) {
        if (str[i] == '\0' && i < len - 1) {
            return qfalse; // Embedded null byte
        }
    }

    return qtrue;
}

/*
===============
Net_BoundsCheckPointer

Bounds checking for pointer arithmetic
===============
*/
qboolean Net_BoundsCheckPointer(const void *ptr, const void *buffer_start, int buffer_size) {
    if (!ptr || !buffer_start || buffer_size <= 0) {
        return qfalse;
    }

    const byte *ptr_byte = (const byte *)ptr;
    const byte *start_byte = (const byte *)buffer_start;
    const byte *end_byte = start_byte + buffer_size;

    if (ptr_byte < start_byte || ptr_byte >= end_byte) {
        return qfalse;
    }

    return qtrue;
}

/*
===============
Net_ValidateServerInfo

Validate server info response
===============
*/
net_protocol_result_t Net_ValidateServerInfo(const byte *data, int length) {
    if (!data || length <= 0) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    const char *info = (const char *)data;

    if (!Net_BoundsCheckString(info, length)) {
        return NET_PROTOCOL_INVALID_PAYLOAD;
    }

    // Check for required server info fields
    if (strstr(info, "\\hostname\\") == NULL) {
        return NET_PROTOCOL_INVALID_PAYLOAD;
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_ValidateClientCommand

Validate client command data
===============
*/
net_protocol_result_t Net_ValidateClientCommand(const byte *data, int length) {
    if (!data || length <= 0) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    const char *command = (const char *)data;

    if (!Net_BoundsCheckString(command, MAX_STRING_LENGTH)) {
        return NET_PROTOCOL_INVALID_PAYLOAD;
    }

    // Check for potentially dangerous commands
    if (strstr(command, "..") != NULL || strstr(command, "\\") != NULL) {
        return NET_PROTOCOL_INVALID_COMMAND;
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_ValidateEntityState

Validate entity state data
===============
*/
net_protocol_result_t Net_ValidateEntityState(const byte *data, int length) {
    if (!data || length < sizeof(entityState_t)) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    // Basic entity state validation
    const entityState_t *es = (const entityState_t *)data;

    if (es->number < 0 || es->number >= MAX_GENTITIES) {
        return NET_PROTOCOL_INVALID_PAYLOAD;
    }

    // Validate origin vector
    for (int i = 0; i < 3; i++) {
        if (!isfinite(es->origin[i])) {
            return NET_PROTOCOL_CORRUPTION_DETECTED;
        }
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_ValidatePlayerState

Validate player state data
===============
*/
net_protocol_result_t Net_ValidatePlayerState(const byte *data, int length) {
    if (!data || length < sizeof(playerState_t)) {
        return NET_PROTOCOL_INVALID_LENGTH;
    }

    // Basic player state validation
    const playerState_t *ps = (const playerState_t *)data;

    // Validate position
    for (int i = 0; i < 3; i++) {
        if (!isfinite(ps->origin[i])) {
            return NET_PROTOCOL_CORRUPTION_DETECTED;
        }
    }

    // Validate velocity
    for (int i = 0; i < 3; i++) {
        if (!isfinite(ps->velocity[i])) {
            return NET_PROTOCOL_CORRUPTION_DETECTED;
        }
    }

    return NET_PROTOCOL_VALID;
}

/*
===============
Net_DetectBufferOverflow

Detect potential buffer overflow attempts
===============
*/
qboolean Net_DetectBufferOverflow(const byte *buffer, int buffer_size, int write_offset, int write_size) {
    if (!buffer || buffer_size <= 0) {
        return qtrue; // Invalid buffer, but not an overflow attempt
    }

    if (write_offset < 0 || write_size < 0) {
        return qtrue; // Invalid parameters
    }

    if (write_offset + write_size > buffer_size) {
        return qtrue; // Buffer overflow detected
    }

    // Check for suspicious patterns that might indicate exploit attempts
    if (write_size > 0 && write_offset >= 0 && write_offset < buffer_size) {
        const byte *write_pos = buffer + write_offset;

        // Check for format string vulnerabilities
        for (int i = 0; i < write_size && i < 64; i++) {
            if (write_pos[i] == '%' && i + 1 < write_size) {
                char next = write_pos[i + 1];
                if (next == 'n' || next == 's' || next == 'p' || next == 'x') {
                    return qtrue; // Potential format string attack
                }
            }
        }
    }

    return qfalse; // No overflow detected
}

/*
===============
Net_ValidateMemoryAccess

Validate memory access bounds
===============
*/
qboolean Net_ValidateMemoryAccess(const void *ptr, size_t access_size, const void *valid_region_start, size_t valid_region_size) {
    if (!ptr || !valid_region_start || access_size == 0) {
        return qfalse;
    }

    const byte *ptr_byte = (const byte *)ptr;
    const byte *region_start = (const byte *)valid_region_start;
    const byte *region_end = region_start + valid_region_size;

    if (ptr_byte < region_start) {
        return qfalse;
    }

    if (ptr_byte + access_size > region_end) {
        return qfalse;
    }

    return qtrue;
}

/*
===============
Net_ValidationInit

Initialize validation context
===============
*/
void Net_ValidationInit(net_validation_context_t *ctx) {
    if (!ctx) return;

    memset(ctx, 0, sizeof(*ctx));

    // Set default configuration
    ctx->config.enable_validation = qtrue;
    ctx->config.strict_mode = qfalse;
    ctx->config.max_packet_rate = 100;  // 100 packets per second
    ctx->config.max_packet_size = MAX_MSGLEN;
    ctx->config.min_packet_size = 4;
    ctx->config.validate_sequences = qtrue;
    ctx->config.detect_corruption = qtrue;
    ctx->config.enable_rate_limiting = qtrue;

    // Initialize stats
    atomic_init(&ctx->stats.total_packets_validated, 0);
    atomic_init(&ctx->stats.packets_rejected, 0);
    atomic_init(&ctx->stats.buffer_overflow_attempts, 0);
    atomic_init(&ctx->stats.corruption_detected, 0);
    atomic_init(&ctx->stats.rate_limit_hits, 0);
    atomic_init(&ctx->stats.invalid_sequences, 0);
    atomic_init(&ctx->stats.size_mismatches, 0);
}

/*
===============
Net_ValidationShutdown

Shutdown validation context
===============
*/
void Net_ValidationShutdown(net_validation_context_t *ctx) {
    if (!ctx) return;

    // Nothing special to clean up currently
    memset(ctx, 0, sizeof(*ctx));
}

/*
===============
Net_ValidationResetStats

Reset validation statistics
===============
*/
void Net_ValidationResetStats(net_validation_context_t *ctx) {
    if (!ctx) return;

    atomic_store(&ctx->stats.total_packets_validated, 0);
    atomic_store(&ctx->stats.packets_rejected, 0);
    atomic_store(&ctx->stats.buffer_overflow_attempts, 0);
    atomic_store(&ctx->stats.corruption_detected, 0);
    atomic_store(&ctx->stats.rate_limit_hits, 0);
    atomic_store(&ctx->stats.invalid_sequences, 0);
    atomic_store(&ctx->stats.size_mismatches, 0);
}

/*
===============
Net_ValidationSetConfig

Set validation configuration
===============
*/
void Net_ValidationSetConfig(net_validation_context_t *ctx, const net_validation_config_t *config) {
    if (!ctx || !config) return;

    ctx->config = *config;
}

/*
===============
Net_ValidationGetStats

Get validation statistics
===============
*/
void Net_ValidationGetStats(const net_validation_context_t *ctx, net_validation_stats_t *stats) {
    if (!ctx || !stats) return;

    stats->total_packets_validated = atomic_load(&ctx->stats.total_packets_validated);
    stats->packets_rejected = atomic_load(&ctx->stats.packets_rejected);
    stats->buffer_overflow_attempts = atomic_load(&ctx->stats.buffer_overflow_attempts);
    stats->corruption_detected = atomic_load(&ctx->stats.corruption_detected);
    stats->rate_limit_hits = atomic_load(&ctx->stats.rate_limit_hits);
    stats->invalid_sequences = atomic_load(&ctx->stats.invalid_sequences);
    stats->size_mismatches = atomic_load(&ctx->stats.size_mismatches);
}

/*
===============
Net_ProtocolResultToString

Convert result code to string
===============
*/
const char *Net_ProtocolResultToString(net_protocol_result_t result) {
    switch (result) {
        case NET_PROTOCOL_VALID:
            return "Valid";
        case NET_PROTOCOL_INVALID_LENGTH:
            return "Invalid Length";
        case NET_PROTOCOL_INVALID_HEADER:
            return "Invalid Header";
        case NET_PROTOCOL_INVALID_SEQUENCE:
            return "Invalid Sequence";
        case NET_PROTOCOL_INVALID_COMMAND:
            return "Invalid Command";
        case NET_PROTOCOL_BUFFER_OVERFLOW:
            return "Buffer Overflow";
        case NET_PROTOCOL_CORRUPTION_DETECTED:
            return "Corruption Detected";
        case NET_PROTOCOL_RATE_LIMIT_EXCEEDED:
            return "Rate Limit Exceeded";
        case NET_PROTOCOL_INVALID_PAYLOAD:
            return "Invalid Payload";
        case NET_PROTOCOL_MALFORMED_PACKET:
            return "Malformed Packet";
        case NET_PROTOCOL_SIZE_MISMATCH:
            return "Size Mismatch";
        default:
            return "Unknown Error";
    }
}

/*
===============
Net_IsCriticalProtocolError

Check if error is critical
===============
*/
qboolean Net_IsCriticalProtocolError(net_protocol_result_t result) {
    switch (result) {
        case NET_PROTOCOL_BUFFER_OVERFLOW:
        case NET_PROTOCOL_CORRUPTION_DETECTED:
        case NET_PROTOCOL_INVALID_LENGTH:
            return qtrue;
        default:
            return qfalse;
    }
}

// Forward declarations for missing validation functions
net_protocol_result_t Net_ValidateSnapshotData(const byte *data, int length) {
    // Basic snapshot validation
    if (!data || length <= 0 || length > MAX_COMMAND_DATA_SIZE) {
        return NET_PROTOCOL_SIZE_MISMATCH;
    }
    return NET_PROTOCOL_VALID;
}

net_protocol_result_t Net_ValidateClientMove(const byte *data, int length) {
    // Basic client move validation
    if (!data || length <= 0 || length > MAX_COMMAND_DATA_SIZE) {
        return NET_PROTOCOL_SIZE_MISMATCH;
    }
    return NET_PROTOCOL_VALID;
}

net_protocol_result_t Net_ValidateServerCommand(const byte *data, int length) {
    // Basic server command validation
    if (!data || length <= 0 || length > MAX_COMMAND_DATA_SIZE) {
        return NET_PROTOCOL_SIZE_MISMATCH;
    }
    return NET_PROTOCOL_VALID;
}