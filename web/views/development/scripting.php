<?php 
$title = "Scripting Guide - id Tech 3 Documentation";
$description = "Complete guide to scripting in Quake III Arena including console commands, cfg files, and automation";
$breadcrumbs = [
    '/development' => 'Development',
    '/development/scripting' => 'Scripting'
];
?>

<div class="content-section">
    <h1>Quake III Scripting Guide</h1>
    
    <blockquote>
        <strong>Automation Power:</strong> Quake III's flexible console system allows for powerful scripting capabilities that can automate tasks, create complex bindings, and enhance gameplay.
    </blockquote>

    <h2>Console System Fundamentals</h2>
    
    <h3>Understanding CVars</h3>
    <p><strong>Console Variables (CVars)</strong> are the foundation of Q3 scripting:</p>
    
    <table>
        <thead>
            <tr>
                <th>Type</th>
                <th>Description</th>
                <th>Example</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Read-Only</strong></td>
                <td>Cannot be changed by user</td>
                <td><code>version</code>, <code>protocol</code></td>
            </tr>
            <tr>
                <td><strong>User</strong></td>
                <td>Saved in config files</td>
                <td><code>name</code>, <code>sensitivity</code></td>
            </tr>
            <tr>
                <td><strong>Server</strong></td>
                <td>Controlled by server</td>
                <td><code>fraglimit</code>, <code>timelimit</code></td>
            </tr>
            <tr>
                <td><strong>System</strong></td>
                <td>Engine/renderer settings</td>
                <td><code>r_mode</code>, <code>com_maxfps</code></td>
            </tr>
        </tbody>
    </table>

    <h3>Basic Console Commands</h3>
    <div class="example">
        <pre>// Variable manipulation
set myvar "value"           // Set a variable
seta myvar "value"          // Set and archive (save to config)
toggle myvar                // Toggle between 0 and 1
toggle myvar 0 1 2          // Cycle through values
inc myvar 5                 // Increment by 5
reset myvar                 // Reset to default value

// Information commands
cvarlist                    // List all variables
cvarlist r_*               // List variables starting with r_
cmdlist                    // List all commands
echo "Hello World"         // Print message to console</pre>
    </div>

    <h2>Configuration Files</h2>
    
    <h3>Config File Types</h3>
    <ul>
        <li><span class="keyword">q3config.cfg</span> - Main configuration file</li>
        <li><span class="keyword">autoexec.cfg</span> - Auto-executed on startup</li>
        <li><span class="keyword">custom.cfg</span> - User-defined configurations</li>
        <li><span class="keyword">server.cfg</span> - Server-specific settings</li>
    </ul>

    <h3>Config File Best Practices</h3>
    <div class="example">
        <pre>// autoexec.cfg example
// This file is executed automatically on startup

// Graphics settings
seta r_mode "6"
seta r_fullscreen "1"
seta r_gamma "1.3"

// Network settings  
seta rate "50000"
seta snaps "40"
seta cl_maxpackets "125"

// Player settings
seta name "YourName"
seta model "doom"
seta color1 "4"
seta color2 "5"

// Execute additional configs
exec binds.cfg
exec scripts.cfg

echo "^2Autoexec.cfg loaded successfully!"</pre>
    </div>

    <h2>Advanced Scripting Techniques</h2>
    
    <h3>Conditional Logic</h3>
    <p>Q3 doesn't have native if/then statements, but you can simulate them:</p>
    
    <div class="example">
        <pre>// Conditional execution using vstr
set option1 "echo Option 1 selected; set nextoption vstr option2"
set option2 "echo Option 2 selected; set nextoption vstr option1" 
set nextoption "vstr option1"

// Usage: nextoption will toggle between option1 and option2
bind F1 "vstr nextoption"

// State-based weapon switching
set weapon_state1 "weapon 3; set weapon_next vstr weapon_state2"
set weapon_state2 "weapon 6; set weapon_next vstr weapon_state3"  
set weapon_state3 "weapon 7; set weapon_next vstr weapon_state1"
set weapon_next "vstr weapon_state1"

