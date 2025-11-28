<?php
/**
 * ET Legacy Overview Documentation
 */
$title = 'ET Legacy Overview - id Tech 3 Documentation';
$breadcrumbs = [
    '/et-legacy' => 'ET Legacy',
    '/et-legacy/overview' => 'Overview'
];
?>

<h1>ET Legacy Overview</h1>

<div class="section">
    <h2>What is ET Legacy?</h2>
    <p><strong>ET Legacy</strong> is a modern, community-maintained fork of the Wolfenstein: Enemy Territory game engine. It builds upon id Tech 3 to provide enhanced graphics, improved gameplay mechanics, and modern platform support while maintaining compatibility with the original ET experience.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Modern Engine:</strong> Updated id Tech 3 with modern rendering</li>
            <li><strong>Enhanced Graphics:</strong> Improved lighting, shadows, and effects</li>
            <li><strong>Cross-Platform:</strong> Windows, Linux, macOS support</li>
            <li><strong>Mod Support:</strong> Compatible with existing ET mods</li>
            <li><strong>Active Development:</strong> Regular updates and bug fixes</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Game Overview</h2>
    
    <h3>Objective-Based Gameplay</h3>
    <p>Wolfenstein: Enemy Territory features team-based objective gameplay where Allied and Axis forces compete to complete or prevent mission objectives.</p>
    
    <h3>Class System</h3>
    <ul>
        <li><strong>Soldier:</strong> Heavy weapons specialist with explosive expertise</li>
        <li><strong>Medic:</strong> Healing and revival specialist with combat skills</li>
        <li><strong>Engineer:</strong> Construction and demolition expert</li>
        <li><strong>Field Ops:</strong> Support class with ammunition and airstrikes</li>
        <li><strong>Covert Ops:</strong> Stealth specialist with reconnaissance abilities</li>
    </ul>
    
    <h3>Skill System</h3>
    <p>Players gain experience (XP) in different skill categories:</p>
    <ul>
        <li><strong>Battle Sense:</strong> General combat awareness and health</li>
        <li><strong>Engineering:</strong> Construction and defusing speed</li>
        <li><strong>First Aid:</strong> Medical efficiency and self-healing</li>
        <li><strong>Signals:</strong> Communication and artillery effectiveness</li>
        <li><strong>Light Weapons:</strong> Accuracy with rifles and pistols</li>
        <li><strong>Heavy Weapons:</strong> Proficiency with machine guns and explosives</li>
        <li><strong>Covert Operations:</strong> Stealth and infiltration abilities</li>
    </ul>
</div>

<div class="section">
    <h2>ET Legacy Enhancements</h2>
    
    <h3>Graphics Improvements</h3>
    <div class="code-block">
        <pre><code># Enhanced graphics settings
seta r_ext_texture_filter_anisotropic "8"  // Anisotropic filtering
seta r_hdr "1"                             // HDR rendering
seta r_postprocess "1"                     // Post-processing effects
seta r_bloom "1"                           // Bloom lighting
seta r_drawSun "1"                         // Enhanced sun rendering
seta r_wolffog "1"                         // Volumetric fog
seta r_lightmap "1"                        // Enhanced lightmaps</code></pre>
    </div>
    
    <h3>Audio Enhancements</h3>
    <ul>
        <li><strong>OpenAL Support:</strong> 3D positional audio</li>
        <li><strong>OGG Vorbis:</strong> Compressed audio support</li>
        <li><strong>Surround Sound:</strong> Multi-channel audio</li>
        <li><strong>Voice Chat:</strong> Integrated voice communication</li>
    </ul>
    
    <h3>Network Improvements</h3>
    <ul>
        <li><strong>IPv6 Support:</strong> Modern networking protocol</li>
        <li><strong>Master Server:</strong> Dedicated server browser</li>
        <li><strong>Auto-Download:</strong> Automatic map and mod downloading</li>
        <li><strong>Demo Recording:</strong> Enhanced demo system</li>
    </ul>
</div>

<div class="section">
    <h2>Installation and Setup</h2>
    
    <h3>System Requirements</h3>
    <ul>
        <li><strong>OS:</strong> Windows 7+, Linux (modern distro), macOS 10.12+</li>
        <li><strong>CPU:</strong> Dual-core 2.0 GHz or higher</li>
        <li><strong>RAM:</strong> 2 GB minimum, 4 GB recommended</li>
        <li><strong>GPU:</strong> OpenGL 3.0 compatible or better</li>
        <li><strong>Storage:</strong> 2 GB free space</li>
    </ul>
    
    <h3>Quick Installation</h3>
    <ol>
        <li>Download ET Legacy from the official website</li>
        <li>Install original Wolfenstein: Enemy Territory (for assets)</li>
        <li>Extract ET Legacy to desired directory</li>
        <li>Copy ET assets (pak0.pk3, pak1.pk3, pak2.pk3) to etmain/</li>
        <li>Run etl.exe (Windows) or etl (Linux/macOS)</li>
    </ol>
    
    <div class="code-block">
        <pre><code># Linux installation example
wget https://www.etlegacy.com/download/etlegacy-linux-x86_64.tar.gz
tar xzf etlegacy-linux-x86_64.tar.gz
cd etlegacy/
# Copy ET assets to etmain/
./etl.x86_64</code></pre>
    </div>
