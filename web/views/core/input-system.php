<?php
/**
 * Input System - id Tech 3 Engine Documentation
 */
$title = 'Input System - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/input-system' => 'Input System'
];
?>

<h1>Input System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 input system handles keyboard, mouse, and gamepad input with a flexible key binding system, support for multiple input devices, and customizable control schemes. The system is designed for low-latency, high-precision input required for competitive gaming.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Key Binding:</strong> Flexible command binding to any input</li>
            <li><strong>Multi-Device:</strong> Keyboard, mouse, and gamepad support</li>
            <li><strong>Low Latency:</strong> Direct input processing for minimal delay</li>
            <li><strong>Customizable:</strong> Per-user control configuration</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Input System Architecture</h2>
    
    <h3>Core Input Structures</h3>
    <div class="code-block">
        <pre><code>// keys.h - Input system definitions
typedef enum {
    K_TAB = 9,
    K_ENTER = 13,
    K_ESCAPE = 27,
    K_SPACE = 32,
    
    // Printable keys
    K_EXCLAIM = 33,     // !
    K_QUOTEDBL = 34,    // "
    K_HASH = 35,        // #
    // ... more printable keys
    
    // Arrow keys
    K_RIGHTARROW = 128,
    K_LEFTARROW,
    K_DOWNARROW,
    K_UPARROW,
    
    // Function keys
    K_F1, K_F2, K_F3, K_F4, K_F5, K_F6,
    K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_F13, K_F14, K_F15,
    
    // Keypad
    K_KP_HOME, K_KP_UPARROW, K_KP_PGUP,
    K_KP_LEFTARROW, K_KP_5, K_KP_RIGHTARROW,
    K_KP_END, K_KP_DOWNARROW, K_KP_PGDN,
    K_KP_ENTER, K_KP_INS, K_KP_DEL,
    K_KP_SLASH, K_KP_MINUS, K_KP_PLUS,
    K_KP_NUMLOCK, K_KP_STAR, K_KP_EQUALS,
    
    // Mouse buttons
    K_MOUSE1 = 200,
    K_MOUSE2,
    K_MOUSE3,
    K_MOUSE4,
    K_MOUSE5,
    
    // Mouse wheel
    K_MWHEELDOWN,
    K_MWHEELUP,
    
    // Joystick buttons
    K_JOY1 = 210,
    K_JOY2, K_JOY3, K_JOY4, K_JOY5, K_JOY6, K_JOY7, K_JOY8,
    K_JOY9, K_JOY10, K_JOY11, K_JOY12, K_JOY13, K_JOY14, K_JOY15, K_JOY16,
    K_JOY17, K_JOY18, K_JOY19, K_JOY20, K_JOY21, K_JOY22, K_JOY23, K_JOY24,
    K_JOY25, K_JOY26, K_JOY27, K_JOY28, K_JOY29, K_JOY30, K_JOY31, K_JOY32,
    
    // Auxiliary keys
    K_AUX1 = 250,
    K_AUX2, K_AUX3, K_AUX4, K_AUX5, K_AUX6, K_AUX7, K_AUX8,
    K_AUX9, K_AUX10, K_AUX11, K_AUX12, K_AUX13, K_AUX14, K_AUX15, K_AUX16,
    
    K_LAST_KEY
} keyNum_t;

// Key binding structure
typedef struct {
    char* binding;          // Command string to execute
    int repeats;           // Number of times key is being held
    qboolean wasPressed;   // Key state from last frame
} qkey_t;

// Global key state
extern qkey_t keys[K_LAST_KEY];

// Key name mapping for configuration
typedef struct {
    char* name;
    int keynum;
} keyname_t;

static keyname_t keynames[] = {
    {"TAB", K_TAB},
    {"ENTER", K_ENTER},
    {"ESCAPE", K_ESCAPE},
    {"SPACE", K_SPACE},
    {"BACKSPACE", K_BACKSPACE},
    {"UPARROW", K_UPARROW},
    {"DOWNARROW", K_DOWNARROW},
    {"LEFTARROW", K_LEFTARROW},
    {"RIGHTARROW", K_RIGHTARROW},
    {"ALT", K_ALT},
    {"CTRL", K_CTRL},
    {"SHIFT", K_SHIFT},
    {"F1", K_F1},
    {"F2", K_F2},
    // ... more key mappings
    {"MOUSE1", K_MOUSE1},
    {"MOUSE2", K_MOUSE2},
    {"MOUSE3", K_MOUSE3},
    {"MWHEELUP", K_MWHEELUP},
    {"MWHEELDOWN", K_MWHEELDOWN},
    {NULL, 0}
};</code></pre>
    </div>
    
    <h3>Input Event Processing</h3>
    <div class="code-block">
        <pre><code>// Input event handling and distribution