bind q "vstr weapon_next"</pre>
    </div>

    <h3>Complex Binding Systems</h3>
    
    <h4>Multi-Function Keys</h4>
    <div class="example">
        <pre>// Movement script with multiple functions
set +moveforward "+forward; echo Moving forward"
set -moveforward "-forward; echo Stopped moving"

// Zoom toggle with different sensitivities
set zoom_in "cg_fov 90; sensitivity 1.0; echo ^2Zoomed In"
set zoom_out "cg_fov 120; sensitivity 3.0; echo ^1Zoomed Out"
set zoom_toggle "vstr zoom_in"

// Toggle between zoom states
set zoom_in "cg_fov 90; sensitivity 1.0; set zoom_toggle vstr zoom_out"
set zoom_out "cg_fov 120; sensitivity 3.0; set zoom_toggle vstr zoom_in"

bind mouse2 "vstr zoom_toggle"</pre>
    </div>

    <h3>Weapon Scripts</h3>
    <div class="example">
        <pre>// Weapon-specific settings
set rail_settings "sensitivity 1.5; cg_fov 90; echo ^5Railgun selected"
set rocket_settings "sensitivity 3.0; cg_fov 110; echo ^1Rocket Launcher selected"

// Auto-execute settings when switching weapons
bind 1 "weapon 1; vstr gauntlet_settings"
bind 2 "weapon 2; vstr machinegun_settings"  
bind 3 "weapon 3; vstr shotgun_settings"
bind 4 "weapon 4; vstr grenade_settings"
bind 5 "weapon 5; vstr rocket_settings"
bind 6 "weapon 6; vstr lightning_settings"
bind 7 "weapon 7; vstr rail_settings"
bind 8 "weapon 8; vstr plasma_settings"
bind 9 "weapon 9; vstr bfg_settings"</pre>
    </div>

    <h2>Server Administration Scripts</h2>
    
    <h3>Basic Server Management</h3>
    <div class="example">
        <pre>// server.cfg - Server configuration
seta sv_hostname "My Q3 Server"
seta sv_maxclients "16"
seta g_motd "Welcome to my server!"
seta g_forcerespawn "5"
seta g_inactivity "300"

// Map rotation
set map1 "map q3dm1; set nextmap vstr map2"
set map2 "map q3dm4; set nextmap vstr map3"
set map3 "map q3dm6; set nextmap vstr map1"
set nextmap "vstr map1"

// Admin commands
set admin_password "your_password_here"
set rcon_password "rcon_password_here"

// Start the rotation
vstr map1</pre>
    </div>

    <h3>Advanced Server Scripts</h3>
    <div class="example">
        <pre>// Automated tournament mode
set tourney_start "g_gametype 1; fraglimit 10; timelimit 10; echo ^2Tournament Started"
set tourney_end "g_gametype 0; fraglimit 20; timelimit 15; echo ^1Tournament Ended"

// Player management
set kick_inactive "kick_inactive_players; echo Kicked inactive players"
set balance_teams "auto_balance_teams; echo Teams balanced"

// Map-specific configurations
set dm1_config "fraglimit 30; timelimit 15; g_weaponrespawn 5"
set dm4_config "fraglimit 25; timelimit 20; g_weaponrespawn 3"
set dm6_config "fraglimit 35; timelimit 12; g_weaponrespawn 7"</pre>
    </div>

    <h2>Client-Side Automation</h2>
    
    <h3>HUD and Display Scripts</h3>
    <div class="example">
        <pre>// Custom HUD configurations
set hud_minimal "cg_draw2d 1; cg_drawgun 0; cg_drawfps 1; cg_lagometer 1"
set hud_full "cg_draw2d 1; cg_drawgun 1; cg_drawfps 1; cg_drawcrosshair 1"
set hud_clean "cg_draw2d 0; cg_drawgun 0; cg_drawfps 0"

// Screenshots and recording
set demo_start "record demo; echo ^2Recording started"
set demo_stop "stoprecord; echo ^1Recording stopped"
set screenshot_hq "r_gamma 1.0; wait; screenshot; wait; r_gamma 1.3"

