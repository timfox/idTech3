/*
===========================================================================
JSON-based network configuration system using enhanced cvar KVP functionality.
Provides structured configuration for QoS parameters and connection settings.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

/*
================
Network Configuration JSON Schema

This defines the structure for network configuration using JSON cvars.
Example configuration:

{
  "connection": {
    "timeout_ms": 30000,
    "keepalive_interval_ms": 5000,
    "max_retransmits": 5,
    "initial_rtt_ms": 100,
    "min_rtt_ms": 20,
    "max_rtt_ms": 1000
  },
  "qos": {
    "enabled": true,
    "packet_priority": {
      "game_state": "high",
      "player_updates": "high",
      "chat_messages": "medium",
      "sound_events": "low"
    },
    "bandwidth_limits": {
      "max_outgoing_kbps": 1000,
      "max_incoming_kbps": 2000,
      "reserved_for_game_kbps": 500
    },
    "congestion_control": {
      "algorithm": "bbr",
      "min_window_packets": 10,
      "max_window_packets": 1000,
      "slow_start_threshold": 50
    }
  },
  "fragmentation": {
    "enhanced": true,
    "max_fragment_size": 1400,
    "fragment_timeout_ms": 100,
    "max_fragments_per_packet": 16,
    "compression": {
      "enabled": true,
      "algorithm": "lz4",
      "min_size_threshold": 512
    }
  },
  "security": {
    "packet_validation": true,
    "strict_mode": false,
    "rate_limiting": {
      "enabled": true,
      "max_packets_per_second": 100,
      "burst_limit": 20,
      "ban_threshold": 1000
    },
    "encryption": {
      "enabled": false,
      "method": "none",
      "key_exchange": "none"
    }
  },
  "debugging": {
    "packet_logging": false,
    "latency_monitoring": true,
    "connection_profiling": false,
    "detailed_stats": false
  }
}
================
*/

static cvar_t *net_config_json = NULL;

/*
================
NET_InitConfigurationJSON

Initialize JSON-based network configuration
================
*/
void NET_InitConfigurationJSON(void) {
	const char *default_config = "{"
		"\"connection\": {"
			"\"timeout_ms\": 30000,"
			"\"keepalive_interval_ms\": 5000,"
			"\"max_retransmits\": 5,"
			"\"initial_rtt_ms\": 100,"
			"\"min_rtt_ms\": 20,"
			"\"max_rtt_ms\": 1000"
		"},"
		"\"qos\": {"
			"\"enabled\": true,"
			"\"packet_priority\": {"
				"\"game_state\": \"high\","
				"\"player_updates\": \"high\","
				"\"chat_messages\": \"medium\","
				"\"sound_events\": \"low\""
			"},"
			"\"bandwidth_limits\": {"
				"\"max_outgoing_kbps\": 1000,"
				"\"max_incoming_kbps\": 2000,"
				"\"reserved_for_game_kbps\": 500"
			"},"
			"\"congestion_control\": {"
				"\"algorithm\": \"bbr\","
				"\"min_window_packets\": 10,"
				"\"max_window_packets\": 1000,"
				"\"slow_start_threshold\": 50"
			"}"
		"},"
		"\"fragmentation\": {"
			"\"enhanced\": true,"
			"\"max_fragment_size\": 1400,"
			"\"fragment_timeout_ms\": 100,"
			"\"max_fragments_per_packet\": 16,"
			"\"compression\": {"
				"\"enabled\": true,"
				"\"algorithm\": \"lz4\","
				"\"min_size_threshold\": 512"
			"}"
		"},"
		"\"security\": {"
			"\"packet_validation\": true,"
			"\"strict_mode\": false,"
			"\"rate_limiting\": {"
				"\"enabled\": true,"
				"\"max_packets_per_second\": 100,"
				"\"burst_limit\": 20,"
				"\"ban_threshold\": 1000"
			"},"
			"\"encryption\": {"
				"\"enabled\": false,"
				"\"method\": \"none\","
				"\"key_exchange\": \"none\""
			"}"
		"},"
		"\"debugging\": {"
			"\"packet_logging\": false,"
			"\"latency_monitoring\": true,"
			"\"connection_profiling\": false,"
			"\"detailed_stats\": false"
		"}"
	"}";

	net_config_json = Cvar_GetJSON("net_config_json", default_config, CVAR_ARCHIVE);
	Cvar_SetDescription(net_config_json, "JSON configuration for network settings (QoS, connection, security)");
	Cvar_SetJSONValidator(net_config_json, CVJ_TYPE_CHECK, NULL);
}

