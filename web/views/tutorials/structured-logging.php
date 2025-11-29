<?php
/**
 * Structured Logging Tutorial
 */
$title = 'Structured Logging Tutorial - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/structured-logging' => 'Structured Logging Tutorial'
];
?>

<h1>Structured Logging Tutorial</h1>

<div class="section">
    <h2>Introduction</h2>
    <p>This tutorial will guide you through setting up and using the structured logging system in id Tech 3. Structured logging provides better log management, filtering, and integration with external systems compared to the legacy <code>Com_Printf</code> system.</p>
    
    <div class="feature-list">
        <h3>What You'll Learn</h3>
        <ul>
            <li>How to enable and configure structured logging</li>
            <li>Using log levels and categories effectively</li>
            <li>Setting up JSON output for log aggregation</li>
            <li>Configuring log rotation</li>
            <li>Integrating with external logging systems</li>
            <li>Migrating from Com_Printf to structured logging</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 engine built and running</li>
        <li>Basic understanding of console commands (CVars)</li>
        <li>Access to configuration files</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Basic Setup</h2>
    
    <h3>Step 1: Enable Structured Logging</h3>
    <p>Structured logging is enabled by default, but you can verify or enable it:</p>
    <div class="code-block">
        <pre><code># Check if logging is enabled
/log_enable

# Enable logging (if disabled)
/set log_enable 1</code></pre>
    </div>
    
    <h3>Step 2: Set Log Level</h3>
    <p>Configure the minimum log level to control verbosity:</p>
    <div class="code-block">
        <pre><code># Show only warnings and errors (recommended for production)
/set log_level 2

# Show info, warnings, and errors (recommended for development)
/set log_level 1

# Show everything including debug messages (for debugging)
/set log_level 0</code></pre>
    </div>
    
    <h3>Step 3: Configure Output Format</h3>
    <p>Choose between human-readable text or machine-readable JSON:</p>
    <div class="code-block">
        <pre><code># Text format (default, human-readable)
/set log_format 0

# JSON format (for log aggregation systems)
/set log_format 1</code></pre>
    </div>
    
    <h3>Step 4: Set Output Destinations</h3>
    <p>Configure where logs are written:</p>
    <div class="code-block">
        <pre><code># Console only (default)
/set log_output 1

# File only
/set log_output 2

# Console and file (recommended)
/set log_output 3

# File and syslog (Linux/Unix)
/set log_output 6

# All destinations
/set log_output 7</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Using Log Categories</h2>
    
    <h3>Understanding Categories</h3>
    <p>Logs are organized into categories for better filtering. Available categories:</p>
    <ul>
        <li><code>general</code> - General engine messages</li>
        <li><code>client</code> - Client-side code</li>
        <li><code>server</code> - Server-side code</li>
        <li><code>renderer</code> - Rendering system</li>
        <li><code>network</code> - Network operations</li>
        <li><code>filesystem</code> - File system operations</li>
        <li><code>sound</code> - Audio system</li>
        <li><code>input</code> - Input handling</li>
        <li><code>physics</code> - Physics simulation</li>
        <li><code>ai</code> - AI/bot code</li>
        <li><code>script</code> - Scripting (Lua/QVM)</li>
        <li><code>memory</code> - Memory management</li>
    </ul>
    
    <h3>Filtering by Category</h3>
    <p>Enable or disable specific categories:</p>
    <div class="code-block">
        <pre><code># Disable network category logs
/set log_category_filter -network

# Enable only renderer and client categories
/set log_category_filter renderer,client

# Enable multiple categories, disable one
/set log_category_filter renderer,client,-network</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Log Rotation</h2>
    
    <h3>Setting Up Size-Based Rotation</h3>
    <p>Rotate logs when they reach a certain size:</p>
    <div class="code-block">
        <pre><code># Rotate when log file reaches 100MB (default)
/set log_rotation_size 100

# Rotate at 50MB
/set log_rotation_size 50

# Disable size-based rotation
/set log_rotation_size 0</code></pre>
    </div>
    
    <h3>Setting Up Time-Based Rotation</h3>
    <p>Rotate logs after a time period:</p>
    <div class="code-block">
        <pre><code># Rotate every 24 hours (default)
/set log_rotation_time 24

# Rotate every 12 hours
/set log_rotation_time 12

# Rotate daily at midnight (24 hours)
/set log_rotation_time 24