</div>

<div class="section">
    <h2>Configuration</h2>
    
    <h3>Basic Settings</h3>
    <div class="code-block">
        <pre><code># Essential ET Legacy settings
seta name "YourPlayerName"                 // Player name
seta rate "25000"                          // Network rate
seta snaps "40"                            // Snapshot rate
seta com_maxfps "125"                      // Frame rate limit
seta m_pitch "0.022"                       // Mouse sensitivity
seta cg_fov "90"                           // Field of view
seta cg_drawgun "1"                        // Draw weapon model</code></pre>
    </div>
    
    <h3>Graphics Configuration</h3>
    <div class="code-block">
        <pre><code># Graphics optimization
seta r_mode "-1"                           // Custom resolution
seta r_customwidth "1920"                 // Screen width
seta r_customheight "1080"                // Screen height
seta r_fullscreen "1"                      // Fullscreen mode
seta r_colorbits "32"                      // Color depth
seta r_depthbits "24"                      // Depth buffer
seta r_stencilbits "8"                     // Stencil buffer</code></pre>
    </div>
    
    <h3>Audio Configuration</h3>
    <div class="code-block">
        <pre><code># Audio settings
seta s_volume "0.8"                        // Master volume
seta s_musicvolume "0.3"                   // Music volume
seta s_doppler "1"                         // Doppler effect
seta s_defaultsound "0"                    // Default audio system
seta s_backend "OpenAL"                    // Audio backend</code></pre>
    </div>
</div>

<div class="section">
    <h2>Game Modes</h2>
    
    <h3>Standard Modes</h3>
    <ul>
        <li><strong>Campaign:</strong> Linked objective maps played in sequence</li>
        <li><strong>Objective:</strong> Single map with specific goals</li>
        <li><strong>Stopwatch:</strong> Time-based competitive mode</li>
        <li><strong>Last Man Standing:</strong> Elimination-based gameplay</li>
    </ul>
    
    <h3>Popular Maps</h3>
    <ul>
        <li><strong>Goldrush:</strong> Classic tank escort mission</li>
        <li><strong>Oasis:</strong> Desert fortification assault</li>
        <li><strong>Battery:</strong> Coastal artillery objective</li>
        <li><strong>Railgun:</strong> Mountain facility infiltration</li>
        <li><strong>Fuel Dump:</strong> Depot destruction mission</li>
        <li><strong>Radar:</strong> Communications disruption</li>
    </ul>
</div>

<div class="section">
    <h2>Modding and Customization</h2>
    
    <h3>Mod Support</h3>
    <p>ET Legacy maintains compatibility with most ET mods:</p>
    <ul>
        <li><strong>ETpro:</strong> Competitive gameplay mod</li>
        <li><strong>ETPub:</strong> Public server features</li>
        <li><strong>Jaymod:</strong> Enhanced gameplay features</li>
        <li><strong>Silent:</strong> Anti-cheat and admin tools</li>
        <li><strong>NoCrash:</strong> Stability improvements</li>
    </ul>
    
    <h3>Custom Content</h3>
    <ul>
        <li><strong>Maps:</strong> Custom objective and campaign maps</li>
        <li><strong>Weapons:</strong> Weapon modifications and additions</li>
        <li><strong>Player Models:</strong> Custom character skins</li>
        <li><strong>HUD:</strong> Interface customization</li>
        <li><strong>Sounds:</strong> Audio replacement packs</li>
    </ul>
</div>

<div class="section">
    <h2>Community and Resources</h2>
    
    <h3>Official Resources</h3>
    <ul>
        <li><strong>Website:</strong> <a href="https://www.etlegacy.com">www.etlegacy.com</a></li>
        <li><strong>GitHub:</strong> Source code and issue tracking</li>
        <li><strong>Forums:</strong> Community discussion and support</li>
        <li><strong>Discord:</strong> Real-time chat and support</li>
    </ul>
    
    <h3>Community Sites</h3>
    <ul>
        <li><strong>Trackbase:</strong> Player statistics and rankings</li>
        <li><strong>Splatterladder:</strong> Server listings and stats</li>
        <li><strong>ETmaps:</strong> Map database and downloads</li>
        <li><strong>Crossfire:</strong> ET community hub</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Common Issues</h3>
    <div class="troubleshooting">
        <h4>Game won't start</h4>
        <ul>
            <li>Verify ET assets are in etmain/ directory</li>
            <li>Check graphics driver compatibility</li>
            <li>Run with administrative privileges (Windows)</li>
        </ul>
        
        <h4>Poor performance</h4>
        <ul>
            <li>Lower graphics settings in options menu</li>
            <li>Disable unnecessary background applications</li>
            <li>Update graphics drivers</li>
        </ul>
        
        <h4>Network connectivity issues</h4>
        <ul>
            <li>Check firewall and router settings</li>
            <li>Verify internet connection stability</li>
            <li>Try different servers from server browser</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="et-legacy/classes">Player Classes</a></li>
        <li><a href="et-legacy/objectives">Objective Gameplay</a></li>
        <li><a href="et-legacy/weapons">Weapon Systems</a></li>
        <li><a href="et-legacy/maps">Maps and Campaigns</a></li>
        <li><a href="server/setup">Server Setup</a></li>
    </ul>
</div> 