/*
================
NET_GetConnectionConfig

Retrieve connection configuration from JSON
================
*/
void NET_GetConnectionConfig(int *timeout_ms, int *keepalive_interval_ms, int *max_retransmits,
							int *initial_rtt_ms, int *min_rtt_ms, int *max_rtt_ms) {
	if (!net_config_json) {
		*timeout_ms = 30000;
		*keepalive_interval_ms = 5000;
		*max_retransmits = 5;
		*initial_rtt_ms = 100;
		*min_rtt_ms = 20;
		*max_rtt_ms = 1000;
		return;
	}

	*timeout_ms = (int)Cvar_GetJSONNumber("net_config_json", "connection.timeout_ms", 30000.0);
	*keepalive_interval_ms = (int)Cvar_GetJSONNumber("net_config_json", "connection.keepalive_interval_ms", 5000.0);
	*max_retransmits = (int)Cvar_GetJSONNumber("net_config_json", "connection.max_retransmits", 5.0);
	*initial_rtt_ms = (int)Cvar_GetJSONNumber("net_config_json", "connection.initial_rtt_ms", 100.0);
	*min_rtt_ms = (int)Cvar_GetJSONNumber("net_config_json", "connection.min_rtt_ms", 20.0);
	*max_rtt_ms = (int)Cvar_GetJSONNumber("net_config_json", "connection.max_rtt_ms", 1000.0);
}

/*
================
NET_GetQoSConfig

Retrieve QoS configuration from JSON
================
*/
void NET_GetQoSConfig(qboolean *enabled, const char **game_state_priority, const char **player_updates_priority,
					 int *max_outgoing_kbps, int *max_incoming_kbps, int *reserved_game_kbps,
					 const char **congestion_algorithm, int *min_window_packets, int *max_window_packets) {
	if (!net_config_json) {
		*enabled = qtrue;
		*game_state_priority = "high";
		*player_updates_priority = "high";
		*max_outgoing_kbps = 1000;
		*max_incoming_kbps = 2000;
		*reserved_game_kbps = 500;
		*congestion_algorithm = "bbr";
		*min_window_packets = 10;
		*max_window_packets = 1000;
		return;
	}

	*enabled = Cvar_GetJSONBoolean("net_config_json", "qos.enabled", qtrue);
	*game_state_priority = Cvar_GetJSONString("net_config_json", "qos.packet_priority.game_state", "high");
	*player_updates_priority = Cvar_GetJSONString("net_config_json", "qos.packet_priority.player_updates", "high");
	*max_outgoing_kbps = (int)Cvar_GetJSONNumber("net_config_json", "qos.bandwidth_limits.max_outgoing_kbps", 1000.0);
	*max_incoming_kbps = (int)Cvar_GetJSONNumber("net_config_json", "qos.bandwidth_limits.max_incoming_kbps", 2000.0);
	*reserved_game_kbps = (int)Cvar_GetJSONNumber("net_config_json", "qos.bandwidth_limits.reserved_for_game_kbps", 500.0);
	*congestion_algorithm = Cvar_GetJSONString("net_config_json", "qos.congestion_control.algorithm", "bbr");
	*min_window_packets = (int)Cvar_GetJSONNumber("net_config_json", "qos.congestion_control.min_window_packets", 10.0);
	*max_window_packets = (int)Cvar_GetJSONNumber("net_config_json", "qos.congestion_control.max_window_packets", 1000.0);
}

/*
================
NET_GetFragmentationConfig

Retrieve fragmentation configuration from JSON
================
*/
void NET_GetFragmentationConfig(qboolean *enhanced, int *max_fragment_size, int *fragment_timeout_ms,
							   int *max_fragments_per_packet, qboolean *compression_enabled,
							   const char **compression_algorithm, int *compression_threshold) {
	if (!net_config_json) {
		*enhanced = qtrue;
		*max_fragment_size = 1400;
		*fragment_timeout_ms = 100;
		*max_fragments_per_packet = 16;
		*compression_enabled = qtrue;
		*compression_algorithm = "lz4";
		*compression_threshold = 512;
		return;
	}

	*enhanced = Cvar_GetJSONBoolean("net_config_json", "fragmentation.enhanced", qtrue);
	*max_fragment_size = (int)Cvar_GetJSONNumber("net_config_json", "fragmentation.max_fragment_size", 1400.0);
	*fragment_timeout_ms = (int)Cvar_GetJSONNumber("net_config_json", "fragmentation.fragment_timeout_ms", 100.0);
	*max_fragments_per_packet = (int)Cvar_GetJSONNumber("net_config_json", "fragmentation.max_fragments_per_packet", 16.0);
	*compression_enabled = Cvar_GetJSONBoolean("net_config_json", "fragmentation.compression.enabled", qtrue);
	*compression_algorithm = Cvar_GetJSONString("net_config_json", "fragmentation.compression.algorithm", "lz4");
	*compression_threshold = (int)Cvar_GetJSONNumber("net_config_json", "fragmentation.compression.min_size_threshold", 512.0);
}