# Disable time-based rotation
/set log_rotation_time 0</code></pre>
    </div>
    
    <h3>Understanding Rotated Files</h3>
    <p>When rotation occurs, files are renamed:</p>
    <ul>
        <li><code>qconsole.log</code> - Current log file</li>
        <li><code>qconsole.log.1</code> - Most recent rotated file</li>
        <li><code>qconsole.log.2</code> - Second most recent</li>
        <li>... up to 10 rotated files</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: JSON Output for Log Aggregation</h2>
    
    <h3>Step 1: Enable JSON Format</h3>
    <div class="code-block">
        <pre><code>/set log_format 1
/set log_output 2  # File output for JSON logs</code></pre>
    </div>
    
    <h3>Step 2: View JSON Logs</h3>
    <p>JSON logs contain structured data:</p>
    <div class="code-block">
        <pre><code>{"timestamp":"2024-01-15T14:30:45+0000","level":"INFO","category":"renderer","file":"tr_image.c","line":123,"function":"LoadTexture","message":"Texture loaded: textures/common/caulk.tga"}
{"timestamp":"2024-01-15T14:30:46+0000","level":"WARN","category":"network","file":"cl_net_chan.c","line":456,"function":"CL_ParseServerMessage","message":"Received invalid packet"}</code></pre>
    </div>
    
    <h3>Step 3: Parse JSON Logs</h3>
    <p>Use standard JSON parsing tools:</p>
    <div class="code-block">
        <pre><code># Using jq (Linux/macOS)
cat qconsole.log | jq '.level, .category, .message'

# Using Python
import json
with open('qconsole.log') as f:
    for line in f:
        log = json.loads(line)
        print(f"{log['level']}: {log['message']}")</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Integration with External Systems</h2>
    
    <h3>Syslog Integration (Linux/Unix)</h3>
    <p>Send logs to system logging daemon:</p>
    <div class="code-block">
        <pre><code># Enable syslog output
/set log_output 4

# Or combine with file output
/set log_output 6</code></pre>
    </div>
    
    <h3>ELK Stack Integration</h3>
    <p>Configure Logstash to read JSON logs:</p>
    <div class="code-block">
        <pre><code># logstash.conf