bind F9 "vstr demo_start"
bind F10 "vstr demo_stop"  
bind F12 "vstr screenshot_hq"</pre>
    </div>

    <h3>Communication Scripts</h3>
    <div class="example">
        <pre>// Quick chat bindings
set chat1 "say_team Need backup!"
set chat2 "say_team Enemy flag taken!"
set chat3 "say_team I have the flag!"
set chat4 "say_team Base is secure!"

// Voice command shortcuts
set voice1 "vsay_team affirmative"
set voice2 "vsay_team negative" 
set voice3 "vsay_team followme"
set voice4 "vsay_team getflag"

// Bind to number pad
bind kp_end "vstr chat1"
bind kp_downarrow "vstr chat2"
bind kp_pgdn "vstr chat3"
bind kp_leftarrow "vstr chat4"</pre>
    </div>

    <h2>Performance and Debugging Scripts</h2>
    
    <h3>Performance Monitoring</h3>
    <div class="example">
        <pre>// Performance test configurations
set fps_test "timedemo 1; demo four; wait 1000; quit"
set net_test "ping; rate; snaps; cl_maxpackets"

// Debug information toggle
set debug_on "developer 1; com_speeds 1; r_showfps 1; cg_lagometer 1"
set debug_off "developer 0; com_speeds 0; r_showfps 0; cg_lagometer 0"
set debug_toggle "vstr debug_on"

// Performance configs
set config_maxfps "com_maxfps 125; r_displayrefresh 120"
set config_quality "r_picmip 0; r_texturemode GL_LINEAR_MIPMAP_LINEAR"
set config_performance "r_picmip 2; r_texturemode GL_LINEAR_MIPMAP_NEAREST"</pre>
    </div>

    <h2>Modding and Development Scripts</h2>
    
    <h3>Developer Utilities</h3>
    <div class="example">
        <pre>// Map testing shortcuts
set devmap_test "devmap mymap; god; noclip; give all"
set bot_add "addbot anarki 3; addbot doom 3"
set bot_remove "kick anarki; kick doom"

// Weapon testing
set test_weapons "give weapons; give ammo; god"
set test_items "give all"
set test_location "where; getpos"

// Quick restart and reload
set quick_restart "map_restart 0"
set reload_map "devmap $mapname"

bind F5 "vstr quick_restart"
bind F6 "vstr reload_map"</pre>
    </div>

    <h3>Asset Development</h3>
    <div class="example">
        <pre>// Shader development
set shader_reload "r_shownormals 1; vid_restart; wait; r_shownormals 0"
set texture_info "r_showtris 1; r_showsky 1"

// Model testing
set model_test "model doom; playermodel doom"
set skin_cycle "team_model doom; team_headmodel doom"

// Sound testing  
set sound_test "s_volume 1; s_musicvolume 0.5"
set sound_info "s_info; soundlist"</pre>
    </div>

    <h2>Utility Scripts and Functions</h2>
    
    <h3>System Management</h3>
    <div class="example">
        <pre>// Config management
set save_config "writeconfig myconfig.cfg; echo Config saved"
set load_config "exec myconfig.cfg; echo Config loaded"
set reset_config "cvar_restart; echo Settings reset"

// File operations
set demo_cleanup "condump demos.txt; echo Demo list saved"
set screenshot_name "screenshot MyScreenshot; echo Screenshot saved"

// System information
set sys_info "systeminfo; r_info; net_info"
set hardware_info "gfxinfo; soundinfo"</pre>
    </div>

    <h2>Script Organization and Management</h2>
    
    <h3>Modular Configuration</h3>
    <div class="example">
        <pre>// File structure:
// autoexec.cfg - Main startup file
// binds.cfg - All key bindings
// graphics.cfg - Graphics settings
// network.cfg - Network configuration
// scripts.cfg - Custom scripts

// autoexec.cfg content:
exec graphics.cfg
exec network.cfg  
exec binds.cfg
exec scripts.cfg
echo "^2All configurations loaded!"

// Use consistent naming
// Prefix scripts by category: gfx_, net_, bind_, etc.</pre>
    </div>

    <h3>Version Control for Configs</h3>
    <div class="example">
        <pre>// Config versioning