void Key_Event(int key, qboolean down, unsigned time) {
    char* kb;
    char cmd[1024];
    
    // Update key state
    keys[key].wasPressed = keys[key].down;
    keys[key].down = down;
    
    if (down) {
        keys[key].repeats++;
        if (keys[key].repeats == 1) {
            anykeydown++;
        }
    } else {
        keys[key].repeats = 0;
        anykeydown--;
        if (anykeydown < 0) {
            anykeydown = 0;
        }
    }
    
    // Console key handling
    if (key == K_CONSOLE || (keys[K_SHIFT].down && key == K_ESCAPE)) {
        if (!down) {
            return;
        }
        Con_ToggleConsole_f();
        return;
    }
    
    // Handle console input
    if (cls.keyCatchers & KEYCATCH_CONSOLE) {
        if (down) {
            Con_KeyEvent(key);
        }
        return;
    }
    
    // Handle UI input
    if (cls.keyCatchers & KEYCATCH_UI) {
        if (down) {
            UI_KeyEvent(key, down);
        }
        return;
    }
    
    // Handle CGAME input
    if (cls.keyCatchers & KEYCATCH_CGAME) {
        if (down) {
            CL_CGameKeyEvent(key, down);
        }
        return;
    }
    
    // Handle menu system
    if (cls.keyCatchers & KEYCATCH_MESSAGE) {
        if (down) {
            Message_Key(key);
        }
        return;
    }
    
    // Normal key binding processing
    if (!down) {
        return;
    }
    
    // Get the bound command
    kb = keys[key].binding;
    if (!kb) {
        if (key >= 200) {
            Com_Printf("%s is unbound, use controls menu to set.\n", Key_KeynumToString(key));
        }
        return;
    }
    
    // Special handling for button commands
    if (kb[0] == '+') {
        // Button commands
        int i;
        char button[1024];
        char* buttonPtr;
        
        Q_strncpyz(button, kb, sizeof(button));
        buttonPtr = button;
        
        // Handle +button commands
        if (down) {
            // Execute +command
            Cbuf_AddText(button);
            Cbuf_AddText("\n");
        } else {
            // Execute -command  
            button[0] = '-';
            Cbuf_AddText(button);
            Cbuf_AddText("\n");
        }
        return;
    }
    
    // Normal commands - only execute on key down
    if (down) {
        Cbuf_AddText(kb);
        Cbuf_AddText("\n");
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Key Binding System</h2>
    
    <h3>Binding Management</h3>
    <div class="code-block">
        <pre><code>// Key binding functions
void Key_SetBinding(int keynum, const char* binding) {
    if (keynum < 0 || keynum >= K_LAST_KEY) {
        return;
    }
    
    // Free existing binding
    if (keys[keynum].binding) {
        Z_Free(keys[keynum].binding);
        keys[keynum].binding = NULL;
    }
    
    // Set new binding
    if (binding) {
        keys[keynum].binding = CopyString(binding);
    }
}

char* Key_GetBinding(int keynum) {
    if (keynum < 0 || keynum >= K_LAST_KEY) {
        return "";
    }
    
    if (!keys[keynum].binding) {
        return "";
    }
    
    return keys[keynum].binding;
}

// Console commands for key binding
void Key_Bind_f(void) {
    int i, c, b;
    char cmd[1024];
    
    c = Cmd_Argc();
    
    if (c < 2) {
        Com_Printf("bind <key> [command] : attach a command to a key\n");
        return;
    }
    
    b = Key_StringToKeynum(Cmd_Argv(1));
    if (b == -1) {
        Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }
    
    if (c == 2) {
        if (keys[b].binding) {
            Com_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), keys[b].binding);
        } else {
            Com_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
        }
        return;
    }
    
    // Copy the rest of the command line
    cmd[0] = 0;
    for (i = 2; i < c; i++) {
        strcat(cmd, Cmd_Argv(i));
        if (i != (c - 1)) {
            strcat(cmd, " ");
        }
    }
    
    Key_SetBinding(b, cmd);
}

