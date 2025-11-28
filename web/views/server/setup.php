<?php
/**
 * Server Setup Documentation
 */
$title = 'Server Setup - ET Legacy Documentation';
$breadcrumbs = [
    '/server' => 'Server',
    '/server/setup' => 'Server Setup'
];
?>

<h1>Server Setup</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Setting up an ET Legacy server allows you to host multiplayer games with custom configurations, maps, and mods. This guide covers installation, configuration, and administration of dedicated servers.</p>
    
    <div class="feature-list">
        <h3>Server Features</h3>
        <ul>
            <li><strong>Dedicated Server:</strong> Headless server for optimal performance</li>
            <li><strong>Mod Support:</strong> ETpro, ETPub, Jaymod, and custom mods</li>
            <li><strong>Remote Administration:</strong> RCON and web-based management</li>
            <li><strong>Custom Maps:</strong> Support for community and custom maps</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>System Requirements</h2>
    
    <h3>Minimum Requirements</h3>
    <ul>
        <li><strong>OS:</strong> Windows Server 2012+, Linux (Ubuntu 18.04+, CentOS 7+)</li>
        <li><strong>CPU:</strong> Dual-core 2.0 GHz</li>
        <li><strong>RAM:</strong> 2 GB (1 GB per 20 players)</li>
        <li><strong>Storage:</strong> 4 GB free space</li>
        <li><strong>Network:</strong> 100 Mbps connection minimum</li>
    </ul>
    
    <h3>Recommended Specifications</h3>
    <ul>
        <li><strong>CPU:</strong> Quad-core 3.0 GHz or higher</li>
        <li><strong>RAM:</strong> 8 GB (allows multiple server instances)</li>
        <li><strong>Storage:</strong> SSD for faster map loading</li>
        <li><strong>Network:</strong> 1 Gbps connection for optimal performance</li>
    </ul>
</div>

<div class="section">
    <h2>Installation</h2>
    
    <h3>Linux Installation</h3>
    <div class="code-block">
        <pre><code># Ubuntu/Debian installation
sudo apt update
sudo apt install wget unzip

# Download ET Legacy dedicated server
wget https://www.etlegacy.com/download/etlegacy-linux-x86_64.tar.gz
tar xzf etlegacy-linux-x86_64.tar.gz
cd etlegacy/

# Create server directory
mkdir -p ~/.etlegacy/etmain