set config_version "v2.1"
set config_date "2024-01-15"
echo "^3Config version: $config_version ($config_date)"

// Backup and restore
set backup_config "writeconfig backup_config.cfg"
set restore_config "exec backup_config.cfg"

// Environment detection
set home_config "exec home.cfg; echo Home config loaded"
set work_config "exec work.cfg; echo Work config loaded"</pre>
    </div>

    <h2>Common Scripting Patterns</h2>
    
    <h3>Toggle Scripts</h3>
    <table>
        <thead>
            <tr>
                <th>Pattern</th>
                <th>Use Case</th>
                <th>Example</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Binary Toggle</strong></td>
                <td>On/Off states</td>
                <td><code>toggle cg_drawfps</code></td>
            </tr>
            <tr>
                <td><strong>Multi-State Toggle</strong></td>
                <td>Cycle through options</td>
                <td><code>toggle r_picmip 0 1 2 3</code></td>
            </tr>
            <tr>
                <td><strong>VStr Toggle</strong></td>
                <td>Complex state changes</td>
                <td>Weapon configurations</td>
            </tr>
            <tr>
                <td><strong>Conditional Toggle</strong></td>
                <td>State-dependent actions</td>
                <td>Context-sensitive binds</td>
            </tr>
        </tbody>
    </table>

    <h2>Troubleshooting Scripts</h2>
    
    <h3>Common Issues</h3>
    <ul>
        <li><strong>Quote Marks:</strong> Use proper quoting for complex commands</li>
        <li><strong>Wait Commands:</strong> Some actions need delays between commands</li>
        <li><strong>Variable Scope:</strong> Some cvars are server-only or client-only</li>
        <li><strong>Command Length:</strong> Very long command strings may be truncated</li>
    </ul>

    <h3>Debugging Techniques</h3>
    <div class="example">
        <pre>// Debug output
set debug_script "echo Script executed at: $(date)"
set trace_var "echo Variable value: $myvar"

// Step-by-step execution
set step1 "echo Step 1; set nextstep vstr step2"
set step2 "echo Step 2; set nextstep vstr step3"
set step3 "echo Step 3; set nextstep vstr step1"

// Error handling
set safe_exec "exec userconfig.cfg || echo Config not found"</pre>
    </div>

    <blockquote>
        <strong>Best Practice:</strong> Always test scripts thoroughly before deployment, especially on servers. A buggy script can crash the game or make it unplayable.
    </blockquote>

    <h2>Security Considerations</h2>
    
    <h3>Server Security</h3>
    <ul>
        <li><strong>RCON Protection:</strong> Use strong passwords for remote console</li>
        <li><strong>Client Validation:</strong> Validate user input in server scripts</li>
        <li><strong>File Access:</strong> Restrict access to sensitive configuration files</li>
        <li><strong>Command Filtering:</strong> Filter dangerous commands from user input</li>
    </ul>

    <h2>Resources and Tools</h2>
    
    <h3>Script Development Tools</h3>
    <ul>
        <li><strong>Text Editors:</strong> VS Code, Notepad++ with syntax highlighting</li>
        <li><strong>Config Managers:</strong> Tools for organizing multiple configurations</li>
        <li><strong>Testing:</strong> Use local servers for testing complex scripts</li>
        <li><strong>Documentation:</strong> Keep detailed comments in your scripts</li>
    </ul>

    <h2>Quick Reference</h2>
    
    <h3>Essential Commands</h3>
    <div class="example">
        <pre>// Most commonly used scripting commands:
set var "value"        // Set variable
seta var "value"       // Set and archive
vstr var               // Execute variable as command
toggle var [values]    // Toggle between values
bind key "command"     // Bind key to command
exec filename          // Execute script file
echo "message"         // Print to console
wait [frames]          // Delay execution
cvar_restart          // Reset all variables
writeconfig file       // Save configuration</pre>
    </div>

    <blockquote>
        <strong>Remember:</strong> Start simple and build complexity gradually. Document your scripts well and always keep backups of working configurations.
    </blockquote>
</div> 