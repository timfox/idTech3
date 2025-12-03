<?php
/**
 * Complete Networking Documentation
 */
$title = 'Complete Networking Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/networking' => 'Networking',
    '/networking/complete-networking' => 'Complete Networking Guide'
];
?>

<div class="content-section">
    <h1>Complete Networking Guide</h1>
    
    <blockquote>
        <strong>Network Architecture:</strong> This guide covers all aspects of networking in id Tech 3, including client-server architecture, protocol details, modern enhancements, and optimization techniques.
    </blockquote>

    <div class="section">
        <h2>Network Architecture Overview</h2>
        <p>The id Tech 3 engine uses an authoritative client-server architecture designed for multiplayer gaming:</p>
        
        <div class="feature-list">
            <h3>Core Principles</h3>
            <ul>
                <li><strong>Server Authority:</strong> All game state decisions made server-side</li>
                <li><strong>Client Prediction:</strong> Client predicts movement for responsiveness</li>
                <li><strong>Lag Compensation:</strong> Server rewinds time for accurate hit detection</li>
                <li><strong>Snapshot System:</strong> Delta-compressed state updates</li>
                <li><strong>UDP Transport:</strong> Low-latency UDP with reliable message channels</li>
            </ul>
        </div>
    </div>

    <div class="section">
        <h2>Client-Server Model</h2>
        
        <h3>Server Responsibilities</h3>
        <ul>
            <li>Authoritative game state management</li>
            <li>Physics simulation and collision detection</li>
            <li>Hit detection with lag compensation</li>
            <li>Snapshot generation and compression</li>
            <li>Client connection management</li>
            <li>Anti-cheat measures</li>
            <li>DoS protection</li>
        </ul>

        <h3>Client Responsibilities</h3>
        <ul>
            <li>User input collection</li>
            <li>Client-side prediction</li>
            <li>Rendering and presentation</li>
            <li>Sound playback</li>
            <li>UI rendering</li>
            <li>Interpolation between snapshots</li>
        </ul>
    </div>

    <div class="section">
        <h2>Protocol Details</h2>
        
        <h3>UDP Transport</h3>
        <ul>
            <li>Low-latency UDP packets</li>
            <li>Reliable message channels for critical data</li>
            <li>Sequence numbers for packet ordering</li>
            <li>Automatic retransmission for reliable messages</li>
            <li>Connectionless design with connection tracking</li>
        </ul>

        <h3>Snapshot System</h3>
        <p>Delta-compressed game state updates:</p>
        <ul>
            <li><strong>Full Snapshots:</strong> Complete game state (periodic)</li>
            <li><strong>Delta Snapshots:</strong> Changes since last acknowledged snapshot</li>
            <li><strong>Compression:</strong> Efficient delta encoding</li>
            <li><strong>Interpolation:</strong> Smooth rendering between snapshots</li>
        </ul>

        <h3>Message Types</h3>
        <ul>
            <li><strong>Client Commands:</strong> User input, movement</li>
            <li><strong>Server Snapshots:</strong> Game state updates</li>
            <li><strong>Reliable Messages:</strong> Chat, commands, downloads</li>
            <li><strong>Download Messages:</strong> File transfers</li>
        </ul>
    </div>

    <div class="section">
        <h2>Modern Enhancements</h2>
        
        <h3>HTTP/2 Support</h3>
        <p><strong>New:</strong> HTTP/2 for faster downloads:</p>
        <ul>
            <li>Multiplexing multiple requests over single connection</li>
            <li>Header compression</li>
            <li>Server push support</li>
            <li>10-30% faster on high-latency connections</li>
            <li>Automatic fallback to HTTP/1.1</li>
        </ul>

        <h3>Connection Pooling</h3>
        <p><strong>New:</strong> Reuse connections for faster downloads:</p>
        <ul>
            <li>20-50% faster downloads</li>
            <li>Reduced connection overhead</li>
            <li>Automatic connection management</li>
            <li>Configurable pool size</li>
        </ul>

        <h3>Rate Limiting</h3>
        <p><strong>Enhanced:</strong> Improved DoS protection:</p>
        <ul>
            <li>Per-client rate limits</li>
            <li>Configurable limits</li>
            <li>Automatic throttling</li>
            <li>Connection timeout handling</li>
        </ul>

        <h3>IPv6 Support</h3>
        <p><strong>Enhanced:</strong> Improved IPv6 handling:</p>
        <ul>
            <li>Dual-stack support (IPv4/IPv6)</li>
            <li>Proper IPv6 address handling</li>
            <li>IPv6-only server support</li>
            <li>Automatic protocol selection</li>
        </ul>

        <h3>WebSocket Support</h3>
        <p><strong>New:</strong> Real-time bidirectional communication:</p>
        <ul>
            <li>WebSocket protocol support</li>
            <li>Real-time server communication</li>
            <li>Event-driven architecture</li>
            <li>Automatic reconnection</li>
        </ul>
        <p>See <a href="networking/websocket">WebSocket Support</a> for details.</p>
    </div>

    <div class="section">
        <h2>Client-Side Prediction</h2>
        
        <h3>How It Works</h3>
        <p>Client predicts movement immediately while waiting for server confirmation:</p>
        <ul>
            <li>Immediate response to user input</li>
            <li>Server reconciliation when snapshot arrives</li>
            <li>Smooth correction of prediction errors</li>
            <li>Configurable prediction depth</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>set cl_predict "1"                // Enable prediction
