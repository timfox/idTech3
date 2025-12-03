<?php 
$title = "Custom Cursor Guide - id Tech 3 Documentation";
$description = "Complete guide to customizing the cursor appearance in Quake III Arena mods";
$breadcrumbs = [
    '/development' => 'Development',
    '/custom_cursor' => 'Custom Cursor'
];
?>

<div class="content-section">
    <h1>Custom Cursor Guide</h1>
    
    <blockquote>
        <strong>Customize Your Cursor:</strong> Learn how to change the cursor appearance in your Quake III Arena mod or game. This guide covers both simple asset replacement and advanced cursor customization through code.
    </blockquote>

    <div class="section">
        <h2>Overview</h2>
        <p>The cursor in Quake III Arena is rendered by the UI module and can be customized in several ways. The cursor system handles menu navigation and provides visual feedback for user interaction.</p>
        
        <div class="feature-list">
            <h3>Cursor Customization Methods</h3>
            <ul>
                <li><strong>Asset Replacement:</strong> Replace the cursor texture file</li>
                <li><strong>Shader Modification:</strong> Use shader effects on the cursor</li>
                <li><strong>Code Modification:</strong> Change cursor behavior and appearance programmatically</li>
                <li><strong>Animated Cursor:</strong> Create animated cursor effects</li>
            </ul>
        </div>
    </div>

    <div class="section">
        <h2>Method 1: Simple Asset Replacement</h2>
        
        <h3>Step 1: Prepare Your Cursor Image</h3>
        <p>The cursor image should be:</p>
        <ul>
            <li><strong>Format:</strong> TGA (Targa) with alpha channel for transparency</li>
            <li><strong>Size:</strong> Recommended 32x32 pixels (can be larger, but will be scaled)</li>
            <li><strong>Color Depth:</strong> 32-bit RGBA (Red, Green, Blue, Alpha)</li>
            <li><strong>Hotspot:</strong> Center point (16,16) for 32x32 cursor</li>
        </ul>

        <h3>Step 2: Create or Replace the Cursor File</h3>
        <p>Place your cursor image in one of these locations:</p>
        <div class="code-block">
            <pre><code>mymod/
├── menu/
│   └── art/
│       └── 3_cursor2.tga    # Default cursor location
└── gfx/
    └── ui/
        └── cursor.tga       # Alternative location</code></pre>
        </div>

        <h3>Step 3: Update the Shader</h3>
        <p>If using a custom cursor file, ensure it has a shader definition. Add to <code>mymod/shaders/iconsprites.shader</code>:</p>
        <div class="code-block">
            <pre><code>gfx/2d/cursor
{
    nopicmip
    nomipmaps
    {
        map gfx/ui/cursor.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    }
}</code></pre>
        </div>

        <h3>Step 4: Register the Cursor in Code</h3>
        <p>In your UI initialization code (<code>ui_qmenu.c</code> or similar), register the cursor shader:</p>
        <div class="code-block">
            <pre><code>// In UI_Init() function
