<?php
/**
 * Structured Logging System Documentation
 */
$title = 'Structured Logging - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/structured-logging' => 'Structured Logging'
];
?>

<h1>Structured Logging System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine now includes a modern structured logging system that replaces the legacy <code>Com_Printf</code> with a more powerful, configurable logging solution. This system provides log levels, categories, multiple output formats, and integration with external logging systems.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Log Levels:</strong> DEBUG, INFO, WARN, ERROR, FATAL</li>
            <li><strong>Categories:</strong> General, client, server, renderer, network, filesystem, sound, input, physics, AI, script, memory</li>
            <li><strong>Output Formats:</strong> Human-readable text and machine-readable JSON</li>
            <li><strong>Output Destinations:</strong> Console, file, and syslog</li>
            <li><strong>Log Rotation:</strong> Size-based and time-based rotation with automatic cleanup</li>
            <li><strong>Backward Compatible:</strong> Existing <code>Com_Printf</code> calls automatically routed through structured logger</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Log Levels</h2>
    <p>The structured logging system uses five log levels:</p>
    <ul>
        <li><strong>DEBUG (0):</strong> Detailed debugging information</li>
        <li><strong>INFO (1):</strong> General informational messages</li>
        <li><strong>WARN (2):</strong> Warning messages</li>
        <li><strong>ERROR (3):</strong> Error conditions</li>
        <li><strong>FATAL (4):</strong> Fatal errors that cause termination</li>
    </ul>
</div>

<div class="section">
    <h2>Log Categories</h2>
    <p>Logs are organized into categories for better filtering and organization:</p>
    <ul>
        <li><strong>general:</strong> General engine messages</li>
        <li><strong>client:</strong> Client-side code</li>
        <li><strong>server:</strong> Server-side code</li>
        <li><strong>renderer:</strong> Rendering system</li>
        <li><strong>network:</strong> Network operations</li>
        <li><strong>filesystem:</strong> File system operations</li>
        <li><strong>sound:</strong> Audio system</li>
        <li><strong>input:</strong> Input handling</li>
        <li><strong>physics:</strong> Physics simulation</li>
        <li><strong>ai:</strong> AI/bot code</li>
        <li><strong>script:</strong> Scripting (Lua/QVM)</li>
        <li><strong>memory:</strong> Memory management</li>
    </ul>
</div>

<div class="section">
    <h2>Configuration</h2>
    
    <h3>CVars</h3>
    <table class="settings-table">
        <tr>
            <th>CVar</th>
            <th>Default</th>
            <th>Description</th>
        </tr>
        <tr>
            <td><code>log_enable</code></td>
            <td>1</td>
            <td>Enable structured logging (0=disabled, 1=enabled)</td>
        </tr>
        <tr>
            <td><code>log_level</code></td>
            <td>1</td>
            <td>Global log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL)</td>
        </tr>
        <tr>
            <td><code>log_format</code></td>
            <td>0</td>
            <td>Log format (0=text, 1=JSON)</td>
        </tr>
        <tr>
            <td><code>log_output</code></td>
            <td>3</td>
            <td>Output destinations (1=console, 2=file, 4=syslog, combine with +)</td>
        </tr>
        <tr>
            <td><code>log_file</code></td>
            <td>qconsole.log</td>
            <td>Log file name</td>
        </tr>
        <tr>
            <td><code>log_rotation_size</code></td>
            <td>100</td>
            <td>Log rotation size in MB (0=disabled)</td>
        </tr>
        <tr>
            <td><code>log_rotation_time</code></td>
            <td>24</td>
            <td>Log rotation time in hours (0=disabled)</td>
        </tr>
        <tr>
            <td><code>log_category_filter</code></td>
            <td>""</td>
            <td>Category filter (comma-separated, prefix with - to disable)</td>
        </tr>
    </table>
    
    <h3>Configuration Examples</h3>
    <div class="code-block">
        <pre><code># Enable JSON format
/set log_format 1

# Enable file and syslog output
/set log_output 6

# Set log level to WARN
/set log_level 2

# Disable network category
/set log_category_filter -network

# Enable only renderer and client categories
/set log_category_filter renderer,client</code></pre>
    </div>
</div>

<div class="section">
    <h2>Usage</h2>
    
    <h3>Basic Logging</h3>
    <div class="code-block">
        <pre><code>#include "q_log.h"

// Simple logging
Q_LogInfo(LOG_CATEGORY_GENERAL, "Engine initialized");
Q_LogWarn(LOG_CATEGORY_NETWORK, "Connection timeout");
Q_LogError(LOG_CATEGORY_FILESYSTEM, "Failed to open file: %s", filename);

// Category-specific macros
Q_LogClient(LOG_LEVEL_INFO, "Client connected");
Q_LogServer(LOG_LEVEL_WARN, "Server overloaded");
Q_LogRenderer(LOG_LEVEL_ERROR, "Shader compilation failed");
Q_LogNetwork(LOG_LEVEL_DEBUG, "Packet received: %d bytes", size);</code></pre>
    </div>
    
    <h3>Advanced Logging</h3>
    <div class="code-block">
        <pre><code>// Direct logging with file/line information
