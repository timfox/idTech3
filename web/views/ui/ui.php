<?php
/**
 * UI System Documentation
 */
$title = 'UI System - id Tech 3 Documentation';
$breadcrumbs = [
    '/ui' => 'UI',
    '/ui/ui' => 'UI System'
];
?>

<h1>User Interface System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 UI system provides menu interfaces, HUD elements, and user interaction. Quake3e extends this with modern UI frameworks including Dear ImGui integration.</p>
    
    <div class="feature-list">
        <h3>UI Components</h3>
        <ul>
            <li><strong>Menu System:</strong> Game menus and configuration</li>
            <li><strong>HUD:</strong> Heads-up display elements</li>
            <li><strong>Console:</strong> Developer and user console</li>
            <li><strong>ImGui:</strong> Modern immediate-mode GUI (Quake3e)</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Menu System</h2>
    
    <h3>Menu Framework</h3>
    <p>Classic id Tech 3 menu system based on menuframework_s:</p>
    <div class="code-block">
        <pre><code>typedef struct {
    int cursor;              // Current menu item
    int cursor_prev;         // Previous cursor position
    int nitems;             // Number of menu items
    void *items[MAX_MENUITEMS];  // Menu item array
    void (*draw)(void);     // Custom draw function
    sfxHandle_t (*key)(int key);  // Key handler
    qboolean wrapAround;    // Cursor wrapping
    qboolean fullscreen;    // Fullscreen menu
    qboolean showlogo;      // Show game logo
} menuframework_s;</code></pre>
    </div>
    
    <h3>Menu Item Types</h3>
    <ul>
        <li><strong>MTYPE_NULL:</strong> Empty space</li>
        <li><strong>MTYPE_SLIDER:</strong> Value slider</li>
        <li><strong>MTYPE_LIST:</strong> Selection list</li>
        <li><strong>MTYPE_ACTION:</strong> Button action</li>
        <li><strong>MTYPE_SPINCONTROL:</strong> Spin control</li>
        <li><strong>MTYPE_FIELD:</strong> Text input field</li>
        <li><strong>MTYPE_RADIOBUTTON:</strong> Radio button</li>
        <li><strong>MTYPE_BITMAP:</strong> Image button</li>
        <li><strong>MTYPE_TEXT:</strong> Static text</li>
    </ul>
</div>

<div class="section">
    <h2>HUD System</h2>
    
    <h3>HUD Elements</h3>
    <div class="code-block">
        <pre><code>// HUD drawing function
void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback) {
    // Draw 3D world
    CG_Draw3DView();
    
    // Draw 2D HUD elements
    if (!cg.renderingThirdPerson) {
        CG_DrawStatusBar();     // Health, armor, ammo
        CG_DrawAmmoWarning();   // Low ammo warning
        CG_DrawWeaponSelect();  // Weapon selection
        CG_DrawCrosshair();     // Crosshair
        CG_DrawPickupItem();    // Item pickup notifications
        CG_DrawReward();        // Score rewards
        CG_DrawScoreboard();    // Scoreboard overlay
        CG_DrawUpperRight();    // Timer, score info
        CG_DrawSnapshot();      // Network info
        CG_DrawFPS();           // FPS counter
        CG_DrawTimer();         // Match timer
        CG_DrawTeamInfo();      // Team status
    }
}</code></pre>
    </div>
    
    <h3>HUD Customization</h3>
    <div class="code-block">
        <pre><code># HUD configuration
seta cg_draw2D "1"             // Enable 2D HUD
seta cg_drawGun "1"            // Draw weapon model
seta cg_drawCrosshair "1"      // Enable crosshair
seta cg_drawCrosshairNames "1" // Show player names
seta cg_drawRewards "1"        // Show score rewards
seta cg_drawStatus "1"         // Show status bar
seta cg_drawTimer "1"          // Show match timer
seta cg_drawFPS "1"            // Show FPS counter
seta cg_drawSnapshot "1"       // Show network info
seta cg_hudFiles "ui/hud.txt"  // Custom HUD definition</code></pre>
    </div>
</div>

<div class="section">
    <h2>Dear ImGui Integration (Quake3e)</h2>
    
    <h3>Modern UI Framework</h3>
    <p>Quake3e includes Dear ImGui for developer tools and modern UI:</p>
    <div class="code-block">
        <pre><code># Enable ImGui in Quake3e
