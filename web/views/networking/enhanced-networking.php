<?php
$title = "Enhanced Networking";
?>

<h1>Enhanced Networking</h1>

<p>This document describes the modern networking enhancements added to the engine.</p>

<h2>Overview</h2>

<p>The enhanced networking system adds HTTP/2 support, connection pooling, rate limiting, improved IPv6 handling, and network statistics tracking to improve download performance and reliability.</p>

<h2>Features Implemented</h2>

<h3>1. HTTP/2 Support</h3>
<p><strong>Status:</strong> ✅ Implemented</p>

<p>HTTP/2 support is automatically enabled when:</p>
<ul>
    <li>curl library supports HTTP/2 (version 7.33.0+)</li>
    <li><code>cl_http2_enable</code> cvar is set to 1 (default: 1)</li>
</ul>

<h4>CVars:</h4>
<ul>
    <li><code>cl_http2_enable</code> - Enable HTTP/2 support (default: 1)</li>
    <li><code>cl_http2_prefer</code> - Prefer HTTP/2 over HTTP/1.1 when both available (default: 1)</li>
</ul>

<h4>Benefits:</h4>
<ul>
    <li>Multiplexing: Multiple requests over a single connection</li>
    <li>Header compression: Reduced overhead</li>
    <li>Server push: Potential for faster content delivery</li>
    <li>Better performance on high-latency connections</li>
</ul>

<h3>2. Connection Pooling</h3>
<p><strong>Status:</strong> ✅ Implemented</p>

<p>Connection pooling reuses HTTP connections to reduce overhead and improve performance.</p>

<h4>CVars:</h4>
<ul>
    <li><code>cl_connection_pool_enable</code> - Enable connection pooling (default: 1)</li>
    <li><code>cl_connection_pool_max</code> - Maximum pooled connections (default: 10, range: 1-50)</li>
    <li><code>cl_connection_pool_timeout</code> - Idle connection timeout in ms (default: 30000, range: 1000-300000)</li>
</ul>

<h4>How it works:</h4>
<ul>
    <li>Connections are reused for requests to the same hostname/port</li>
    <li>Idle connections are kept alive for faster subsequent requests</li>
    <li>Old idle connections are automatically cleaned up</li>
    <li>Limits prevent excessive connection usage</li>
</ul>

<h4>Benefits:</h4>
<ul>
    <li>Reduced connection establishment overhead</li>
    <li>Faster downloads from the same server</li>
    <li>Better resource utilization</li>
</ul>

<h3>3. Rate Limiting</h3>
<p><strong>Status:</strong> ✅ Implemented</p>

<p>Rate limiting prevents overwhelming servers and helps avoid being blocked.</p>

<h4>CVars:</h4>
<ul>
    <li><code>cl_rate_limit_enable</code> - Enable rate limiting (default: 1)</li>
    <li><code>cl_rate_limit_max_per_sec</code> - Maximum requests per second (default: 10, range: 1-100)</li>
    <li><code>cl_rate_limit_max_concurrent</code> - Maximum concurrent requests (default: 5, range: 1-20)</li>
</ul>

<h4>How it works:</h4>
<ul>
    <li>Tracks requests per second in a sliding window</li>
    <li>Limits concurrent active requests</li>
    <li>Automatically throttles when limits are reached</li>
</ul>

<h4>Benefits:</h4>
<ul>
    <li>Prevents server overload</li>
    <li>Reduces risk of being blocked</li>
    <li>More polite network behavior</li>
</ul>

<h3>4. IPv6 Improvements</h3>
<p><strong>Status:</strong> ✅ Implemented</p>

<p>Enhanced IPv6 support with better dual-stack handling and preference settings.</p>

<h4>CVars:</h4>
<ul>
    <li><code>cl_prefer_ipv6</code> - Prefer IPv6 connections over IPv4 (default: 0)</li>
</ul>

<h4>Features:</h4>
<ul>
    <li>Automatic IPv6 detection and support</li>
    <li>Configurable IPv6 preference</li>
    <li>Dual-stack support (IPv4 and IPv6)</li>
    <li>Better connection handling for IPv6 addresses</li>
</ul>

<h4>Benefits:</h4>
<ul>
    <li>Future-proof networking</li>
    <li>Better connectivity in IPv6-only environments</li>
    <li>Improved performance on IPv6 networks</li>