set cl_maxpackets "30"            // Max packets per second
set cl_packetdup "0"              // Packet duplication</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Lag Compensation</h2>
        
        <h3>How It Works</h3>
        <p>Server rewinds time to client's view when performing hit detection:</p>
        <ul>
            <li>Stores entity history</li>
            <li>Rewinds to client's view time</li>
            <li>Performs hit detection</li>
            <li>Restores current state</li>
            <li>Configurable history length</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>set g_lagCompensation "1"         // Enable lag compensation
set sv_lagCompensationTime "200"  // History length (ms)</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Interpolation</h2>
        
        <h3>Smooth Rendering</h3>
        <p>Client interpolates between snapshots for smooth rendering:</p>
        <ul>
            <li>Configurable interpolation delay</li>
            <li>Smooth entity movement</li>
            <li>Reduced jitter</li>
            <li>Backward extrapolation for late snapshots</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>set cl_timenudge "0"               // Time adjustment
set cl_interpolate "1"             // Enable interpolation
set cl_interpolateTime "100"       // Interpolation delay (ms)</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Performance Optimization</h2>
        
        <h3>Network Optimization</h3>
        <ul>
            <li><strong>Delta Compression:</strong> Only send changes</li>
            <li><strong>Entity Culling:</strong> Don't send invisible entities</li>
            <li><strong>Rate Limiting:</strong> Control update frequency</li>
            <li><strong>Packet Batching:</strong> Combine multiple messages</li>
            <li><strong>Compression:</strong> Compress large messages</li>
        </ul>

        <h3>Server Optimization</h3>
        <ul>
            <li>Efficient snapshot generation</li>
            <li>Delta compression optimization</li>
            <li>Connection pooling</li>
            <li>Reduced memory usage</li>
            <li>DoS protection</li>
        </ul>

        <h3>Client Optimization</h3>
        <ul>
            <li>Efficient prediction</li>
            <li>Smart interpolation</li>
            <li>Connection reuse</li>
            <li>HTTP/2 multiplexing</li>
        </ul>
    </div>

    <div class="section">
        <h2>Security Features</h2>
        
        <h3>DoS Protection</h3>
        <ul>
            <li>Rate limiting per client</li>
            <li>Connection limits</li>
            <li>Resource limits</li>
            <li>Automatic timeout handling</li>
        </ul>

        <h3>Anti-Cheat</h3>
        <ul>
            <li>Server-side validation</li>
            <li>Movement validation</li>
            <li>Speed limits</li>
            <li>Position validation</li>
        </ul>
    </div>

    <div class="section">
        <h2>Configuration</h2>
        
        <h3>Client CVARs</h3>
        <div class="code-block">
            <pre><code>// Connection
set cl_maxpackets "30"            // Max packets/sec
set cl_packetdup "0"              // Packet duplication
set cl_timeout "200"              // Connection timeout

// Prediction
set cl_predict "1"                // Enable prediction
set cl_predictTime "100"          // Prediction time (ms)

// Interpolation
set cl_interpolate "1"             // Enable interpolation
set cl_interpolateTime "100"       // Interpolation delay</code></pre>
        </div>

        <h3>Server CVARs</h3>
        <div class="code-block">
            <pre><code>// Network
set sv_maxclients "64"            // Max clients
set sv_fps "20"                   // Server tick rate
set sv_lagCompensationTime "200"  // Lag compensation

// Rate Limiting
set sv_rateLimit "10000"          // Rate limit (bytes/sec)
set sv_maxRate "25000"            // Max rate per client</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="networking/networking">Networking</a> - Network system overview</li>
            <li><a href="networking/websocket">WebSocket Support</a> - WebSocket documentation</li>
            <li><a href="server/setup">Server Setup</a> - Server configuration</li>
            <li><a href="engine/architecture">Engine Architecture</a> - System architecture</li>
        </ul>
    </div>
</div>