void Key_Unbind_f(void) {
    int b;
    
    if (Cmd_Argc() != 2) {
        Com_Printf("unbind <key> : remove commands from a key\n");
        return;
    }
    
    b = Key_StringToKeynum(Cmd_Argv(1));
    if (b == -1) {
        Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }
    
    Key_SetBinding(b, NULL);
}

void Key_Unbindall_f(void) {
    int i;
    
    for (i = 0; i < K_LAST_KEY; i++) {
        if (keys[i].binding) {
            Key_SetBinding(i, NULL);
        }
    }
}

void Key_Bindlist_f(void) {
    int i;
    
    for (i = 0; i < K_LAST_KEY; i++) {
        if (keys[i].binding && keys[i].binding[0]) {
            Com_Printf("%s \"%s\"\n", Key_KeynumToString(i), keys[i].binding);
        }
    }
}</code></pre>
    </div>
    
    <h3>Default Key Bindings</h3>
    <div class="code-block">
        <pre><code>// Default key binding setup
void CL_InitKeyCommands(void) {
    // Register key binding commands
    Cmd_AddCommand("bind", Key_Bind_f);
    Cmd_AddCommand("unbind", Key_Unbind_f);
    Cmd_AddCommand("unbindall", Key_Unbindall_f);
    Cmd_AddCommand("bindlist", Key_Bindlist_f);
    
    // Set default bindings
    Key_SetBinding(K_TAB, "+scores");
    Key_SetBinding('`', "toggleconsole");
    Key_SetBinding('~', "toggleconsole");
    Key_SetBinding(K_ESCAPE, "togglemenu");
    
    // Movement
    Key_SetBinding('w', "+forward");
    Key_SetBinding('s', "+back");
    Key_SetBinding('a', "+moveleft");
    Key_SetBinding('d', "+moveright");
    Key_SetBinding(K_SPACE, "+moveup");
    Key_SetBinding('c', "+movedown");
    Key_SetBinding(K_SHIFT, "+speed");
    Key_SetBinding(K_CTRL, "+strafe");
    
    // Weapons
    Key_SetBinding(K_MOUSE1, "+attack");
    Key_SetBinding(K_MOUSE2, "+button2");
    Key_SetBinding('1', "weapon 1");
    Key_SetBinding('2', "weapon 2");
    Key_SetBinding('3', "weapon 3");
    Key_SetBinding('4', "weapon 4");
    Key_SetBinding('5', "weapon 5");
    Key_SetBinding('6', "weapon 6");
    Key_SetBinding('7', "weapon 7");
    Key_SetBinding('8', "weapon 8");
    Key_SetBinding('9', "weapon 9");
    Key_SetBinding('0', "weapon 10");
    Key_SetBinding('[', "weapprev");
    Key_SetBinding(']', "weapnext");
    
    // Communication
    Key_SetBinding('t', "messagemode");
    Key_SetBinding('y', "messagemode2");
    
    // Function keys
    Key_SetBinding(K_F1, "vote yes");
    Key_SetBinding(K_F2, "vote no");
    Key_SetBinding(K_F3, "ready");
    Key_SetBinding(K_F4, "notready");
    Key_SetBinding(K_F5, "screenshot");
    
    // Screenshots and demos
    Key_SetBinding(K_F12, "screenshot");
    
    // Development
    Key_SetBinding(K_F11, "r_speeds");
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Mouse Input System</h2>
    
    <h3>Mouse Configuration</h3>
    <div class="code-block">
        <pre><code>// Mouse input handling and sensitivity
typedef struct {
    int windowWidth, windowHeight;
    int mousePos[2];
    int mouseDelta[2];
    qboolean mouseActive;
    qboolean mouseInitialized;
    float sensitivity;
    qboolean smoothing;
    qboolean accel;
    float accelThreshold;
    float accelPower;
} mouseInput_t;

static mouseInput_t mouse;

void IN_InitMouse(void) {
    // Register mouse CVars
    m_sensitivity = Cvar_Get("sensitivity", "5", CVAR_ARCHIVE);
    m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE);
    m_yaw = Cvar_Get("m_yaw", "0.022", CVAR_ARCHIVE);
    m_forward = Cvar_Get("m_forward", "0.25", CVAR_ARCHIVE);
    m_side = Cvar_Get("m_side", "0.25", CVAR_ARCHIVE);
    m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
    
    // Mouse acceleration
    m_accel = Cvar_Get("m_accel", "0", CVAR_ARCHIVE);
    m_accelStyle = Cvar_Get("m_accelStyle", "0", CVAR_ARCHIVE);
    m_accelOffset = Cvar_Get("m_accelOffset", "0", CVAR_ARCHIVE);
    m_accelPower = Cvar_Get("m_accelPower", "2", CVAR_ARCHIVE);
    
    // Initialize mouse state
    mouse.sensitivity = m_sensitivity->value;
    mouse.smoothing = m_filter->integer;
    mouse.accel = m_accel->value;
    mouse.mouseInitialized = qtrue;
}