# Copy ET assets (you need original ET installation)
cp /path/to/et/etmain/*.pk3 ~/.etlegacy/etmain/</code></pre>
    </div>
    
    <h3>Windows Installation</h3>
    <div class="code-block">
        <pre><code># PowerShell installation
# Download ET Legacy from official website
# Extract to desired directory (e.g., C:\ETLegacy\)

# Create server configuration
mkdir C:\ETLegacy\etmain
mkdir C:\ETLegacy\legacy

# Copy ET assets
copy "C:\Program Files\Wolfenstein - Enemy Territory\etmain\*.pk3" C:\ETLegacy\etmain\</code></pre>
    </div>
    
    <h3>Docker Installation</h3>
    <div class="code-block">
        <pre><code># Docker installation
docker pull etlegacy/server:latest

# Run server container
docker run -d \
  --name etlegacy-server \
  -p 27960:27960/udp \
  -v /path/to/etmain:/app/etmain:ro \
  -v /path/to/server-config:/app/legacy \
  etlegacy/server:latest</code></pre>
    </div>
</div>

<div class="section">
    <h2>Basic Configuration</h2>
    
    <h3>Server Configuration File</h3>
    <p>Create <code>legacy/server.cfg</code> with basic settings:</p>
    <div class="code-block">
        <pre><code>// Basic server configuration
set sv_hostname "My ET Legacy Server"
set sv_maxclients "32"
set g_password ""
set rconpassword "your_secure_rcon_password"

// Game settings
set g_gametype "6"           // Objective mode
set timelimit "20"           // Match time limit
set g_friendlyfire "1"       // Enable friendly fire
set g_antilag "1"            // Enable antilag

// Map rotation
set g_campaign "legacy1"     // Default campaign

// Logging
set g_log "server.log"
set g_logsync "2"

// Network settings
set sv_fps "20"              // Server FPS
set sv_pure "1"              // Pure server mode
set sv_floodprotect "1"      // Flood protection

// Administration
set g_tyranny "1"            // Referee system
set vote_limit "5"           // Vote limitations</code></pre>
    </div>
    
    <h3>Campaign Configuration</h3>
    <p>Create <code>legacy/campaigncycle.cfg</code> for map rotation:</p>
    <div class="code-block">
        <pre><code>// Campaign rotation configuration
set d1 "set g_campaign legacy1 ; campaign legacy1 ; set nextcampaign vstr d2"
set d2 "set g_campaign legacy2 ; campaign legacy2 ; set nextcampaign vstr d3"  
set d3 "set g_campaign legacy3 ; campaign legacy3 ; set nextcampaign vstr d1"
set nextcampaign "vstr d1"
vstr nextcampaign</code></pre>
    </div>
</div>

<div class="section">
    <h2>Starting the Server</h2>
    
    <h3>Linux Startup</h3>
    <div class="code-block">
        <pre><code># Direct startup
./etlded +set dedicated 2 +set net_port 27960 +exec server.cfg

# With screen (recommended for persistent servers)
screen -S etlegacy ./etlded +set dedicated 2 +exec server.cfg

# Systemd service (create /etc/systemd/system/etlegacy.service)
[Unit]
Description=ET Legacy Server
After=network.target

[Service]
Type=simple
User=etlegacy
WorkingDirectory=/home/etlegacy/etlegacy
ExecStart=/home/etlegacy/etlegacy/etlded +set dedicated 2 +exec server.cfg
Restart=always

[Install]
WantedBy=multi-user.target</code></pre>
    </div>
    
    <h3>Windows Startup</h3>
    <div class="code-block">
        <pre><code># Batch file startup (start-server.bat)
@echo off
cd /d "C:\ETLegacy"
etlded.exe +set dedicated 2 +set net_port 27960 +exec server.cfg
pause

# Windows Service
# Use NSSM (Non-Sucking Service Manager) to create Windows service
nssm install ETLegacy "C:\ETLegacy\etlded.exe"
nssm set ETLegacy Parameters "+set dedicated 2 +exec server.cfg"
nssm set ETLegacy AppDirectory "C:\ETLegacy"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Advanced Configuration</h2>
    
    <h3>Mod Configuration</h3>
    <p>Setting up popular mods:</p>
    <div class="code-block">
        <pre><code># ETpro configuration
set fs_game "etpro"
exec etpro.cfg

# ETPub configuration  
set fs_game "etpub"
exec etpub.cfg

# Jaymod configuration
set fs_game "jaymod"
exec jaymod.cfg

# Silent mod configuration
set fs_game "silent"
exec silent.cfg</code></pre>
    </div>
    
    <h3>Anti-Cheat Configuration</h3>
    <div class="code-block">
        <pre><code>// Anti-cheat settings
set sv_pure "1"              // Pure server mode
set pb_sv_enable "1"         // PunkBuster (if available)
set g_censorpenalty "5"      // Penalty for censored names
set g_ipcomplaintlimit "3"   // IP complaint limit
set g_complaintlimit "6"     // General complaint limit
set g_maxwarnings "3"        // Warning system</code></pre>
    </div>
    
    <h3>Performance Optimization</h3>
    <div class="code-block">
        <pre><code>// Performance settings
set sv_fps "20"              // Server FPS (higher = more responsive)
set com_hunkmegs "128"       // Memory allocation
set com_soundmegs "64"       // Sound memory
set com_zonemegs "32"        // Zone memory
set rate "25000"             // Default player rate
set sv_dl_maxRate "1000000"  // Download rate limit</code></pre>
    </div>
</div>

<div class="section">
    <h2>Remote Administration</h2>
    
    <h3>RCON Commands</h3>
    <div class="code-block">
        <pre><code># RCON usage
rcon your_password status        // Show player list
rcon your_password kick 2        // Kick player by slot number
rcon your_password ban 2         // Ban player by slot
rcon your_password map oasis     // Change map
rcon your_password restart      // Restart map
rcon your_password nextmap      // Go to next map
rcon your_password rcon_password new_pass  // Change RCON password</code></pre>
    </div>
    
    <h3>Admin Levels</h3>
    <div class="code-block">
        <pre><code>// Admin system configuration
set g_admin "admin.dat"          // Admin file
set g_adminlog "admin.log"       // Admin log file
set g_adminchat "1"              // Enable admin chat

// Admin levels (in admin.dat)
1:your_guid:name:100:immune     // Level 100 admin
2:other_guid:name:50:kick       // Level 50 admin with kick</code></pre>
    </div>
    
    <h3>Web Administration</h3>
    <p>Setting up web-based server management:</p>
    <div class="code-block">
        <pre><code># Enable web admin (if supported by mod)
set g_protect "1"
set g_webAdmin "1" 
set g_webAdminPort "8080"
set g_webAdminPassword "secure_web_password"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Map Management</h2>
    
    <h3>Installing Custom Maps</h3>
    <div class="code-block">
        <pre><code># Map installation process
1. Download map .pk3 file
2. Place in etmain/ directory (or mod directory)
3. Add to map rotation in server.cfg

# Map rotation example
set m1 "map oasis ; set nextmap vstr m2"
set m2 "map battery ; set nextmap vstr m3"  
set m3 "map goldrush ; set nextmap vstr m1"
set nextmap "vstr m1"
vstr nextmap</code></pre>
    </div>
    
    <h3>Map Download Configuration</h3>
    <div class="code-block">
        <pre><code>// Auto-download settings
set sv_allowdownload "1"        // Enable downloads
set sv_wwwDownload "1"          // Enable HTTP downloads
set sv_wwwBaseURL "http://yourserver.com/etmaps/"  // HTTP download URL
set sv_wwwDlDisconnected "0"    // Don't disconnect during download</code></pre>
    </div>
</div>

<div class="section">
    <h2>Security and Maintenance</h2>
    
    <h3>Security Best Practices</h3>
    <ul>
        <li><strong>Strong RCON Password:</strong> Use complex passwords</li>
        <li><strong>Admin Access:</strong> Limit admin privileges</li>
        <li><strong>Regular Updates:</strong> Keep server software updated</li>
        <li><strong>Firewall:</strong> Configure proper firewall rules</li>
        <li><strong>Log Monitoring:</strong> Regular log file review</li>
    </ul>
    
    <h3>Backup and Recovery</h3>
    <div class="code-block">
        <pre><code># Backup script example (Linux)
#!/bin/bash
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="/backups/etlegacy"
SERVER_DIR="/home/etlegacy/etlegacy"

# Create backup
tar czf "$BACKUP_DIR/etlegacy_backup_$DATE.tar.gz" \
  "$SERVER_DIR/legacy/" \
  "$SERVER_DIR/etmain/" \
  "$SERVER_DIR/*.cfg"

# Keep only last 30 days of backups
find "$BACKUP_DIR" -name "*.tar.gz" -mtime +30 -delete</code></pre>
    </div>
    
    <h3>Log Rotation</h3>
    <div class="code-block">
        <pre><code># Logrotate configuration (/etc/logrotate.d/etlegacy)
/home/etlegacy/etlegacy/legacy/*.log {
    daily
    rotate 30
    compress
    missingok
    notifempty
    create 644 etlegacy etlegacy
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Common Issues</h3>
    <div class="troubleshooting">
        <h4>Server not showing in browser</h4>
        <ul>
            <li>Check firewall settings (port 27960 UDP)</li>
            <li>Verify sv_master settings</li>
            <li>Ensure heartbeat is enabled</li>
        </ul>
        
        <h4>Players can't connect</h4>
        <ul>
            <li>Check server password settings</li>
            <li>Verify sv_maxclients not exceeded</li>
            <li>Review ban list</li>
        </ul>
        
        <h4>Poor server performance</h4>
        <ul>
            <li>Increase sv_fps setting</li>
            <li>Optimize memory settings</li>
            <li>Check CPU and network usage</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="server/configuration">Server Configuration</a></li>
        <li><a href="server/rcon">RCON Administration</a></li>
        <li><a href="server/logging">Server Logging</a></li>
        <li><a href="et-legacy/overview">ET Legacy Overview</a></li>
    </ul>
</div> 