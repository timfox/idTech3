<?php
/**
 * Enhanced Networking Setup Tutorial
 */
$title = 'Enhanced Networking Setup Tutorial - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/enhanced-networking' => 'Enhanced Networking Setup'
];
?>

<h1>Enhanced Networking Setup Tutorial</h1>

<div class="section">
    <h2>Introduction</h2>
    <p>This tutorial will guide you through setting up and using the enhanced networking features in id Tech 3, including HTTP/2 support, connection pooling, rate limiting, and IPv6 improvements.</p>
    
    <div class="feature-list">
        <h3>What You'll Learn</h3>
        <ul>
            <li>Enabling HTTP/2 support</li>
            <li>Configuring connection pooling</li>
            <li>Setting up rate limiting</li>
            <li>Enabling IPv6 support</li>
            <li>Optimizing network performance</li>
            <li>Troubleshooting network issues</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 engine with enhanced networking compiled</li>
        <li>libcurl with HTTP/2 support</li>
        <li>Network access for testing</li>
        <li>Basic understanding of networking concepts</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: HTTP/2 Support</h2>
    
    <h3>Step 1: Verify HTTP/2 Support</h3>
    <p>Check if your build has HTTP/2 support:</p>
    <div class="code-block">
        <pre><code># Check curl version (should show HTTP/2 support)
curl --version

# Should show something like:
# Features: ... HTTP2 ...</code></pre>
    </div>
    
    <h3>Step 2: Enable HTTP/2</h3>
    <p>HTTP/2 is enabled by default if supported. Verify:</p>
    <div class="code-block">
        <pre><code># Check HTTP/2 status
/cl_http2_enable

# Enable if disabled
/set cl_http2_enable 1</code></pre>
    </div>
    
    <h3>Step 3: Test HTTP/2 Connection</h3>
    <p>Test with a server that supports HTTP/2:</p>
    <div class="code-block">
        <pre><code># Make a request and check protocol used
# Check console or network overlay for protocol info
/set cl_imgui_debug_network 1

# Protocol will show as "HTTP/2" if successful</code></pre>
    </div>
    
    <h3>Step 4: Benefits of HTTP/2</h3>
    <ul>
        <li><strong>Multiplexing:</strong> Multiple requests over single connection</li>
        <li><strong>Header Compression:</strong> Reduced overhead</li>
        <li><strong>Server Push:</strong> Server can push resources proactively</li>
        <li><strong>Better Performance:</strong> Faster than HTTP/1.1</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Connection Pooling</h2>
    
    <h3>Step 1: Enable Connection Pooling</h3>
    <p>Connection pooling reuses connections for better performance:</p>
    <div class="code-block">
        <pre><code># Enable connection pooling
/set cl_http_pool_enable 1

# Set maximum connections per host
/set cl_http_pool_max_connections 10

# Set connection timeout
/set cl_http_pool_timeout 30</code></pre>
    </div>
    
    <h3>Step 2: Configure Pool Settings</h3>
    <p>Optimize pool settings for your use case:</p>
    <div class="code-block">
        <pre><code># For high-traffic scenarios
/set cl_http_pool_max_connections 20

# For low-latency requirements
/set cl_http_pool_timeout 10

# For resource-constrained environments
/set cl_http_pool_max_connections 5</code></pre>
    </div>
    
    <h3>Step 3: Monitor Connection Pool</h3>
    <p>Use network overlay to monitor pool usage:</p>
    <div class="code-block">
        <pre><code># Enable network overlay
/set cl_imgui_debug_network 1

# Check connection pool statistics
# Shows: active connections, pool size, reuse rate</code></pre>
    </div>
    
    <h3>Step 4: Benefits</h3>
    <ul>
        <li><strong>Reduced Latency:</strong> Reuse existing connections</li>
        <li><strong>Lower Overhead:</strong> Fewer connection handshakes</li>
        <li><strong>Better Performance:</strong> Especially with HTTP/2</li>
        <li><strong>Resource Efficiency:</strong> Fewer open connections</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Rate Limiting</h2>
    
    <h3>Step 1: Enable Rate Limiting</h3>
    <p>Rate limiting prevents overwhelming servers:</p>
    <div class="code-block">
        <pre><code># Enable rate limiting
