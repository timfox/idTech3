<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Implementing Scrolling Credits in id Tech 3</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            line-height: 1.6;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
        }
        
        .code-block {
            background: #f4f4f4;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
        
        .note {
            background: #fff3cd;
            padding: 10px;
            border-left: 4px solid #ffc107;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <h1>Implementing Scrolling Credits in id Tech 3</h1>
    
    <h2>1. Core Structure</h2>
    <p>The credits system consists of:</p>
    <ul>
        <li>Credit line structure</li>
        <li>Menu framework</li>
        <li>Drawing system</li>
        <li>Timing control</li>
    </ul>

    <h2>2. Credit Line Structure</h2>
    <div class="code-block">
        <pre>
typedef struct {
    char *string;      // The credit text
    int style;         // UI style flags (UI_SMALLFONT, UI_BIGFONT, etc)
    vec4_t *colour;    // Text color
} cr_line;

// Example credit data
cr_line final_credits[] = {
    { "GAME TITLE", UI_CENTER|UI_GIANTFONT, &color_headertext },
    { "Created by", UI_CENTER|UI_BIGFONT, &color_maintext },
    { "Development Team", UI_CENTER|UI_BIGFONT, &color_headertext },
    { NULL }  // Terminator
};</pre>
    </div>

    <h2>3. Menu Framework</h2>
    <div class="code-block">
        <pre>
typedef struct {
    menuframework_s menu;
} creditsmenu_t;

static creditsmenu_t s_credits;

void UI_CreditMenu(int text_id) {
    memset(&s_credits, 0, sizeof(s_credits));
    
    // Set credits data based on type
    if (!text_id) {
        credits = final_credits;
        credits_len = sizeof(final_credits);
        SCROLLSPEED = 5;
    } else {
        credits = intro_story;
        credits_len = sizeof(intro_story);
        SCROLLSPEED = 1.5;
    }
    
    s_credits.menu.draw = ScrollingCredits_Draw;
    s_credits.menu.key = UI_CreditMenu_Key;
    s_credits.menu.fullscreen = qtrue;
    UI_PushMenu(&s_credits.menu);
    
    starttime = uis.realtime;
}</pre>
    </div>

    <h2>4. Drawing System</h2>
    <div class="code-block">
        <pre>
static void ScrollingCredits_Draw(void) {
    int x = 320, y, n;
    
    // Draw background
    UI_DrawHandlePic(0, 0, 640, 480, Background);
    
    // Calculate initial Y position based on time
    y = 480 - SCROLLSPEED * (float)(uis.realtime - starttime) / 100;
    
    // Draw each credit line
    for(n = 0; n <= credits_len - 1; n++) {
        if(credits[n].string == NULL) {
            if(y < -16) {
                // Credits finished
                break;
            }
            break;
        }
        
        if(y > -(PROP_HEIGHT * (1 / PROP_SMALL_SIZE_SCALE))) {
            UI_DrawProportionalString(x, y, credits[n].string, 
                                    credits[n].style, *credits[n].colour);
        }
        
        // Adjust Y for next line based on font size
        if(credits[n].style & UI_SMALLFONT) {
            y += PROP_HEIGHT * PROP_SMALL_SIZE_SCALE;
        } else if(credits[n].style & UI_BIGFONT) {
            y += PROP_HEIGHT;
        } else if(credits[n].style & UI_GIANTFONT) {
            y += PROP_HEIGHT * (1 / PROP_SMALL_SIZE_SCALE);
        }
        
        if (y > 480) break;
    }
}</pre>
    </div>

    <h2>5. Key Handling</h2>
    <div class="code-block">
        <pre>
static sfxHandle_t UI_CreditMenu_Key(int key) {
    if(key & K_CHAR_FLAG) {
        return 0;
    }
    
    // Handle exit
    if (exit_game) {
        trap_Cmd_ExecuteText(EXEC_APPEND, va("s_musicvolume %f; quit\n", mvolume));
    } else {
        UI_SPArena_Start(NULL);
    }
    
    return 0;
}</pre>
    </div>

    <h2>6. Implementation Steps</h2>
    <ol>
        <li>Define your credit lines with appropriate styles and colors</li>
        <li>Set up the menu framework with draw and key handlers</li>
        <li>Implement the drawing system with proper timing</li>
        <li>Handle user input for skipping or exiting</li>
    </ol>

    <div class="note">
        <strong>Important:</strong> The system uses the engine's UI drawing functions and timing system. Make sure to properly integrate with the engine's rendering pipeline.
    </div>

    <h2>7. Performance Considerations</h2>
    <ul>
        <li>Only draw text that's visible on screen</li>
        <li>Use the engine's built-in text rendering functions</li>
        <li>Properly handle memory for credit line structures</li>
        <li>Consider screen resolution and aspect ratio</li>
    </ul>

    <div class="note">
        <strong>Remember:</strong> The credits system is integrated with the engine's menu system and uses its timing and rendering functions. Make sure to properly initialize and clean up resources.
    </div>
</body>
</html>