uis.cursor = trap_R_RegisterShaderNoMip("menu/art/3_cursor2");
// Or for custom location:
uis.cursor = trap_R_RegisterShaderNoMip("gfx/ui/cursor");</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Method 2: Code-Based Cursor Customization</h2>
        
        <h3>Understanding the Cursor System</h3>
        <p>The cursor is drawn in <code>UI_Refresh()</code> function in <code>ui_atoms.c</code>:</p>
        <div class="code-block">
            <pre><code>void UI_Refresh(int realtime) {
    // ... menu drawing code ...
    
    // Draw cursor (drawn last so it appears on top)
    UI_SetColor(NULL);
    UI_DrawHandlePic(uis.cursorx - 16, uis.cursory - 16, 32, 32, uis.cursor);
}</code></pre>
        </div>

        <h3>Cursor Position Variables</h3>
        <p>The cursor position is tracked in the <code>uis</code> structure:</p>
        <div class="code-block">
            <pre><code>typedef struct {
    int cursorx;        // Current X position
    int cursory;        // Current Y position
    qhandle_t cursor;   // Cursor shader handle
    // ... other UI state ...
} uiInfo_t;</code></pre>
        </div>

        <h3>Custom Cursor Drawing Function</h3>
        <p>Create a custom cursor drawing function for advanced effects:</p>
        <div class="code-block">
            <pre><code>void UI_DrawCustomCursor(void) {
    float scale;
    int x, y;
    vec4_t color;
    
    // Get cursor position
    x = uis.cursorx;
    y = uis.cursory;
    
    // Apply UI scaling if needed
    scale = uis.scale;
    if (scale != 1.0f) {
        x = (int)(x * scale + uis.bias);
        y = (int)(y * scale);
    }
    
    // Set cursor color (white = no tinting)
    color[0] = color[1] = color[2] = color[3] = 1.0f;
    UI_SetColor(color);
    
    // Draw cursor centered on position
    // Parameters: x, y, width, height, shader
    UI_DrawHandlePic(x - 16, y - 16, 32, 32, uis.cursor);
    
    // Reset color
    UI_SetColor(NULL);
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Method 3: Animated Cursor</h2>
        
        <h3>Creating an Animated Cursor</h3>
        <p>To create an animated cursor, you can cycle through multiple cursor frames:</p>
        <div class="code-block">
            <pre><code>// In ui_local.h or ui_main.c
static int cursorFrame = 0;
static int cursorFrameTime = 0;
#define CURSOR_FRAME_COUNT 4
#define CURSOR_FRAME_DELAY 100  // milliseconds per frame

qhandle_t cursorFrames[CURSOR_FRAME_COUNT];

// In UI_Init()
cursorFrames[0] = trap_R_RegisterShaderNoMip("menu/art/cursor_frame1");
cursorFrames[1] = trap_R_RegisterShaderNoMip("menu/art/cursor_frame2");
cursorFrames[2] = trap_R_RegisterShaderNoMip("menu/art/cursor_frame3");
cursorFrames[3] = trap_R_RegisterShaderNoMip("menu/art/cursor_frame4");

// In UI_Refresh()
void UI_DrawAnimatedCursor(void) {
    int currentTime = uis.realtime;
    
    // Advance frame if enough time has passed
    if (currentTime - cursorFrameTime > CURSOR_FRAME_DELAY) {
        cursorFrame = (cursorFrame + 1) % CURSOR_FRAME_COUNT;
        cursorFrameTime = currentTime;
    }
    
    // Draw current frame
    UI_SetColor(NULL);
    UI_DrawHandlePic(uis.cursorx - 16, uis.cursory - 16, 
                     32, 32, cursorFrames[cursorFrame]);
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Method 4: Cursor States (Normal, Hover, Click)</h2>
        
        <h3>Implementing Cursor States</h3>
        <p>Change cursor appearance based on interaction state:</p>
        <div class="code-block">
            <pre><code>typedef enum {
    CURSOR_NORMAL,
    CURSOR_HOVER,
    CURSOR_CLICK
} cursorState_t;

static cursorState_t currentCursorState = CURSOR_NORMAL;
qhandle_t cursorNormal, cursorHover, cursorClick;

// In UI_Init()
cursorNormal = trap_R_RegisterShaderNoMip("menu/art/cursor_normal");
cursorHover = trap_R_RegisterShaderNoMip("menu/art/cursor_hover");
cursorClick = trap_R_RegisterShaderNoMip("menu/art/cursor_click");

// In UI_Refresh() or menu item handlers
void UI_UpdateCursorState(menuDef_t *menu) {
    menuDef_t *item;
    
    // Check if cursor is over a clickable item
    item = Menu_ItemAtCursor(menu);
    if (item && (item->type == MTYPE_ACTION || 
                 item->type == MTYPE_BITMAP)) {
        currentCursorState = CURSOR_HOVER;
    } else {
        currentCursorState = CURSOR_NORMAL;
    }
}

void UI_DrawStatefulCursor(void) {
    qhandle_t cursorShader;
    
    switch (currentCursorState) {
        case CURSOR_HOVER:
            cursorShader = cursorHover;
            break;
        case CURSOR_CLICK:
            cursorShader = cursorClick;
            break;
        default:
            cursorShader = cursorNormal;
            break;
    }
    
    UI_SetColor(NULL);
    UI_DrawHandlePic(uis.cursorx - 16, uis.cursory - 16, 
                     32, 32, cursorShader);
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Advanced Techniques</h2>
        
        <h3>Cursor Scaling</h3>
        <p>Scale the cursor based on UI scale or resolution:</p>
        <div class="code-block">
            <pre><code>void UI_DrawScaledCursor(void) {
    float scale;
    int size;
    
    // Calculate scale based on resolution
    scale = uis.scale;
    size = (int)(32 * scale);
    
    UI_SetColor(NULL);
    UI_DrawHandlePic(uis.cursorx - (size / 2), 
                     uis.cursory - (size / 2), 
                     size, size, uis.cursor);
}</code></pre>
        </div>

        <h3>Cursor Rotation</h3>
        <p>Rotate the cursor for special effects (requires renderer support):</p>
        <div class="code-block">
            <pre><code>void UI_DrawRotatedCursor(float angle) {
    // Note: This requires renderer support for rotation
    // Most renderers don't support arbitrary rotation in UI
    // Consider using pre-rotated cursor frames instead
    
    // Alternative: Use shader rotation if supported
    // See shader documentation for rotation effects
}</code></pre>
        </div>

        <h3>Cursor Trail Effect</h3>
        <p>Create a cursor trail by drawing multiple cursor instances:</p>
        <div class="code-block">
            <pre><code>typedef struct {
    int x, y;
    int time;
    float alpha;
} cursorTrail_t;

#define MAX_TRAIL_POINTS 5
static cursorTrail_t trail[MAX_TRAIL_POINTS];
static int trailIndex = 0;

void UI_UpdateCursorTrail(void) {
    int i;
    float alpha;
    
    // Add current position to trail
    trail[trailIndex].x = uis.cursorx;
    trail[trailIndex].y = uis.cursory;
    trail[trailIndex].time = uis.realtime;
    trail[trailIndex].alpha = 1.0f;
    
    trailIndex = (trailIndex + 1) % MAX_TRAIL_POINTS;
    
    // Draw trail (oldest first)
    for (i = 0; i < MAX_TRAIL_POINTS; i++) {
        int idx = (trailIndex + i) % MAX_TRAIL_POINTS;
        int age = uis.realtime - trail[idx].time;
        
        if (age > 0 && age < 200) {  // 200ms trail
            alpha = 1.0f - (age / 200.0f);
            vec4_t color = {1.0f, 1.0f, 1.0f, alpha};
            
            UI_SetColor(color);
            UI_DrawHandlePic(trail[idx].x - 16, 
                           trail[idx].y - 16, 
                           32, 32, uis.cursor);
        }
    }
    
    // Draw main cursor on top
    UI_SetColor(NULL);
    UI_DrawHandlePic(uis.cursorx - 16, uis.cursory - 16, 32, 32, uis.cursor);
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Shader-Based Cursor Effects</h2>
        
        <h3>Glowing Cursor</h3>
        <p>Create a glowing effect using shader blending:</p>
        <div class="code-block">
            <pre><code>gfx/2d/cursor_glow
{
    nopicmip
    nomipmaps
    {
        // Glow layer
        map gfx/ui/cursor.tga
        blendFunc GL_SRC_ALPHA GL_ONE
        rgbGen wave sin 0.5 0.3 0 1
    }
    {
        // Main cursor
        map gfx/ui/cursor.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    }
}</code></pre>
        </div>

        <h3>Pulsing Cursor</h3>
        <p>Make the cursor pulse using shader animation:</p>
        <div class="code-block">
            <pre><code>gfx/2d/cursor_pulse
{
    nopicmip
    nomipmaps
    {
        map gfx/ui/cursor.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen wave sin 0.5 0.2 0 1
        alphaGen wave sin 0.5 0.3 0.5 1
    }
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Best Practices</h2>
        
        <h3>Performance Considerations</h3>
        <ul>
            <li><strong>Use NoMip:</strong> Always use <code>trap_R_RegisterShaderNoMip()</code> for cursors to avoid mipmap generation</li>
            <li><strong>Small Textures:</strong> Keep cursor textures small (32x32 to 64x64) for best performance</li>
            <li><strong>Cache Shaders:</strong> Register cursor shaders once during initialization, not every frame</li>
            <li><strong>Avoid Complex Shaders:</strong> Simple alpha-blended cursors perform best</li>
        </ul>

        <h3>Design Guidelines</h3>
        <ul>
            <li><strong>Visibility:</strong> Ensure cursor is visible against all backgrounds</li>
            <li><strong>Size:</strong> 32x32 pixels is standard, but can be adjusted for high-DPI displays</li>
            <li><strong>Hotspot:</strong> Center the hotspot at the cursor's visual center</li>
            <li><strong>Contrast:</strong> Use high contrast colors or outlines for visibility</li>
            <li><strong>Animation:</strong> Keep animations subtle to avoid distraction</li>
        </ul>

        <h3>Compatibility</h3>
        <ul>
            <li><strong>Format Support:</strong> TGA is most compatible, but JPG/PNG may work</li>
            <li><strong>Renderer Support:</strong> Test with both OpenGL and Vulkan renderers</li>
            <li><strong>Resolution Scaling:</strong> Ensure cursor looks good at all resolutions</li>
            <li><strong>Mod Compatibility:</strong> Consider how your cursor works with other mods</li>
        </ul>
    </div>

    <div class="section">
        <h2>Troubleshooting</h2>
        
        <h3>Common Issues</h3>
        
        <h4>Cursor Not Appearing</h4>
        <ul>
            <li>Check that <code>uis.cursor</code> is registered in <code>UI_Init()</code></li>
            <li>Verify the shader path matches your file location</li>
            <li>Ensure the shader file exists and is properly formatted</li>
            <li>Check that <code>UI_Refresh()</code> is calling the cursor drawing code</li>
        </ul>

        <h4>Cursor Appears Incorrectly</h4>
        <ul>
            <li>Verify image format is TGA with alpha channel</li>
            <li>Check that hotspot offset matches cursor size (typically -16, -16 for 32x32)</li>
            <li>Ensure shader uses correct blend function for transparency</li>
            <li>Test with different UI scales</li>
        </ul>

        <h4>Performance Issues</h4>
        <ul>
            <li>Reduce cursor texture size if too large</li>
            <li>Simplify shader effects if using complex shaders</li>
            <li>Limit animation frame rate</li>
            <li>Disable cursor trail effects if causing slowdown</li>
        </ul>
    </div>

    <div class="section">
        <h2>Example: Complete Custom Cursor Implementation</h2>
        
        <p>Here's a complete example of implementing a custom animated cursor:</p>
        <div class="code-block">
            <pre><code>// In ui_local.h
#define CURSOR_FRAMES 4
extern qhandle_t customCursorFrames[CURSOR_FRAMES];
extern int cursorAnimFrame;
extern int cursorAnimTime;

// In ui_qmenu.c (UI_Init function)
qhandle_t customCursorFrames[CURSOR_FRAMES];
int cursorAnimFrame = 0;
int cursorAnimTime = 0;

void UI_InitCustomCursor(void) {
    customCursorFrames[0] = trap_R_RegisterShaderNoMip("menu/art/cursor1");
    customCursorFrames[1] = trap_R_RegisterShaderNoMip("menu/art/cursor2");
    customCursorFrames[2] = trap_R_RegisterShaderNoMip("menu/art/cursor3");
    customCursorFrames[3] = trap_R_RegisterShaderNoMip("menu/art/cursor4");
    
    // Set default cursor to first frame
    uis.cursor = customCursorFrames[0];
}

// In ui_atoms.c (UI_Refresh function)
void UI_DrawAnimatedCursor(void) {
    int currentTime = uis.realtime;
    
    // Update animation frame
    if (currentTime - cursorAnimTime > 100) {  // 100ms per frame
        cursorAnimFrame = (cursorAnimFrame + 1) % CURSOR_FRAMES;
        cursorAnimTime = currentTime;
        uis.cursor = customCursorFrames[cursorAnimFrame];
    }
    
    // Draw cursor (existing code)
    UI_SetColor(NULL);
    UI_DrawHandlePic(uis.cursorx - 16, uis.cursory - 16, 32, 32, uis.cursor);
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="/ui/ui">UI System Documentation</a> - Learn about the UI framework</li>
            <li><a href="/development/modding">Modding Guide</a> - General modding information</li>
            <li><a href="/rendering/shaders">Shader System</a> - Shader creation and effects</li>
            <li><a href="/tools/asset-tools">Asset Tools</a> - Tools for creating game assets</li>
        </ul>
    </div>
</div>