/set cl_http_rate_limit_enable 1

# Set requests per second limit
/set cl_http_rate_limit_rps 10

# Set burst size (requests allowed in burst)
/set cl_http_rate_limit_burst 20</code></pre>
    </div>
    
    <h3>Step 2: Configure Limits</h3>
    <p>Set appropriate limits for your use case:</p>
    
    <h4>Conservative (Low Traffic)</h4>
    <div class="code-block">
        <pre><code>/set cl_http_rate_limit_rps 5
/set cl_http_rate_limit_burst 10</code></pre>
    </div>
    
    <h4>Moderate (Normal Use)</h4>
    <div class="code-block">
        <pre><code>/set cl_http_rate_limit_rps 10
/set cl_http_rate_limit_burst 20</code></pre>
    </div>
    
    <h4>Aggressive (High Performance)</h4>
    <div class="code-block">
        <pre><code>/set cl_http_rate_limit_rps 50
/set cl_http_rate_limit_burst 100</code></pre>
    </div>
    
    <h3>Step 3: Monitor Rate Limiting</h3>
    <p>Check if requests are being rate limited:</p>
    <div class="code-block">
        <pre><code># Enable logging
/set log_enable 1
/set log_category_filter network

# Check for rate limit messages
# Messages like: "Rate limit exceeded, request queued"</code></pre>
    </div>
    
    <h3>Step 4: Disable if Needed</h3>
    <p>For testing or if causing issues:</p>
    <div class="code-block">
        <pre><code># Disable rate limiting
/set cl_http_rate_limit_enable 0</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: IPv6 Support</h2>
    
    <h3>Step 1: Enable IPv6</h3>
    <p>IPv6 support is enabled by default if available:</p>
    <div class="code-block">
        <pre><code># Check IPv6 status
/net_ipv6

# Enable IPv6
/set net_ipv6 1</code></pre>
    </div>
    
    <h3>Step 2: Verify IPv6 Availability</h3>
    <p>Check if your system supports IPv6:</p>
    <div class="code-block">
        <pre><code># Linux: Check for IPv6 interface
ip -6 addr show

# Check if IPv6 is enabled in kernel
cat /proc/sys/net/ipv6/conf/all/disable_ipv6
# Should show: 0 (enabled)</code></pre>
    </div>
    
    <h3>Step 3: Test IPv6 Connection</h3>
    <p>Connect to an IPv6 server:</p>
    <div class="code-block">
        <pre><code># Connect to IPv6 server
/connect [2001:db8::1]:27960

# Check connection info
/status
# Should show IPv6 address if using IPv6</code></pre>
    </div>
    
    <h3>Step 4: IPv6 Benefits</h3>
    <ul>
        <li><strong>Larger Address Space:</strong> More available addresses</li>
        <li><strong>Better Routing:</strong> Improved routing efficiency</li>
        <li><strong>Future-Proof:</strong> IPv6 is the future of networking</li>
        <li><strong>Dual Stack:</strong> Works alongside IPv4</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Complete Configuration</h2>
    
    <h3>Production Server Configuration</h3>
    <p>Recommended settings for production servers:</p>
    <div class="code-block">
        <pre><code>// Enhanced Networking Configuration
// HTTP/2 Support
set cl_http2_enable 1

// Connection Pooling
set cl_http_pool_enable 1
set cl_http_pool_max_connections 10
set cl_http_pool_timeout 30

// Rate Limiting
set cl_http_rate_limit_enable 1
set cl_http_rate_limit_rps 10
set cl_http_rate_limit_burst 20

// IPv6 Support
set net_ipv6 1

// Network Statistics
set cl_net_stats 1</code></pre>
    </div>
    
    <h3>Development Configuration</h3>
    <p>Settings for development/testing:</p>
    <div class="code-block">
        <pre><code>// Development Networking Configuration
