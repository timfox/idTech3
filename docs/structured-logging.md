# Structured Logging System

## Overview

The id Tech 3 engine now includes a modern structured logging system that replaces the legacy `Com_Printf` with a more powerful, configurable logging solution.

## Features

### Log Levels
- **DEBUG** (0): Detailed debugging information
- **INFO** (1): General informational messages
- **WARN** (2): Warning messages
- **ERROR** (3): Error conditions
- **FATAL** (4): Fatal errors that cause termination

### Log Categories
- **general**: General engine messages
- **client**: Client-side code
- **server**: Server-side code
- **renderer**: Rendering system
- **network**: Network operations
- **filesystem**: File system operations
- **sound**: Audio system
- **input**: Input handling
- **physics**: Physics simulation
- **ai**: AI/bot code
- **script**: Scripting (Lua/QVM)
- **memory**: Memory management

### Output Formats
- **Text**: Human-readable format with timestamps, levels, and categories
- **JSON**: Machine-readable JSON format for log aggregation systems

### Output Destinations
- **Console**: Standard console output (stdout/stderr)
- **File**: Log files with rotation support
- **Syslog**: System logging daemon (Linux/Unix only)

### Log Rotation
- **Size-based**: Rotate when log file reaches specified size (default: 100MB)
- **Time-based**: Rotate after specified time period (default: 24 hours)
- **Automatic cleanup**: Keeps up to 10 rotated log files

## Configuration

### CVars

| CVar | Default | Description |
|------|---------|-------------|
| `log_enable` | `1` | Enable structured logging (0=disabled, 1=enabled) |
| `log_level` | `1` | Global log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL) |
| `log_format` | `0` | Log format (0=text, 1=JSON) |
| `log_output` | `3` | Output destinations (1=console, 2=file, 4=syslog, combine with +) |
| `log_file` | `qconsole.log` | Log file name |
| `log_rotation_size` | `100` | Log rotation size in MB (0=disabled) |
| `log_rotation_time` | `24` | Log rotation time in hours (0=disabled) |
| `log_category_filter` | `""` | Category filter (comma-separated, prefix with - to disable) |

### Examples

```bash
# Enable JSON format
/set log_format 1

# Enable file and syslog output
/set log_output 6

# Set log level to WARN
/set log_level 2

# Disable network category
/set log_category_filter -network

# Enable only renderer and client categories
/set log_category_filter renderer,client
```

## Usage

### Basic Logging

```c
#include "q_log.h"

// Simple logging
Q_LogInfo(LOG_CATEGORY_GENERAL, "Engine initialized");
Q_LogWarn(LOG_CATEGORY_NETWORK, "Connection timeout");
Q_LogError(LOG_CATEGORY_FILESYSTEM, "Failed to open file: %s", filename);

// Category-specific macros
Q_LogClient(LOG_LEVEL_INFO, "Client connected");
Q_LogServer(LOG_LEVEL_WARN, "Server overloaded");
Q_LogRenderer(LOG_LEVEL_ERROR, "Shader compilation failed");
Q_LogNetwork(LOG_LEVEL_DEBUG, "Packet received: %d bytes", size);
```

### Advanced Logging

```c
// Direct logging with file/line information
Q_Log(LOG_LEVEL_INFO, LOG_CATEGORY_RENDERER, __FILE__, __LINE__, __FUNCTION__, 
      "Texture loaded: %s", texture_name);
```

### Compatibility

The system maintains backward compatibility with `Com_Printf`:
- When `log_enable` is set to 1, `Com_Printf` calls are automatically routed through the structured logger
- Legacy `com_logfile` CVar still works for compatibility
- Existing code doesn't need to be modified

## Output Examples

### Text Format
```
[2024-01-15 14:30:45] [INFO] [renderer] tr_image.c:123 LoadTexture() - Texture loaded: textures/common/caulk.tga
[2024-01-15 14:30:46] [WARN] [network] cl_net_chan.c:456 CL_ParseServerMessage() - Received invalid packet
[2024-01-15 14:30:47] [ERROR] [filesystem] files.c:789 FS_FOpenFileRead() - File not found: maps/q3dm1.bsp
```

### JSON Format
```json
{"timestamp":"2024-01-15T14:30:45+0000","level":"INFO","category":"renderer","file":"tr_image.c","line":123,"function":"LoadTexture","message":"Texture loaded: textures/common/caulk.tga"}
{"timestamp":"2024-01-15T14:30:46+0000","level":"WARN","category":"network","file":"cl_net_chan.c","line":456,"function":"CL_ParseServerMessage","message":"Received invalid packet"}
{"timestamp":"2024-01-15T14:30:47+0000","level":"ERROR","category":"filesystem","file":"files.c","line":789,"function":"FS_FOpenFileRead","message":"File not found: maps/q3dm1.bsp"}
```

## Integration with External Systems

### Syslog Integration
On Linux/Unix systems, logs can be sent to syslog:
```bash
/set log_output 4  # Syslog only
/set log_output 6  # File + Syslog
```

### Log Aggregation
JSON format is compatible with log aggregation systems like:
- **ELK Stack** (Elasticsearch, Logstash, Kibana)
- **Loki** (Grafana Labs)
- **Splunk**
- **Fluentd**

### Example Logstash Configuration
```ruby
input {
  file {
    path => "/path/to/qconsole.log"
    codec => "json"
  }
}

filter {
  if [level] == "ERROR" {
    mutate { add_tag => [ "error" ] }
  }
}

output {
  elasticsearch {
    hosts => ["localhost:9200"]
    index => "idtech3-logs-%{+YYYY.MM.dd}"
  }
}
```

## Performance Considerations

- Logging is disabled when `log_enable` is 0
- Category filtering happens before message formatting
- File I/O is buffered unless sync mode is enabled
- JSON formatting has minimal overhead
- Syslog uses async I/O

## Migration Guide

### For Developers

1. **Replace Com_Printf calls:**
   ```c
   // Old
   Com_Printf("Loading map: %s\n", mapname);
   
   // New
   Q_LogInfo(LOG_CATEGORY_FILESYSTEM, "Loading map: %s", mapname);
   ```

2. **Replace Com_DPrintf calls:**
   ```c
   // Old
   Com_DPrintf("Debug: %s\n", debug_info);
   
   // New
   Q_LogDebug(LOG_CATEGORY_GENERAL, "Debug: %s", debug_info);
   ```

3. **Use appropriate categories:**
   - Choose the most specific category
   - Use LOG_CATEGORY_GENERAL only when no other category fits

4. **Use appropriate levels:**
   - DEBUG: Detailed debugging info
   - INFO: Normal operation messages
   - WARN: Recoverable errors
   - ERROR: Non-fatal errors
   - FATAL: Fatal errors

## Troubleshooting

### Logs not appearing
- Check `log_enable` is set to 1
- Verify log level is appropriate
- Check category filter settings
- Ensure output destination is configured

### Log rotation not working
- Verify `log_rotation_size` or `log_rotation_time` is set
- Check file system permissions
- Ensure sufficient disk space

### JSON parsing errors
- Verify `log_format` is set to 1
- Check for invalid characters in log messages
- Ensure proper JSON escaping

## Future Enhancements

Potential future improvements:
- Remote logging via UDP/TCP
- Log compression
- Custom log formatters
- Per-category log files
- Log sampling/throttling
- Structured metadata support

