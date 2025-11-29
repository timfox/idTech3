<?php
/**
 * Networking System Documentation
 */
$title = 'Networking - id Tech 3 Documentation';
$breadcrumbs = [
    '/networking' => 'Networking',
    '/networking/networking' => 'Networking System'
];
?>

<h1>Networking System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 networking system provides client-server architecture for multiplayer gameplay with prediction and lag compensation. Recent enhancements include modern HTTP/2 support, connection pooling, rate limiting, IPv6 improvements, and WebSocket support for real-time bidirectional communication.</p>
    
    <div class="feature-list">
        <h3>Recent Enhancements</h3>
        <ul>
            <li><strong>HTTP/2 Support:</strong> Multiplexing and header compression for faster downloads</li>
            <li><strong>Connection Pooling:</strong> Reuse HTTP connections to reduce overhead</li>
            <li><strong>Rate Limiting:</strong> Prevent server overload and blocking</li>
            <li><strong>IPv6 Improvements:</strong> Enhanced dual-stack support and preference settings</li>
            <li><strong>WebSocket Support:</strong> Real-time bidirectional communication</li>
            <li><strong>Network Statistics:</strong> Comprehensive monitoring and debugging tools</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Architecture</h2>
    <ul>
        <li><strong>Client-Server Model:</strong> Authoritative server with client prediction</li>
        <li><strong>UDP Protocol:</strong> Reliable and unreliable message delivery</li>
        <li><strong>Delta Compression:</strong> Efficient state synchronization</li>
        <li><strong>Lag Compensation:</strong> Server-side hit detection</li>
        <li><strong>Enhanced HTTP:</strong> Modern HTTP/2 with connection pooling and rate limiting</li>
        <li><strong>WebSocket:</strong> Real-time bidirectional communication for chat, updates, and administration</li>
    </ul>
</div>

<div class="section">
    <h2>Enhanced Networking Features</h2>
    
    <h3>HTTP/2 Support</h3>
    <p>HTTP/2 is automatically enabled when curl library supports it (version 7.33.0+). Benefits include:</p>
    <ul>
        <li>Multiplexing: Multiple requests over a single connection</li>
        <li>Header compression: Reduced overhead</li>
        <li>Better performance on high-latency connections (10-30% faster)</li>
    </ul>
    
    <div class="code-block">
        <pre><code># Enable HTTP/2 (default: enabled)
set cl_http2_enable 1
set cl_http2_prefer 1</code></pre>
    </div>
    
    <h3>Connection Pooling</h3>
    <p>Connection pooling reuses HTTP connections to reduce overhead and improve performance (20-50% faster for repeated downloads).</p>
    
    <div class="code-block">
        <pre><code># Configure connection pooling
set cl_connection_pool_enable 1
set cl_connection_pool_max 10        # Maximum pooled connections (1-50)
set cl_connection_pool_timeout 30000 # Idle timeout in ms</code></pre>
    </div>
    
    <h3>Rate Limiting</h3>
    <p>Rate limiting prevents overwhelming servers and helps avoid being blocked.</p>
    
    <div class="code-block">
        <pre><code># Configure rate limiting
set cl_rate_limit_enable 1
set cl_rate_limit_max_per_sec 10      # Max requests per second (1-100)
set cl_rate_limit_max_concurrent 5     # Max concurrent requests (1-20)</code></pre>
    </div>
    
    <h3>IPv6 Improvements</h3>
    <p>Enhanced IPv6 support with better dual-stack handling and preference settings.</p>
    
    <div class="code-block">
        <pre><code># Prefer IPv6 connections
set cl_prefer_ipv6 1</code></pre>
    </div>
    
    <h3>Network Statistics</h3>
    <p>Comprehensive network statistics tracking for monitoring and debugging.</p>
    
    <div class="code-block">
        <pre><code># Show network statistics
set cl_net_stats 1</code></pre>
    </div>
    
    <p>Statistics include:</p>
    <ul>
        <li>Total bytes sent/received</li>
        <li>Total requests (success/failed)</li>
        <li>Average response time</li>
        <li>HTTP/2 vs HTTP/1.1 usage</li>
        <li>IPv6 vs IPv4 usage</li>
    </ul>
</div>

<div class="section">
    <h2>WebSocket Support</h2>
    <p>WebSocket support enables real-time bidirectional communication between the game client and servers. Useful for:</p>
    <ul>
        <li>Real-time chat systems</li>
        <li>Live server updates</li>
        <li>Remote administration</li>
        <li>Real-time statistics</li>
        <li>Push notifications</li>
    </ul>
    
    <p>See <a href="networking/websocket">WebSocket Support</a> for detailed documentation.</p>
</div>

<div class="section">
    <h2>Server Setup</h2>
    <div class="code-block">
        <pre><code># Dedicated server
set dedicated 2
set net_port 27960
exec server.cfg</code></pre>
    </div>
</div>

<div class="section">
    <h2>Network Settings</h2>
    <div class="code-block">
        <pre><code># Client networking
seta rate "25000"
seta snaps "40"  
seta cl_maxpackets "125"

# Enhanced networking configuration
set cl_http2_enable 1
set cl_connection_pool_enable 1
set cl_rate_limit_enable 1
set cl_prefer_ipv6 0
set cl_net_stats 0</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="networking/websocket">WebSocket Support</a></li>
        <li><a href="core/structured-logging">Structured Logging</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
    </ul>
</div> 