seta r_imgui "1"               // Enable ImGui
seta r_imguiDemo "0"           // Show ImGui demo window
seta r_imguiConsole "1"        // ImGui console window
seta r_imguiStats "1"          // Performance statistics
seta r_imguiDebug "0"          // Debug information</code></pre>
    </div>
    
    <h3>ImGui Features</h3>
    <ul>
        <li><strong>Developer Console:</strong> Enhanced console interface</li>
        <li><strong>Performance Stats:</strong> Real-time performance monitoring</li>
        <li><strong>Renderer Debug:</strong> Graphics debugging tools</li>
        <li><strong>Memory Usage:</strong> Memory profiling</li>
        <li><strong>CVars Editor:</strong> Runtime variable editing</li>
    </ul>
</div>

<div class="section">
    <h2>Console System</h2>
    
    <h3>Console Features</h3>
    <ul>
        <li><strong>Command Execution:</strong> Execute game commands</li>
        <li><strong>CVar Management:</strong> Variable manipulation</li>
        <li><strong>Auto-completion:</strong> Tab completion</li>
        <li><strong>Command History:</strong> Previous command recall</li>
        <li><strong>Scripting:</strong> Execute script files</li>
    </ul>
    
    <div class="code-block">
        <pre><code># Console configuration
seta con_conspeed "3"         // Console slide speed
seta con_notifytime "3"       // Notification display time
seta con_opacity "1.0"        // Console background opacity
seta com_maxfps "125"         // Frame rate limit
seta developer "1"            // Developer mode</code></pre>
    </div>
</div>

<div class="section">
    <h2>UI Scripting</h2>
    
    <h3>Menu Scripts</h3>
    <p>UI behavior can be scripted using the menu system:</p>
    <div class="code-block">
        <pre><code>// Custom menu item
{
    name            "myButton"
    group           "myGroup"
    type            ITEM_TYPE_BUTTON
    text            "Custom Button"
    textfont        UI_FONT_NORMAL
    textscale       0.4
    rect            100 200 120 20
    textalign       ITEM_ALIGN_CENTER
    textalignx      60
    textaligny      16
    forecolor       1 1 1 1
    backcolor       0 0 0 0.5
    bordercolor     0.5 0.5 0.5 1
    visible         1
    action {
        play "sound/misc/menu1.wav";
        open myOtherMenu;
    }
}</code></pre>
    </div>
    
    <h3>HUD Scripts</h3>
    <p>Custom HUD elements can be defined in HUD script files.</p>
</div>

<div class="section">
    <h2>Custom UI Development</h2>
    
    <h3>Creating Custom Menus</h3>
    <ol>
        <li>Define menu structure in .menu files</li>
        <li>Create menu art assets</li>
        <li>Implement menu logic in C code</li>
        <li>Add menu callbacks and event handling</li>
        <li>Test menu navigation and functionality</li>
    </ol>
    
    <h3>UI Asset Creation</h3>
    <ul>
        <li><strong>Menu Backgrounds:</strong> TGA/JPG images</li>
        <li><strong>Button States:</strong> Normal, highlighted, pressed</li>
        <li><strong>Fonts:</strong> TrueType font support</li>
        <li><strong>Icons:</strong> UI element graphics</li>
    </ul>
</div>

<div class="section">
    <h2>Accessibility Features</h2>
    
    <h3>UI Accessibility</h3>
    <ul>
        <li><strong>Font Scaling:</strong> Adjustable text size</li>
        <li><strong>Color Options:</strong> Colorblind-friendly palettes</li>
        <li><strong>High Contrast:</strong> Enhanced visibility modes</li>
        <li><strong>Key Remapping:</strong> Customizable controls</li>
    </ul>
    
    <div class="code-block">
        <pre><code># Accessibility settings
seta cg_hudScale "1.2"         // Scale HUD elements
seta cg_fontScale "1.1"        // Scale font size
seta r_textureMode "GL_LINEAR" // Font smoothing
seta cg_colorBlind "0"         // Colorblind mode</code></pre>
    </div>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Common UI Issues</h3>
    <div class="troubleshooting">
        <h4>Menu not displaying correctly</h4>
        <ul>
            <li>Check menu file syntax</li>
            <li>Verify asset paths are correct</li>
            <li>Test with default UI files</li>
        </ul>
        
        <h4>HUD elements missing</h4>
        <ul>
            <li>Check cg_draw2D setting</li>
            <li>Verify HUD configuration</li>
            <li>Reset HUD settings to default</li>
        </ul>
        
        <h4>Font rendering issues</h4>
        <ul>
            <li>Check font file integrity</li>
            <li>Verify font scale settings</li>
            <li>Test with different texture modes</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="ui/imgui-integration">Dear ImGui Integration</a></li>
        <li><a href="ui/hud-customization">HUD Customization</a></li>
        <li><a href="ui/menu-scripting">Menu Scripting</a></li>
        <li><a href="development/modding">UI Modding</a></li>
    </ul>
</div> 