void IN_MouseEvent(int mstate) {
    int i;
    
    if (!mouse.mouseInitialized) {
        return;
    }
    
    // Handle mouse button changes
    for (i = 0; i < 5; i++) {
        if ((mstate & (1 << i)) && !(mouse.buttonstate & (1 << i))) {
            Key_Event(K_MOUSE1 + i, qtrue, Sys_Milliseconds());
        }
        if (!(mstate & (1 << i)) && (mouse.buttonstate & (1 << i))) {
            Key_Event(K_MOUSE1 + i, qfalse, Sys_Milliseconds());
        }
    }
    
    mouse.buttonstate = mstate;
}

void IN_MouseMove(usercmd_t* cmd) {
    float mx, my;
    float rate;
    float accelSensitivity;
    float speed, power;
    
    if (!mouse.mouseActive) {
        return;
    }
    
    // Get raw mouse input
    mx = mouse.mouseDelta[0];
    my = mouse.mouseDelta[1];
    
    mouse.mouseDelta[0] = 0;
    mouse.mouseDelta[1] = 0;
    
    // Apply filtering
    if (m_filter->integer) {
        mouse.mouseAccumulation[0] = (mx + mouse.oldMouseAccumulation[0]) * 0.5f;
        mouse.mouseAccumulation[1] = (my + mouse.oldMouseAccumulation[1]) * 0.5f;
        
        mouse.oldMouseAccumulation[0] = mx;
        mouse.oldMouseAccumulation[1] = my;
        
        mx = mouse.mouseAccumulation[0];
        my = mouse.mouseAccumulation[1];
    }
    
    // Calculate frame rate scaling
    rate = sqrt(mx * mx + my * my) / (float)frame_msec;
    accelSensitivity = m_sensitivity->value;
    
    // Mouse acceleration
    if (m_accel->value != 0.0f) {
        if (m_accelStyle->integer == 0) {
            // Quake-style acceleration
            speed = sqrt(mx * mx + my * my);
            if (speed > m_accelOffset->value) {
                speed -= m_accelOffset->value;
                if (speed < m_accelThreshold->value) {
                    accelSensitivity += speed * m_accel->value;
                } else {
                    accelSensitivity += m_accelThreshold->value * m_accel->value;
                }
            }
        } else {
            // Power function acceleration
            speed = sqrt(mx * mx + my * my);
            power = powf(speed / m_accelPower->value, m_accelPower->value);
            accelSensitivity *= power;
        }
    }
    
    // Apply sensitivity
    mx *= accelSensitivity;
    my *= accelSensitivity;
    
    // Add to movement commands
    if (!in_strafe.active) {
        cmd->angles[YAW] -= ANGLE2SHORT(mx * m_yaw->value);
    } else {
        cmd->rightmove = ClampChar(cmd->rightmove + m_side->value * mx);
    }
    
    if ((in_mlooking || freelook->integer) && !in_strafe.active) {
        cmd->angles[PITCH] += ANGLE2SHORT(my * m_pitch->value);
    } else {
        cmd->forwardmove -= ClampChar(cmd->forwardmove - m_forward->value * my);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Gamepad Support</h2>
    
    <h3>Joystick/Gamepad Integration</h3>
    <div class="code-block">
        <pre><code>// Gamepad/joystick support
typedef struct {
    qboolean avail;
    int id;
    char name[128];
    int numAxes;
    int numButtons;
    int numHats;
    float axes[MAX_JOYSTICK_AXIS];
    unsigned char buttons[MAX_JOYSTICK_BUTTONS];
    unsigned char hats[4];
    qboolean valid;
} joystick_t;

static joystick_t joy;

void IN_InitJoystick(void) {
    // Register joystick CVars
    in_joystick = Cvar_Get("in_joystick", "0", CVAR_ARCHIVE | CVAR_LATCH);
    in_joystickThreshold = Cvar_Get("in_joystickThreshold", "0.15", CVAR_ARCHIVE);
    
    j_pitch = Cvar_Get("j_pitch", "0.022", CVAR_ARCHIVE);
    j_yaw = Cvar_Get("j_yaw", "0.022", CVAR_ARCHIVE);
    j_forward = Cvar_Get("j_forward", "-0.25", CVAR_ARCHIVE);
    j_side = Cvar_Get("j_side", "0.25", CVAR_ARCHIVE);
    j_up = Cvar_Get("j_up", "0", CVAR_ARCHIVE);
    j_pitch_axis = Cvar_Get("j_pitch_axis", "3", CVAR_ARCHIVE);
    j_yaw_axis = Cvar_Get("j_yaw_axis", "2", CVAR_ARCHIVE);
    j_forward_axis = Cvar_Get("j_forward_axis", "1", CVAR_ARCHIVE);
    j_side_axis = Cvar_Get("j_side_axis", "0", CVAR_ARCHIVE);
    j_up_axis = Cvar_Get("j_up_axis", "4", CVAR_ARCHIVE);
    
    if (!in_joystick->integer) {
        Com_Printf("Joystick is not active.\n");
        return;
    }
    
    // Initialize SDL joystick subsystem
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        Com_Printf("SDL joystick initialization failed: %s\n", SDL_GetError());
        return;
    }
    
    // Detect available joysticks
    int numJoysticks = SDL_NumJoysticks();
    if (numJoysticks <= 0) {
        Com_Printf("No joysticks detected.\n");
        return;
    }
    
    // Open first available joystick
    joy.gameController = SDL_JoystickOpen(0);
    if (!joy.gameController) {
        Com_Printf("Failed to open joystick: %s\n", SDL_GetError());
        return;
    }
    
    // Get joystick information
    Q_strncpyz(joy.name, SDL_JoystickName(joy.gameController), sizeof(joy.name));
    joy.numAxes = SDL_JoystickNumAxes(joy.gameController);
    joy.numButtons = SDL_JoystickNumButtons(joy.gameController);
    joy.numHats = SDL_JoystickNumHats(joy.gameController);
    
    Com_Printf("Joystick %s opened\n", joy.name);
    Com_Printf("Axes: %d, Buttons: %d, Hats: %d\n", 
              joy.numAxes, joy.numButtons, joy.numHats);
    
    joy.avail = qtrue;
}

void IN_JoyMove(usercmd_t* cmd) {
    float   fAxisValue;
    int     i;
    unsigned char buttonstate;
    
    if (!joy.avail) {
        return;
    }
    
    SDL_JoystickUpdate();
    
    // Handle analog sticks
    if (joy.numAxes) {
        // Left stick - movement
        if (j_forward_axis->integer >= 0 && j_forward_axis->integer < joy.numAxes) {
            fAxisValue = SDL_JoystickGetAxis(joy.gameController, j_forward_axis->integer) / 32767.0f;
            if (fabs(fAxisValue) > in_joystickThreshold->value) {
                cmd->forwardmove = ClampChar(cmd->forwardmove + (int)(fAxisValue * 127.0f * j_forward->value));
            }
        }
        
        if (j_side_axis->integer >= 0 && j_side_axis->integer < joy.numAxes) {
            fAxisValue = SDL_JoystickGetAxis(joy.gameController, j_side_axis->integer) / 32767.0f;
            if (fabs(fAxisValue) > in_joystickThreshold->value) {
                cmd->rightmove = ClampChar(cmd->rightmove + (int)(fAxisValue * 127.0f * j_side->value));
            }
        }
        
        // Right stick - looking
        if (j_yaw_axis->integer >= 0 && j_yaw_axis->integer < joy.numAxes) {
            fAxisValue = SDL_JoystickGetAxis(joy.gameController, j_yaw_axis->integer) / 32767.0f;
            if (fabs(fAxisValue) > in_joystickThreshold->value) {
                cmd->angles[YAW] += ANGLE2SHORT(fAxisValue * j_yaw->value * 10);
            }
        }
        
        if (j_pitch_axis->integer >= 0 && j_pitch_axis->integer < joy.numAxes) {
            fAxisValue = SDL_JoystickGetAxis(joy.gameController, j_pitch_axis->integer) / 32767.0f;
            if (fabs(fAxisValue) > in_joystickThreshold->value) {
                cmd->angles[PITCH] += ANGLE2SHORT(fAxisValue * j_pitch->value * 10);
            }
        }
        
        // Triggers
        if (j_up_axis->integer >= 0 && j_up_axis->integer < joy.numAxes) {
            fAxisValue = SDL_JoystickGetAxis(joy.gameController, j_up_axis->integer) / 32767.0f;
            if (fabs(fAxisValue) > in_joystickThreshold->value) {
                cmd->upmove = ClampChar(cmd->upmove + (int)(fAxisValue * 127.0f * j_up->value));
            }
        }
    }
    
    // Handle buttons
    for (i = 0; i < joy.numButtons && i < 32; i++) {
        buttonstate = SDL_JoystickGetButton(joy.gameController, i);
        
        if (buttonstate != joy.buttons[i]) {
            Key_Event(K_JOY1 + i, buttonstate ? qtrue : qfalse, Sys_Milliseconds());
            joy.buttons[i] = buttonstate;
        }
    }
    
    // Handle D-pad (hat)
    if (joy.numHats) {
        unsigned char hat = SDL_JoystickGetHat(joy.gameController, 0);
        
        // Handle D-pad as discrete button presses
        Key_Event(K_HATUP, (hat & SDL_HAT_UP) ? qtrue : qfalse, Sys_Milliseconds());
        Key_Event(K_HATDOWN, (hat & SDL_HAT_DOWN) ? qtrue : qfalse, Sys_Milliseconds());
        Key_Event(K_HATLEFT, (hat & SDL_HAT_LEFT) ? qtrue : qfalse, Sys_Milliseconds());
        Key_Event(K_HATRIGHT, (hat & SDL_HAT_RIGHT) ? qtrue : qfalse, Sys_Milliseconds());
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Input Configuration</h2>
    
    <h3>CVars for Input Customization</h3>
    <div class="code-block">
        <pre><code>// Input system configuration variables

// Mouse settings
seta sensitivity "5.0"          // Mouse sensitivity multiplier
seta m_pitch "0.022"           // Mouse pitch (up/down) sensitivity  
seta m_yaw "0.022"             // Mouse yaw (left/right) sensitivity
seta m_forward "0.25"          // Mouse forward/back when moving
seta m_side "0.25"             // Mouse strafe sensitivity
seta m_filter "0"              // Mouse smoothing filter (0/1)

// Mouse acceleration  
seta m_accel "0"               // Mouse acceleration amount
seta m_accelStyle "0"          // Acceleration style (0=Quake, 1=Power)
seta m_accelOffset "0"         // Acceleration threshold offset
seta m_accelPower "2"          // Power function exponent

// Joystick settings
seta in_joystick "0"           // Enable joystick (0/1)
seta in_joystickThreshold "0.15" // Analog stick deadzone
seta j_pitch "0.022"           // Joystick pitch sensitivity
seta j_yaw "0.022"             // Joystick yaw sensitivity  
seta j_forward "-0.25"         // Joystick forward/back sensitivity
seta j_side "0.25"             // Joystick strafe sensitivity
seta j_up "0"                  // Joystick up/down sensitivity

// Joystick axis mapping
seta j_pitch_axis "3"          // Right stick Y-axis for pitch
seta j_yaw_axis "2"            // Right stick X-axis for yaw
seta j_forward_axis "1"        // Left stick Y-axis for movement
seta j_side_axis "0"           // Left stick X-axis for strafing
seta j_up_axis "4"             // Trigger or other axis for up/down

// Keyboard settings
seta cl_run "1"                // Always run (0/1)
seta in_keyboardDebug "0"      // Show keyboard debug info</code></pre>
    </div>
    
    <h3>Advanced Input Bindings</h3>
    <div class="code-block">
        <pre><code>// Advanced binding examples and techniques

// Multi-command bindings
bind F1 "say_team Need backup!; play sound/teamplay/flagcapture_yourteam.wav"

// Conditional bindings with script-like logic
bind MOUSE3 "vstr zoomtoggle"
seta zoom_in "weapnext; sensitivity 1; set zoomtoggle vstr zoom_out"
seta zoom_out "weapprev; sensitivity 5; set zoomtoggle vstr zoom_in" 
seta zoomtoggle "vstr zoom_in"

// Weapon cycling with fallback
bind q "weapon 3; wait; weapon 2; wait; weapon 1"

// Communication aliases
alias sorry "say_team Sorry!"
alias thanks "say_team Thanks!"
alias incoming "say_team Incoming!"
bind KP_INS "sorry"
bind KP_DEL "thanks"  
bind KP_ENTER "incoming"

// Movement combinations
bind SPACE "+moveup; +speed"    // Jump + run
bind ALT "+strafe; +speed"      // Strafe run

// Complex weapon bindings
bind 1 "weapon 1; cg_drawgun 1"         // Gauntlet + show weapon
bind 2 "weapon 2; cg_drawgun 1"         // Machinegun + show weapon
bind 3 "weapon 3; cg_drawgun 0"         // Shotgun + hide weapon for better aim

// Context-sensitive bindings (via scripting)
alias +context_fire "vstr currentweapon_fire"
alias -context_fire "vstr currentweapon_stopfire"
bind MOUSE1 "+context_fire"

// Zoom toggle with sensitivity scaling  
alias +zoom "sensitivity 2; cg_fov 90; cg_zoomfov 20"
alias -zoom "sensitivity 5; cg_fov 90"
bind MOUSE2 "+zoom"

// Quick configuration switches
alias config_duel "exec duel.cfg; echo Duel config loaded"
alias config_ffa "exec ffa.cfg; echo FFA config loaded"  
alias config_ctf "exec ctf.cfg; echo CTF config loaded"

// Performance bindings for debugging
bind F9 "toggle r_speeds; toggle cg_drawfps"
bind F10 "toggle r_showtris; toggle r_shownormals"
bind F11 "screenshot; echo Screenshot taken"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Platform-Specific Input</h2>
    
    <h3>Windows Input</h3>
    <div class="code-block">
        <pre><code>// Windows-specific input handling
#ifdef _WIN32

#include <windows.h>
#include <dinput.h>

// DirectInput setup for advanced mouse/keyboard
LPDIRECTINPUT8 g_pDI = NULL;
LPDIRECTINPUTDEVICE8 g_pMouse = NULL;
LPDIRECTINPUTDEVICE8 g_pKeyboard = NULL;

qboolean IN_InitDirectInput(void) {
    HRESULT hr;
    
    // Create DirectInput object
    hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                           IID_IDirectInput8, (VOID**)&g_pDI, NULL);
    if (FAILED(hr)) {
        return qfalse;
    }
    
    // Create mouse device
    hr = g_pDI->CreateDevice(GUID_SysMouse, &g_pMouse, NULL);
    if (FAILED(hr)) {
        return qfalse;
    }
    
    // Set mouse data format
    hr = g_pMouse->SetDataFormat(&c_dfDIMouse2);
    if (FAILED(hr)) {
        return qfalse;
    }
    
    // Set mouse cooperative level
    hr = g_pMouse->SetCooperativeLevel(g_wv.hWnd, 
                                      DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    if (FAILED(hr)) {
        return qfalse;
    }
    
    return qtrue;
}

void IN_ReadDirectInputMouse(void) {
    DIMOUSESTATE2 dims2;
    HRESULT hr;
    
    if (!g_pMouse) {
        return;
    }
    
    // Poll the mouse
    g_pMouse->Poll();
    
    // Get mouse state
    hr = g_pMouse->GetDeviceState(sizeof(DIMOUSESTATE2), &dims2);
    if (FAILED(hr)) {
        // Try to reacquire
        g_pMouse->Acquire();
        return;
    }
    
    // Update mouse deltas
    mouse.mouseDelta[0] += dims2.lX;
    mouse.mouseDelta[1] += dims2.lY;
    
    // Handle mouse wheel
    if (dims2.lZ > 0) {
        Key_Event(K_MWHEELUP, qtrue, Sys_Milliseconds());
        Key_Event(K_MWHEELUP, qfalse, Sys_Milliseconds());
    } else if (dims2.lZ < 0) {
        Key_Event(K_MWHEELDOWN, qtrue, Sys_Milliseconds());
        Key_Event(K_MWHEELDOWN, qfalse, Sys_Milliseconds());
    }
    
    // Handle buttons
    for (int i = 0; i < 8; i++) {
        if ((dims2.rgbButtons[i] & 0x80) && !(mouse.oldButtonState & (1 << i))) {
            Key_Event(K_MOUSE1 + i, qtrue, Sys_Milliseconds());
        }
        if (!(dims2.rgbButtons[i] & 0x80) && (mouse.oldButtonState & (1 << i))) {
            Key_Event(K_MOUSE1 + i, qfalse, Sys_Milliseconds());
        }
    }
    
    // Update button state
    mouse.oldButtonState = 0;
    for (int i = 0; i < 8; i++) {
        if (dims2.rgbButtons[i] & 0x80) {
            mouse.oldButtonState |= (1 << i);
        }
    }
}

#endif // _WIN32</code></pre>
    </div>
    
    <h3>Linux Input</h3>
    <div class="code-block">
        <pre><code>// Linux-specific input via X11/evdev
#ifdef __linux__

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <linux/input.h>

void IN_ProcessX11Events(void) {
    XEvent event;
    
    while (XPending(dpy)) {
        XNextEvent(dpy, &event);
        
        switch (event.type) {
            case KeyPress:
            case KeyRelease: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                int key = IN_TranslateX11Key(keysym);
                if (key) {
                    Key_Event(key, event.type == KeyPress, Sys_Milliseconds());
                }
                break;
            }
            
            case ButtonPress:
            case ButtonRelease: {
                int button = event.xbutton.button;
                qboolean down = (event.type == ButtonPress);
                
                switch (button) {
                    case 1: Key_Event(K_MOUSE1, down, Sys_Milliseconds()); break;
                    case 2: Key_Event(K_MOUSE3, down, Sys_Milliseconds()); break;
                    case 3: Key_Event(K_MOUSE2, down, Sys_Milliseconds()); break;
                    case 4: 
                        if (down) {
                            Key_Event(K_MWHEELUP, qtrue, Sys_Milliseconds());
                            Key_Event(K_MWHEELUP, qfalse, Sys_Milliseconds());
                        }
                        break;
                    case 5:
                        if (down) {
                            Key_Event(K_MWHEELDOWN, qtrue, Sys_Milliseconds());
                            Key_Event(K_MWHEELDOWN, qfalse, Sys_Milliseconds());
                        }
                        break;
                    default:
                        if (button < 16) {
                            Key_Event(K_MOUSE1 + button - 1, down, Sys_Milliseconds());
                        }
                        break;
                }
                break;
            }
            
            case MotionNotify: {
                // Raw mouse movement
                if (mouse.mouseActive) {
                    mouse.mouseDelta[0] += event.xmotion.x - mouse.lastX;
                    mouse.mouseDelta[1] += event.xmotion.y - mouse.lastY;
                }
                mouse.lastX = event.xmotion.x;
                mouse.lastY = event.xmotion.y;
                break;
            }
        }
    }
}

#endif // __linux__</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/core/console-system">Console System</a></li>
        <li><a href="/core/engine-subsystems">Engine Subsystems</a></li>
        <li><a href="/development/debugging">Debugging Tools</a></li>
        <li><a href="/ui/ui">User Interface</a></li>
        <li><a href="/platform/cross-platform">Cross-Platform Development</a></li>
    </ul>
</div>