/*
================
NET_GetSecurityConfig

Retrieve security configuration from JSON
================
*/
void NET_GetSecurityConfig(qboolean *packet_validation, qboolean *strict_mode,
						 qboolean *rate_limiting_enabled, int *max_packets_per_second,
						 qboolean *encryption_enabled, const char **encryption_method) {
	if (!net_config_json) {
		*packet_validation = qtrue;
		*strict_mode = qfalse;
		*rate_limiting_enabled = qtrue;
		*max_packets_per_second = 100;
		*encryption_enabled = qfalse;
		*encryption_method = "none";
		return;
	}

	*packet_validation = Cvar_GetJSONBoolean("net_config_json", "security.packet_validation", qtrue);
	*strict_mode = Cvar_GetJSONBoolean("net_config_json", "security.strict_mode", qfalse);
	*rate_limiting_enabled = Cvar_GetJSONBoolean("net_config_json", "security.rate_limiting.enabled", qtrue);
	*max_packets_per_second = (int)Cvar_GetJSONNumber("net_config_json", "security.rate_limiting.max_packets_per_second", 100.0);
	*encryption_enabled = Cvar_GetJSONBoolean("net_config_json", "security.encryption.enabled", qfalse);
	*encryption_method = Cvar_GetJSONString("net_config_json", "security.encryption.method", "none");
}

/*
================
NET_GetDebuggingConfig

Retrieve debugging configuration from JSON
================
*/
void NET_GetDebuggingConfig(qboolean *packet_logging, qboolean *latency_monitoring,
						   qboolean *connection_profiling, qboolean *detailed_stats) {
	if (!net_config_json) {
		*packet_logging = qfalse;
		*latency_monitoring = qtrue;
		*connection_profiling = qfalse;
		*detailed_stats = qfalse;
		return;
	}

	*packet_logging = Cvar_GetJSONBoolean("net_config_json", "debugging.packet_logging", qfalse);
	*latency_monitoring = Cvar_GetJSONBoolean("net_config_json", "debugging.latency_monitoring", qtrue);
	*connection_profiling = Cvar_GetJSONBoolean("net_config_json", "debugging.connection_profiling", qfalse);
	*detailed_stats = Cvar_GetJSONBoolean("net_config_json", "debugging.detailed_stats", qfalse);
}

/*
================
NET_UpdateQoSConfig

Update QoS configuration in JSON
================
*/
void NET_UpdateQoSConfig(qboolean enabled, int max_outgoing_kbps, int max_incoming_kbps, int reserved_game_kbps) {
	if (!net_config_json) return;

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"qos\": {"
				"\"enabled\": %s,"
				"\"bandwidth_limits\": {"
					"\"max_outgoing_kbps\": %d,"
					"\"max_incoming_kbps\": %d,"
					"\"reserved_for_game_kbps\": %d"
				"}"
			"}"
		"}",
		enabled ? "true" : "false",
		max_outgoing_kbps,
		max_incoming_kbps,
		reserved_game_kbps);

	Cvar_SetJSON("net_config_json", json_buffer);
}

/*
================
NET_UpdateSecurityConfig

Update security configuration in JSON
================
*/
void NET_UpdateSecurityConfig(qboolean packet_validation, qboolean strict_mode,
							 qboolean rate_limiting_enabled, int max_packets_per_second) {
	if (!net_config_json) return;

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"security\": {"
				"\"packet_validation\": %s,"
				"\"strict_mode\": %s,"
				"\"rate_limiting\": {"
					"\"enabled\": %s,"
					"\"max_packets_per_second\": %d"
				"}"
			"}"
		"}",
		packet_validation ? "true" : "false",
		strict_mode ? "true" : "false",
		rate_limiting_enabled ? "true" : "false",
		max_packets_per_second);

	Cvar_SetJSON("net_config_json", json_buffer);
}

/*
================
NET_UpdateDebuggingConfig

Update debugging configuration in JSON
================
*/
void NET_UpdateDebuggingConfig(qboolean packet_logging, qboolean latency_monitoring,
							  qboolean connection_profiling, qboolean detailed_stats) {
	if (!net_config_json) return;

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"debugging\": {"
				"\"packet_logging\": %s,"
				"\"latency_monitoring\": %s,"
				"\"connection_profiling\": %s,"
				"\"detailed_stats\": %s"
			"}"
		"}",
		packet_logging ? "true" : "false",
		latency_monitoring ? "true" : "false",
		connection_profiling ? "true" : "false",
		detailed_stats ? "true" : "false");

	Cvar_SetJSON("net_config_json", json_buffer);
}

/*
================
NET_GetPacketPriority

Get packet priority for a specific packet type
================
*/
const char *NET_GetPacketPriority(const char *packet_type) {
	if (!net_config_json || !packet_type) {
		return "medium";
	}

	char key_path[256];
	Com_sprintf(key_path, sizeof(key_path), "qos.packet_priority.%s", packet_type);

	return Cvar_GetJSONString("net_config_json", key_path, "medium");
}

/*
================
NET_ShutdownConfigurationJSON

Shutdown JSON-based network configuration
================
*/
void NET_ShutdownConfigurationJSON(void) {
	// JSON cvars are automatically cleaned up by the cvar system
	net_config_json = NULL;
}