Q_Log(LOG_LEVEL_INFO, LOG_CATEGORY_RENDERER, __FILE__, __LINE__, __FUNCTION__, 
      "Texture loaded: %s", texture_name);</code></pre>
    </div>
    
    <h3>Backward Compatibility</h3>
    <p>The system maintains backward compatibility with <code>Com_Printf</code>:</p>
    <ul>
        <li>When <code>log_enable</code> is set to 1, <code>Com_Printf</code> calls are automatically routed through the structured logger</li>
        <li>Legacy <code>com_logfile</code> CVar still works for compatibility</li>
        <li>Existing code doesn't need to be modified</li>
    </ul>
</div>

<div class="section">
    <h2>Output Examples</h2>
    
    <h3>Text Format</h3>
    <div class="code-block">
        <pre><code>[2024-01-15 14:30:45] [INFO] [renderer] tr_image.c:123 LoadTexture() - Texture loaded: textures/common/caulk.tga
[2024-01-15 14:30:46] [WARN] [network] cl_net_chan.c:456 CL_ParseServerMessage() - Received invalid packet
[2024-01-15 14:30:47] [ERROR] [filesystem] files.c:789 FS_FOpenFileRead() - File not found: maps/q3dm1.bsp</code></pre>
    </div>
    
    <h3>JSON Format</h3>
    <div class="code-block">
        <pre><code>{"timestamp":"2024-01-15T14:30:45+0000","level":"INFO","category":"renderer","file":"tr_image.c","line":123,"function":"LoadTexture","message":"Texture loaded: textures/common/caulk.tga"}
{"timestamp":"2024-01-15T14:30:46+0000","level":"WARN","category":"network","file":"cl_net_chan.c","line":456,"function":"CL_ParseServerMessage","message":"Received invalid packet"}
{"timestamp":"2024-01-15T14:30:47+0000","level":"ERROR","category":"filesystem","file":"files.c","line":789,"function":"FS_FOpenFileRead","message":"File not found: maps/q3dm1.bsp"}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Log Rotation</h2>
    <p>The logging system supports automatic log rotation:</p>
    <ul>
        <li><strong>Size-based:</strong> Rotate when log file reaches specified size (default: 100MB)</li>
        <li><strong>Time-based:</strong> Rotate after specified time period (default: 24 hours)</li>
        <li><strong>Automatic cleanup:</strong> Keeps up to 10 rotated log files</li>
    </ul>
    
    <div class="code-block">
        <pre><code># Enable size-based rotation (100MB)
/set log_rotation_size 100

# Enable time-based rotation (24 hours)
/set log_rotation_time 24

# Disable rotation
/set log_rotation_size 0
/set log_rotation_time 0</code></pre>
    </div>
</div>

<div class="section">
    <h2>Integration with External Systems</h2>
    
    <h3>Syslog Integration</h3>
    <p>On Linux/Unix systems, logs can be sent to syslog:</p>
    <div class="code-block">
        <pre><code>/set log_output 4  # Syslog only
/set log_output 6  # File + Syslog</code></pre>
    </div>
    
    <h3>Log Aggregation</h3>
    <p>JSON format is compatible with log aggregation systems like:</p>
    <ul>
        <li><strong>ELK Stack</strong> (Elasticsearch, Logstash, Kibana)</li>
        <li><strong>Loki</strong> (Grafana Labs)</li>
        <li><strong>Splunk</strong></li>
        <li><strong>Fluentd</strong></li>
    </ul>
</div>

<div class="section">
    <h2>Performance Considerations</h2>
    <ul>
        <li>Logging is disabled when <code>log_enable</code> is 0</li>
        <li>Category filtering happens before message formatting</li>
        <li>File I/O is buffered unless sync mode is enabled</li>
        <li>JSON formatting has minimal overhead</li>
        <li>Syslog uses async I/O</li>
    </ul>
</div>

<div class="section">
    <h2>Migration Guide</h2>
    
    <h3>For Developers</h3>
    <p>To migrate from <code>Com_Printf</code> to structured logging:</p>
    
    <div class="code-block">
        <pre><code>// Old
Com_Printf("Loading map: %s\n", mapname);

// New
Q_LogInfo(LOG_CATEGORY_FILESYSTEM, "Loading map: %s", mapname);

// Old
Com_DPrintf("Debug: %s\n", debug_info);

// New
Q_LogDebug(LOG_CATEGORY_GENERAL, "Debug: %s", debug_info);</code></pre>
    </div>
    
    <h3>Best Practices</h3>
    <ul>
        <li>Choose the most specific category</li>
        <li>Use <code>LOG_CATEGORY_GENERAL</code> only when no other category fits</li>
        <li>Use appropriate log levels (DEBUG for detailed info, ERROR for errors)</li>
        <li>Include context in log messages</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/memory-management">Memory Management</a></li>
        <li><a href="core/memory-safety">Memory Safety & Profiling</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
        <li><a href="imgui">ImGui Debug Overlays</a></li>
    </ul>
</div>

