/*
===========================================================================
JSON-based network configuration system header.
===========================================================================
*/

#ifndef NET_CONFIG_JSON_H
#define NET_CONFIG_JSON_H

// Function prototypes
void NET_InitConfigurationJSON(void);
void NET_ShutdownConfigurationJSON(void);
void NET_GetConnectionConfig(int *timeout_ms, int *keepalive_interval_ms, int *max_retransmits, int *initial_rtt_ms, int *min_rtt_ms, int *max_rtt_ms);
void NET_GetQoSConfig(qboolean *enabled, const char **game_state_priority, const char **player_updates_priority, int *max_outgoing_kbps, int *max_incoming_kbps, int *reserved_game_kbps, const char **congestion_algorithm, int *min_window_packets, int *max_window_packets);
void NET_UpdateQoSConfig(qboolean enabled, int max_outgoing_kbps, int max_incoming_kbps, int reserved_game_kbps);
void NET_GetFragmentationConfig(qboolean *enhanced, int *max_fragment_size, int *fragment_timeout_ms, int *max_fragments_per_packet, qboolean *compression_enabled, const char **compression_algorithm, int *compression_threshold);
void NET_GetSecurityConfig(qboolean *packet_validation, qboolean *strict_mode, qboolean *rate_limiting_enabled, int *max_packets_per_second, qboolean *encryption_enabled, const char **encryption_method);
void NET_UpdateSecurityConfig(qboolean packet_validation, qboolean strict_mode, qboolean rate_limiting_enabled, int max_packets_per_second);
void NET_GetDebuggingConfig(qboolean *packet_logging, qboolean *latency_monitoring, qboolean *connection_profiling, qboolean *detailed_stats);
void NET_UpdateDebuggingConfig(qboolean packet_logging, qboolean latency_monitoring, qboolean connection_profiling, qboolean detailed_stats);
const char *NET_GetPacketPriority(const char *packet_type);

// JSON-based network configuration functions
void NET_InitConfigurationJSON(void);
void NET_ShutdownConfigurationJSON(void);

// Connection configuration
void NET_GetConnectionConfig(int *timeout_ms, int *keepalive_interval_ms, int *max_retransmits,
							int *initial_rtt_ms, int *min_rtt_ms, int *max_rtt_ms);

// QoS configuration
void NET_GetQoSConfig(qboolean *enabled, const char **game_state_priority, const char **player_updates_priority,
					 int *max_outgoing_kbps, int *max_incoming_kbps, int *reserved_game_kbps,
					 const char **congestion_algorithm, int *min_window_packets, int *max_window_packets);
void NET_UpdateQoSConfig(qboolean enabled, int max_outgoing_kbps, int max_incoming_kbps, int reserved_game_kbps);

// Fragmentation configuration
void NET_GetFragmentationConfig(qboolean *enhanced, int *max_fragment_size, int *fragment_timeout_ms,
							   int *max_fragments_per_packet, qboolean *compression_enabled,
							   const char **compression_algorithm, int *compression_threshold);

// Security configuration
void NET_GetSecurityConfig(qboolean *packet_validation, qboolean *strict_mode,
						 qboolean *rate_limiting_enabled, int *max_packets_per_second,
						 qboolean *encryption_enabled, const char **encryption_method);
void NET_UpdateSecurityConfig(qboolean packet_validation, qboolean strict_mode,
							 qboolean rate_limiting_enabled, int max_packets_per_second);

// Debugging configuration
void NET_GetDebuggingConfig(qboolean *packet_logging, qboolean *latency_monitoring,
						   qboolean *connection_profiling, qboolean *detailed_stats);
void NET_UpdateDebuggingConfig(qboolean packet_logging, qboolean latency_monitoring,
							  qboolean connection_profiling, qboolean detailed_stats);

// Utility functions
const char *NET_GetPacketPriority(const char *packet_type);

#endif // NET_CONFIG_JSON_H