set cl_http2_enable 1
set cl_http_pool_enable 1
set cl_http_pool_max_connections 5
set cl_http_rate_limit_enable 0  // Disable for testing
set net_ipv6 1
set cl_net_stats 1
set cl_imgui_debug_network 1  // Enable network overlay</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Performance Optimization</h2>
    
    <h3>Step 1: Enable All Features</h3>
    <div class="code-block">
        <pre><code>/set cl_http2_enable 1
/set cl_http_pool_enable 1
/set net_ipv6 1</code></pre>
    </div>
    
    <h3>Step 2: Tune Connection Pool</h3>
    <p>Adjust based on your server's capacity:</p>
    <ul>
        <li>Start with default values</li>
        <li>Monitor connection pool usage</li>
        <li>Increase if you see connection churn</li>
        <li>Decrease if hitting server limits</li>
    </ul>
    
    <h3>Step 3: Optimize Rate Limits</h3>
    <p>Find the right balance:</p>
    <ul>
        <li>Too low: Requests queued unnecessarily</li>
        <li>Too high: Risk overwhelming server</li>
        <li>Monitor server response times</li>
        <li>Adjust based on server capacity</li>
    </ul>
    
    <h3>Step 4: Monitor Performance</h3>
    <p>Use network overlay to track improvements:</p>
    <div class="code-block">
        <pre><code># Enable network overlay
/set cl_imgui_debug_network 1

# Monitor:
# - Request success rate
# - Average response time
# - Connection reuse rate
# - Protocol usage (HTTP/2 vs HTTP/1.1)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Troubleshooting</h2>
    
    <h3>HTTP/2 Not Working</h3>
    <p><strong>Problem:</strong> Still using HTTP/1.1.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify curl has HTTP/2 support: <code>curl --version</code></li>
        <li>Check server supports HTTP/2</li>
        <li>Verify <code>cl_http2_enable</code> is set to 1</li>
        <li>Check network overlay for protocol used</li>
        <li>Try forcing HTTP/2 if supported</li>
    </ul>
    
    <h3>Connection Pool Not Reusing</h3>
    <p><strong>Problem:</strong> New connections for each request.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify <code>cl_http_pool_enable</code> is enabled</li>
        <li>Check pool timeout isn't too short</li>
        <li>Ensure requests go to same host</li>
        <li>Monitor pool statistics in overlay</li>
    </ul>
    
    <h3>Rate Limiting Too Aggressive</h3>
    <p><strong>Problem:</strong> Requests being delayed.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Increase <code>cl_http_rate_limit_rps</code></li>
        <li>Increase <code>cl_http_rate_limit_burst</code></li>
        <li>Disable temporarily for testing: <code>/set cl_http_rate_limit_enable 0</code></li>
        <li>Check if server has its own rate limiting</li>
    </ul>
    
    <h3>IPv6 Connection Fails</h3>
    <p><strong>Problem:</strong> Cannot connect via IPv6.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify system has IPv6 enabled</li>
        <li>Check network supports IPv6</li>
        <li>Verify server supports IPv6</li>
        <li>Try IPv4 fallback: <code>/set net_ipv6 0</code></li>
        <li>Check firewall allows IPv6</li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li><strong>Enable HTTP/2:</strong> Always use HTTP/2 when available</li>
        <li><strong>Use Connection Pooling:</strong> Enable for better performance</li>
        <li><strong>Set Appropriate Rate Limits:</strong> Balance between performance and server protection</li>
        <li><strong>Monitor Network Stats:</strong> Use overlay to track performance</li>
        <li><strong>Test Both IPv4 and IPv6:</strong> Ensure compatibility</li>
        <li><strong>Adjust Based on Use Case:</strong> Different settings for dev vs production</li>
        <li><strong>Keep Updated:</strong> Update curl/libcurl for latest features</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="networking/networking">Enhanced Networking Documentation</a> - Complete reference</li>
        <li><a href="networking/websocket">WebSocket Support</a> - Real-time communication</li>
        <li><a href="tutorials/websocket">WebSocket Tutorial</a> - WebSocket integration guide</li>
        <li><a href="imgui">ImGui Debug Overlays</a> - Network monitoring</li>
    </ul>
</div>