input {
  file {
    path => "/path/to/qconsole.log"
    codec => "json_lines"
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
}</code></pre>
    </div>
    
    <h3>Grafana Loki Integration</h3>
    <p>Send logs to Loki for visualization:</p>
    <div class="code-block">
        <pre><code># promtail-config.yml
server:
  http_listen_port: 9080
  grpc_listen_port: 0

positions:
  filename: /tmp/positions.yaml

clients:
  - url: http://localhost:3100/loki/api/v1/push

scrape_configs:
  - job_name: idtech3
    static_configs:
      - targets:
          - localhost
        labels:
          job: idtech3
          __path__: /path/to/qconsole.log</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Developer Usage</h2>
    
    <h3>Using Structured Logging in Code</h3>
    <p>Replace <code>Com_Printf</code> calls with structured logging:</p>
    
    <h4>Before (Legacy)</h4>
    <div class="code-block">
        <pre><code>void LoadTexture(const char *name) {
    texture_t *tex = FindTexture(name);
    if (!tex) {
        Com_Printf("Warning: Texture %s not found\n", name);
        return;
    }
    Com_Printf("Texture %s loaded successfully\n", name);
}</code></pre>
    </div>
    
    <h4>After (Structured Logging)</h4>
    <div class="code-block">
        <pre><code>#include "q_log.h"

void LoadTexture(const char *name) {
    texture_t *tex = FindTexture(name);
    if (!tex) {
        Q_LogWarn(LOG_CATEGORY_RENDERER, "Texture %s not found", name);
        return;
    }
    Q_LogInfo(LOG_CATEGORY_RENDERER, "Texture %s loaded successfully", name);
}</code></pre>
    </div>
    
    <h3>Using Category-Specific Macros</h3>
    <div class="code-block">
        <pre><code>// Client-side logging
Q_LogClient(LOG_LEVEL_INFO, "Client connected to server");

// Server-side logging
Q_LogServer(LOG_LEVEL_WARN, "Server overloaded, reducing tick rate");

// Renderer logging
Q_LogRenderer(LOG_LEVEL_ERROR, "Shader compilation failed: %s", error);

// Network logging
Q_LogNetwork(LOG_LEVEL_DEBUG, "Packet received: %d bytes", size);</code></pre>
    </div>
    
    <h3>Advanced Logging with File/Line Information</h3>
    <div class="code-block">
        <pre><code>// Direct logging with full context
Q_Log(LOG_LEVEL_INFO, LOG_CATEGORY_RENDERER, __FILE__, __LINE__, __FUNCTION__,
      "Texture loaded: %s (width: %d, height: %d)", name, width, height);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Practical Examples</h2>
    
    <h3>Example 1: Development Setup</h3>
    <p>Configuration for active development:</p>
    <div class="code-block">
        <pre><code># Development configuration (add to autoexec.cfg)
set log_enable 1
set log_level 0          # Show all messages including debug
set log_format 0         # Text format for readability
set log_output 3         # Console and file
set log_file dev.log
set log_category_filter ""  # Show all categories</code></pre>
    </div>
    
    <h3>Example 2: Production Server Setup</h3>
    <p>Configuration for production servers:</p>
    <div class="code-block">
        <pre><code># Production server configuration
set log_enable 1
set log_level 2          # Warnings and errors only
set log_format 1         # JSON for log aggregation
set log_output 6         # File and syslog
set log_file server.log
set log_rotation_size 100
set log_rotation_time 24
set log_category_filter -network  # Reduce network log noise</code></pre>
    </div>
    
    <h3>Example 3: Debugging Specific System</h3>
    <p>Focus logs on a specific system:</p>
    <div class="code-block">
        <pre><code># Debug renderer issues
set log_level 0          # Show debug messages
set log_category_filter renderer  # Only renderer category
set log_file renderer_debug.log</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Troubleshooting</h2>
    
    <h3>Logs Not Appearing</h3>
    <p><strong>Problem:</strong> No logs are being written.</p>
    <p><strong>Solution:</strong></p>
    <ol>
        <li>Check <code>log_enable</code> is set to 1</li>
        <li>Verify log level is appropriate (try <code>log_level 0</code> to see all)</li>
        <li>Check category filter isn't excluding everything</li>
        <li>Verify output destination is configured</li>
        <li>Check file permissions if using file output</li>
    </ol>
    
    <h3>Too Many Logs</h3>
    <p><strong>Problem:</strong> Logs are too verbose.</p>
    <p><strong>Solution:</strong></p>
    <ol>
        <li>Increase log level: <code>/set log_level 2</code> (warnings/errors only)</li>
        <li>Filter categories: <code>/set log_category_filter -network,-filesystem</code></li>
        <li>Disable debug messages: Ensure log_level is 1 or higher</li>
    </ol>
    
    <h3>Log Rotation Not Working</h3>
    <p><strong>Problem:</strong> Log files keep growing.</p>
    <p><strong>Solution:</strong></p>
    <ol>
        <li>Verify rotation is enabled: <code>/log_rotation_size</code> or <code>/log_rotation_time</code></li>
        <li>Check file system permissions</li>
        <li>Ensure sufficient disk space</li>
        <li>Check log file isn't locked by another process</li>
    </ol>
    
    <h3>JSON Parsing Errors</h3>
    <p><strong>Problem:</strong> JSON logs can't be parsed.</p>
    <p><strong>Solution:</strong></p>
    <ol>
        <li>Verify <code>log_format</code> is set to 1</li>
        <li>Check for invalid characters in log messages</li>
        <li>Ensure proper JSON escaping in messages</li>
        <li>Use JSONL (JSON Lines) format - one JSON object per line</li>
    </ol>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li><strong>Use Appropriate Log Levels:</strong> DEBUG for detailed info, INFO for normal operation, WARN for recoverable issues, ERROR for problems</li>
        <li><strong>Choose Specific Categories:</strong> Use the most specific category rather than "general"</li>
        <li><strong>Include Context:</strong> Add relevant information to log messages (file names, IDs, counts)</li>
        <li><strong>Production Settings:</strong> Use higher log levels and JSON format in production</li>
        <li><strong>Development Settings:</strong> Use lower log levels and text format during development</li>
        <li><strong>Monitor Log Sizes:</strong> Enable rotation to prevent disk space issues</li>
        <li><strong>Filter Strategically:</strong> Use category filters to focus on relevant logs</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/structured-logging">Structured Logging Documentation</a> - Complete reference</li>
        <li><a href="imgui">ImGui Debug Overlays</a> - View logs in-game</li>
        <li><a href="development/debugging">Debugging Guide</a> - General debugging techniques</li>
        <li><a href="core/memory-safety">Memory Safety & Profiling</a> - Memory debugging</li>
    </ul>
</div>

