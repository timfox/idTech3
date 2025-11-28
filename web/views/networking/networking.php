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
    <p>The id Tech 3 networking system provides client-server architecture for multiplayer gameplay with prediction and lag compensation.</p>
</div>

<div class="section">
    <h2>Architecture</h2>
    <ul>
        <li><strong>Client-Server Model:</strong> Authoritative server with client prediction</li>
        <li><strong>UDP Protocol:</strong> Reliable and unreliable message delivery</li>
        <li><strong>Delta Compression:</strong> Efficient state synchronization</li>
        <li><strong>Lag Compensation:</strong> Server-side hit detection</li>
    </ul>
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
seta cl_maxpackets "125"</code></pre>
    </div>
</div> 