</ul>

<h3>5. Network Statistics</h3>
<p><strong>Status:</strong> ✅ Implemented</p>

<p>Comprehensive network statistics tracking for monitoring and debugging.</p>

<h4>CVar:</h4>
<ul>
    <li><code>cl_net_stats</code> - Show network statistics (default: 0)</li>
</ul>

<h4>Tracked Metrics:</h4>
<ul>
    <li>Total bytes sent/received</li>
    <li>Total requests (success/failed)</li>
    <li>Average response time</li>
    <li>HTTP/2 vs HTTP/1.1 usage</li>
    <li>IPv6 vs IPv4 usage</li>
</ul>

<h4>Benefits:</h4>
<ul>
    <li>Performance monitoring</li>
    <li>Debugging network issues</li>
    <li>Understanding connection patterns</li>
</ul>

<h2>Usage</h2>

<h3>Automatic Usage</h3>
<p>The enhanced networking is automatically used when:</p>
<ol>
    <li>curl is enabled (<code>USE_CURL</code> is defined)</li>
    <li>Enhanced networking is initialized (happens automatically on first download)</li>
</ol>

<h3>Manual Configuration</h3>
<p>Users can configure behavior via CVars:</p>
<pre><code>// Enable HTTP/2
set cl_http2_enable 1
set cl_http2_prefer 1

// Configure connection pooling
set cl_connection_pool_enable 1
set cl_connection_pool_max 10
set cl_connection_pool_timeout 30000

// Configure rate limiting
set cl_rate_limit_enable 1
set cl_rate_limit_max_per_sec 10
set cl_rate_limit_max_concurrent 5

// Prefer IPv6
set cl_prefer_ipv6 1

// Show statistics
set cl_net_stats 1</code></pre>

<h2>Technical Details</h2>

<h3>Architecture</h3>
<p>The enhanced networking system consists of:</p>
<ol>
    <li><code>cl_net_enhanced.h</code> - Header file with type definitions and function prototypes</li>
    <li><code>cl_net_enhanced.c</code> - Implementation of enhanced features</li>
    <li><strong>Integration</strong> - Seamless integration with existing <code>cl_curl.c</code> code</li>
</ol>

<h3>Compatibility</h3>
<ul>
    <li><strong>Backward Compatible:</strong> Falls back to standard curl behavior if enhanced features fail</li>
    <li><strong>Graceful Degradation:</strong> Works even if HTTP/2 is not supported</li>
    <li><strong>No Breaking Changes:</strong> Existing code continues to work</li>
</ul>

<h3>Dependencies</h3>
<ul>
    <li>curl library (already required)</li>
    <li>curl version 7.33.0+ for HTTP/2 support (optional, gracefully degrades)</li>
</ul>

<h2>Performance Impact</h2>

<h3>Expected Improvements</h3>
<ul>
    <li><strong>Connection Pooling:</strong> 20-50% faster for repeated downloads from same server</li>
    <li><strong>HTTP/2:</strong> 10-30% faster on high-latency connections</li>
    <li><strong>Rate Limiting:</strong> Prevents server overload, may slightly slow aggressive downloads</li>
</ul>

<h3>Overhead</h3>
<ul>
    <li>Minimal memory overhead (~few KB per pooled connection)</li>
    <li>Negligible CPU overhead</li>
    <li>Network statistics add minimal tracking overhead</li>
</ul>

<h2>Future Enhancements</h2>

<ul>
    <li><strong>WebSocket Implementation</strong> - Add libwebsockets integration</li>
    <li><strong>HTTP/3 Support</strong> - When curl adds HTTP/3 support</li>
    <li><strong>Advanced Statistics</strong> - More detailed metrics and visualization</li>
    <li><strong>Adaptive Rate Limiting</strong> - Dynamic rate limiting based on server response</li>
    <li><strong>Connection Health Monitoring</strong> - Detect and replace bad connections</li>
</ul>

<h2>See Also</h2>

<ul>
    <li><a href="networking/complete-networking">Complete Networking Guide</a></li>
    <li><a href="networking/networking">Networking</a></li>
    <li><a href="networking/websocket">WebSocket Support</a></li>
</ul>

