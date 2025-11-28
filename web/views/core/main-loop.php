<?php
/**
 * Main Loop Analysis - id Tech 3 Frame Processing
 */
$title = 'Main Loop Analysis - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/main-loop' => 'Main Loop Analysis'
];
?>

<h1>Main Loop Analysis - Frame Processing and Timing</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 main loop is the central coordinator of all engine systems, managing frame timing, system updates, and ensuring smooth gameplay. Understanding its architecture is crucial for performance optimization and system integration.</p>
    
    <div class="feature-list">
        <h3>Main Loop Responsibilities</h3>
        <ul>
            <li><strong>Frame Timing:</strong> Precise control of frame rate and delta time</li>
            <li><strong>System Coordination:</strong> Orchestrate updates across all subsystems</li>
            <li><strong>Event Processing:</strong> Handle input, network, and system events</li>
            <li><strong>Performance Monitoring:</strong> Track timing and identify bottlenecks</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Main Loop Architecture</h2>
    
    <h3>Core Loop Structure</h3>
    <div class="code-block">
        <pre><code>// qcommon.c - Main engine loop
// This is the heart of the id Tech 3 engine

int main(int argc, char** argv) {
    // Platform and engine initialization
    Sys_PlatformInit();
    Com_Init(argc, argv);
    
    // Main execution loop
    while (1) {
        // Exception handling for error recovery
        if (setjmp(abortframe)) {
            continue; // Jump here on non-fatal errors
        }
        
        try {
            // Process one complete frame
            Com_Frame();
        } catch (...) {
            // Handle fatal errors
            break;
        }
    }
    
    // Cleanup and shutdown
    Com_Shutdown();
    return 0;
}

// The core frame processing function
void Com_Frame(void) {
    int timeBeforeFirstEvents;
    int timeBeforeServer; 
    int timeBeforeEvents;
    int timeBeforeClient;
    int timeAfter;
    
    // Measure frame start time
    timeBeforeFirstEvents = Sys_Milliseconds();
    
    // 1. Handle events (input, network, console)
    Com_EventLoop();
    
    // 2. Frame rate limiting and timing
    Com_FrameRateLimit();
    
    // 3. Calculate frame timing
    Com_CalculateFrameTiming();
    
    // 4. Server processing
    timeBeforeServer = Sys_Milliseconds();
    SV_Frame(com_frameMsec);
    timeAfterServer = Sys_Milliseconds();
    
    // 5. Client processing  
    timeBeforeClient = Sys_Milliseconds();
    CL_Frame(com_frameMsec);
    timeAfterClient = Sys_Milliseconds();
    
    // 6. Performance monitoring and reporting
    Com_PerformanceMonitoring(timeBeforeFirstEvents, timeBeforeServer,
                             timeAfterServer, timeBeforeClient, timeAfterClient);
}</code></pre>
    </div>
    
    <h3>Frame Timing Variables</h3>
    <div class="code-block">
        <pre><code>// Global timing state
static int com_frameTime;       // Current frame timestamp
static int com_frameMsec;       // Frame duration in milliseconds  
static int com_lastFrameTime;   // Previous frame timestamp
static float com_frameTimeRemainder; // Sub-millisecond timing remainder

// Configuration variables
extern cvar_t* com_maxfps;      // Maximum frame rate
extern cvar_t* com_fixedtime;   // Fixed timestep mode
extern cvar_t* com_timescale;   // Time scaling factor
extern cvar_t* com_speeds;      // Performance monitoring

// Timing calculation with precision handling
void Com_CalculateFrameTiming(void) {
    int currentTime = Sys_Milliseconds();
    
    if (com_fixedtime->integer) {
        // Fixed timestep mode for debugging/testing
        com_frameMsec = com_fixedtime->integer;
        com_frameTime = com_lastFrameTime + com_frameMsec;
    } else {
        // Variable timestep
        com_frameMsec = currentTime - com_lastFrameTime;
        com_frameTime = currentTime;
    }
    
    // Apply time scaling
    if (com_timescale->value != 1.0f) {
        com_frameMsec *= com_timescale->value;
    }
    
    // Clamp frame time to prevent simulation instability
    if (com_frameMsec < 1) {
        com_frameMsec = 1;
    } else if (com_frameMsec > 200) {
        // Clamp to 5 FPS minimum to prevent spiral of death
        com_frameMsec = 200;
    }
    
    com_lastFrameTime = currentTime;
}

// Frame rate limiting with high precision
void Com_FrameRateLimit(void) {
    if (com_maxfps->integer <= 0) {
        return; // No frame rate limit
    }
    
    // Calculate minimum frame time
    float targetFrameTime = 1000.0f / com_maxfps->integer;
    int minMsec = (int)targetFrameTime;
    com_frameTimeRemainder += targetFrameTime - minMsec;
    
    // Handle sub-millisecond precision
    if (com_frameTimeRemainder >= 1.0f) {
        minMsec += (int)com_frameTimeRemainder;
        com_frameTimeRemainder -= (int)com_frameTimeRemainder;
    }
    
    // Wait for target frame time
    while (1) {
        int currentTime = Sys_Milliseconds();
        int elapsed = currentTime - com_frameTime;
        
        if (elapsed >= minMsec) {
            break;
        }
        
        // Intelligent sleeping to avoid busy waiting
        int remainingTime = minMsec - elapsed;
        if (remainingTime > 2) {
            Sys_Sleep(remainingTime - 1);
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Event Processing</h2>
    
    <h3>Event Loop Implementation</h3>
    <div class="code-block">
        <pre><code>// Event processing drives the entire engine
void Com_EventLoop(void) {
    sysEvent_t ev;
    
    while (1) {
        // Get next system event
        ev = Com_GetEvent();
        
        // No more events this frame
        if (ev.evType == SE_NONE) {
            break;
        }
        
        // Route events to appropriate subsystems
        switch (ev.evType) {
        case SE_KEY:
            // Keyboard input
            CL_KeyEvent(ev.evValue, ev.evValue2, ev.evTime);
            break;
            
        case SE_CHAR:
            // Character input for text entry
            CL_CharEvent(ev.evValue);
            break;
            
        case SE_MOUSE:
            // Mouse movement and buttons
            CL_MouseEvent(ev.evValue, ev.evValue2, ev.evTime);
            break;
            
        case SE_JOYSTICK_AXIS:
            // Gamepad/joystick input
            CL_JoystickEvent(ev.evValue, ev.evValue2, ev.evTime);
            break;
            
        case SE_CONSOLE:
            // Console command execution
            Cbuf_AddText((char*)ev.evPtr);
            Cbuf_AddText("\n");
            break;
            
        default:
            Com_Error(ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType);
            break;
        }
        
        // Free event memory if allocated
        if (ev.evPtr) {
            Z_Free(ev.evPtr);
        }
    }
    
    // Execute console commands accumulated this frame
    Cbuf_Execute();
}

// Platform-specific event gathering
sysEvent_t Com_GetEvent(void) {
    // Check for events from multiple sources
    sysEvent_t ev;
    
    // 1. Check for network events
    ev = NET_GetEvent();
    if (ev.evType != SE_NONE) {
        return ev;
    }
    
    // 2. Check for input events  
    ev = IN_GetEvent();
    if (ev.evType != SE_NONE) {
        return ev;
    }
    
    // 3. Check for system events (window close, etc.)
    ev = Sys_GetEvent();
    if (ev.evType != SE_NONE) {
        return ev;
    }
    
    // No events available
    ev.evType = SE_NONE;
    ev.evTime = 0;
    return ev;
}</code></pre>
    </div>
    
    <h3>Command Buffer Management</h3>
    <div class="code-block">
        <pre><code>// Console command buffer for deferred execution
#define MAX_CMD_BUFFER 16384
#define MAX_CMD_LINE 1024

typedef struct {
    byte data[MAX_CMD_BUFFER];
    int cursize;
} cmd_buffer_t;

static cmd_buffer_t cmd_text;
static cmd_buffer_t defer_text_buf;

// Add commands to buffer for execution
void Cbuf_AddText(const char* text) {
    int l = strlen(text);
    
    if (cmd_text.cursize + l >= MAX_CMD_BUFFER) {
        Com_Printf("Cbuf_AddText: overflow\n");
        return;
    }
    
    memcpy(&cmd_text.data[cmd_text.cursize], text, l);
    cmd_text.cursize += l;
}

// Execute all commands in buffer
void Cbuf_Execute(void) {
    int i;
    char* text;
    char line[MAX_CMD_LINE];
    int quotes;
    
    while (cmd_text.cursize) {
        // Find a complete line
        text = (char*)cmd_text.data;
        quotes = 0;
        
        for (i = 0; i < cmd_text.cursize; i++) {
            if (text[i] == '"') {
                quotes++;
            }
            
            if (!(quotes & 1) && text[i] == ';') {
                break; // Semicolon separates commands
            }
            
            if (text[i] == '\n' || text[i] == '\r') {
                break;
            }
        }
        
        if (i >= MAX_CMD_LINE - 1) {
            i = MAX_CMD_LINE - 1;
        }
        
        memcpy(line, text, i);
        line[i] = 0;
        
        // Delete the text from the command buffer and move remaining commands down
        if (i == cmd_text.cursize) {
            cmd_text.cursize = 0;
        } else {
            i++;
            cmd_text.cursize -= i;
            memmove(text, text + i, cmd_text.cursize);
        }
        
        // Execute the command line
        Cmd_ExecuteString(line);
        
        // Check for defer commands that need to run next frame
        if (defer_text_buf.cursize) {
            memcpy(&cmd_text.data[cmd_text.cursize], defer_text_buf.data, defer_text_buf.cursize);
            cmd_text.cursize += defer_text_buf.cursize;
            defer_text_buf.cursize = 0;
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Server Frame Processing</h2>
    
    <h3>Server Main Loop</h3>
    <div class="code-block">
        <pre><code>// sv_main.c - Server frame processing
void SV_Frame(int msec) {
    if (!com_sv_running->integer) {
        return;
    }
    
    // Server timing
    sv.timeResidual += msec;
    
    // Run multiple server frames if needed to catch up
    while (sv.timeResidual >= SV_FRAME_MSEC) {
        sv.timeResidual -= SV_FRAME_MSEC;
        sv.time += SV_FRAME_MSEC;
        
        // Process server frame
        SV_RunFrame();
    }
    
    // Handle client connections and messages
    SV_CheckTimeouts();
    SV_CheckPaused();
    SV_RunFrame();
}

void SV_RunFrame(void) {
    int i;
    client_t* cl;
    
    // Read packets from clients
    SV_PacketEvent();
    
    // Update server time
    sv.time += SV_FRAME_MSEC;
    
    // Run game simulation
    if (sv.state == SS_GAME) {
        // Execute game frame
        VM_Call(gvm, GAME_RUN_FRAME, sv.time);
        
        // Check for map changes
        if (sv.mapname[0] == 0) {
            return; // Map changed, exit frame
        }
    }
    
    // Send messages to all connected clients
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->state == CS_FREE) {
            continue;
        }
        
        if (svs.time < cl->nextSnapshotTime) {
            continue; // Not time to send snapshot yet
        }
        
        // Build and send snapshot
        SV_SendClientSnapshot(cl);
        cl->nextSnapshotTime = svs.time + SV_SNAPSHOT_MSEC;
    }
    
    // Master server communication
    SV_MasterHeartbeat();
}</code></pre>
    </div>
    
    <h3>Client Message Processing</h3>
    <div class="code-block">
        <pre><code>// Process incoming client packets
void SV_PacketEvent(void) {
    netadr_t from;
    msg_t netmsg;
    byte msgBuffer[MAX_MSGLEN];
    
    while (NET_GetPacket(NS_SERVER, &from, &netmsg)) {
        // Setup message buffer
        MSG_Init(&netmsg, msgBuffer, sizeof(msgBuffer));
        
        // Find client by address
        client_t* cl = SV_ReadPackets(&from);
        
        if (!cl) {
            // Connectionless packet or new client
            SV_ConnectionlessPacket(&from, &netmsg);
            continue;
        }
        
        // Process client message
        if (cl->state != CS_ZOMBIE) {
            SV_ExecuteClientMessage(cl, &netmsg);
        }
    }
}

// Execute client commands and update state
void SV_ExecuteClientMessage(client_t* cl, msg_t* msg) {
    int serverId, messageAcknowledge, reliableAcknowledge;
    usercmd_t nullcmd;
    usercmd_t cmds[MAX_PACKET_USERCMDS];
    int numCmds = 0;
    
    MSG_BeginReadingOOB(msg);
    
    // Read message header
    serverId = MSG_ReadLong(msg);
    messageAcknowledge = MSG_ReadLong(msg);
    reliableAcknowledge = MSG_ReadLong(msg);
    
    if (serverId != sv.serverId) {
        return; // Wrong server instance
    }
    
    // Update reliable acknowledgments
    cl->reliableAcknowledge = reliableAcknowledge;
    
    // Read user commands
    memset(&nullcmd, 0, sizeof(nullcmd));
    
    for (numCmds = 0; numCmds < MAX_PACKET_USERCMDS; numCmds++) {
        if (MSG_ReadBits(msg, 1) == 0) {
            break;
        }
        
        usercmd_t* cmd = &cmds[numCmds];
        MSG_ReadDeltaUsercmd(msg, &nullcmd, cmd);
        
        // Prevent time travel
        if (cmd->serverTime > cmds[numCmds-1].serverTime) {
            cmds[numCmds-1] = *cmd;
        }
        
        nullcmd = *cmd;
    }
    
    // Execute user commands
    for (int i = 0; i < numCmds; i++) {
        if (sv.time < cmds[i].serverTime) {
            continue; // Command is in the future
        }
        
        SV_ClientThink(cl, &cmds[i]);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Client Frame Processing</h2>
    
    <h3>Client Main Loop</h3>
    <div class="code-block">
        <pre><code>// cl_main.c - Client frame processing  
void CL_Frame(int msec) {
    static int extratime = 0;
    static int lastFrameTime = 0;
    int frameMsec;
    
    // Accumulate time for variable frame rates
    extratime += msec;
    
    // Don't flood server with packets
    if (!cl_timedemo->integer) {
        if (cls.state == CA_CONNECTED && extratime < 100) {
            return;
        }
    }
    
    frameMsec = extratime;
    extratime = 0;
    
    cl.oldFrameTime = cl.frameTime;
    cl.frameTime = cls.realtime;
    
    // Update subsystems based on client state
    switch (cls.state) {
    case CA_DISCONNECTED:
        SCR_UpdateScreen();
        S_Update();
        Con_RunConsole();
        break;
        
    case CA_CONNECTING:
    case CA_CHALLENGING:
    case CA_CONNECTED:
        // Send connect packets
        CL_CheckForResend();
        SCR_UpdateScreen();
        S_Update();
        break;
        
    case CA_LOADING:
        // Loading map or assets
        SCR_UpdateScreen();
        break;
        
    case CA_PRIMED:
        // Ready to enter game
        CL_SendPureChecksums();
        SCR_UpdateScreen();
        break;
        
    case CA_ACTIVE:
        // In-game processing
        CL_ActiveFrame(frameMsec);
        break;
    }
    
    // Process downloads
    CL_WWWDownload();
}

// Active gameplay frame processing
void CL_ActiveFrame(int msec) {
    // Read packets from server
    CL_ReadPackets();
    
    // Generate user commands
    CL_CreateNewCommands();
    CL_CreateCmd();
    
    // Send commands to server
    CL_SendCmd();
    
    // Predict player movement
    CL_PredictMovement();
    
    // Update local entities (shells, gibs, etc.)
    CL_UpdateLocalEntities();
    
    // Update particle systems
    CL_UpdateExplosions();
    CL_UpdateDLights();
    CL_UpdateLightStyles();
    
    // Update audio system
    S_Update();
    
    // Render the frame
    SCR_UpdateScreen();
    
    // Update timing statistics
    CL_UpdateFrameStats();
}</code></pre>
    </div>
    
    <h3>Client Prediction and Smoothing</h3>
    <div class="code-block">
        <pre><code>// Client-side prediction for smooth gameplay
void CL_PredictMovement(void) {
    int cmdNum;
    usercmd_t* cmd;
    playerState_t oldPlayerState;
    int delta;
    
    if (cls.state != CA_ACTIVE || cl.demoplayback || 
        cl.snap.ps.pm_flags & PMF_FOLLOW) {
        return;
    }
    
    // Copy current player state for prediction
    oldPlayerState = cl.snap.ps;
    
    // Run prediction from last server snapshot
    cmdNum = cl.cmdNumber;
    
    // Predict forward to current time
    while (cmdNum <= cl.cmdNumber) {
        cmd = &cl.cmds[cmdNum & CMD_MASK];
        
        if (cmd->serverTime > cl.snap.serverTime) {
            // Predict this command
            Pmove(&cl.pmove);
        }
        
        cmdNum++;
    }
    
    // Check for prediction errors
    delta = abs(cl.predicted_player_state.origin[0] - cl.snap.ps.origin[0]) +
            abs(cl.predicted_player_state.origin[1] - cl.snap.ps.origin[1]) +
            abs(cl.predicted_player_state.origin[2] - cl.snap.ps.origin[2]);
    
    if (delta > 640) {
        // Large prediction error - snap to server position
        cl.predicted_player_state = cl.snap.ps;
        cl.predicted_step_time = 0;
        cl.predicted_step = 0;
    } else if (delta > 4) {
        // Small error - smooth correction
        if (cl_showmiss->integer) {
            Com_Printf("prediction miss on %i: %i\n", cl.framecount, delta);
        }
        
        // Gradually correct position over several frames
        VectorSubtract(cl.snap.ps.origin, cl.predicted_player_state.origin, 
                      cl.predicted_error);
        cl.predicted_error_time = cls.realtime;
    }
    
    // Apply smoothing for small corrections
    if (cl.predicted_error_time) {
        float f = (cls.realtime - cl.predicted_error_time) / PREDICTION_FAST_ADJUST;
        
        if (f >= 1.0) {
            // Correction complete
            cl.predicted_error_time = 0;
            VectorClear(cl.predicted_error);
        } else {
            // Interpolate correction
            VectorScale(cl.predicted_error, 1.0 - f, cl.predicted_error);
            VectorAdd(cl.predicted_player_state.origin, cl.predicted_error,
                     cl.predicted_player_state.origin);
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Performance Monitoring</h2>
    
    <h3>Frame Time Analysis</h3>
    <div class="code-block">
        <pre><code>// Performance monitoring and optimization
typedef struct {
    int count;              // Number of samples
    float total;            // Total time
    float average;          // Average time
    float min, max;         // Min/max times
    int lastUpdate;         // Last update time
} perfCounter_t;

static perfCounter_t pc_server;
static perfCounter_t pc_client;
static perfCounter_t pc_renderer;
static perfCounter_t pc_sound;

void Com_PerformanceMonitoring(int timeBeforeFirstEvents, int timeBeforeServer,
                              int timeAfterServer, int timeBeforeClient, 
                              int timeAfterClient) {
    
    if (!com_speeds->integer) {
        return;
    }
    
    // Calculate subsystem times
    int serverTime = timeAfterServer - timeBeforeServer;
    int clientTime = timeAfterClient - timeBeforeClient;
    int totalTime = timeAfterClient - timeBeforeFirstEvents;
    
    // Update performance counters
    UpdatePerfCounter(&pc_server, serverTime);
    UpdatePerfCounter(&pc_client, clientTime);
    
    // Print detailed timing information
    if (com_speeds->integer >= 2) {
        Com_Printf("frame:%i total:%i sv:%i cl:%i\n",
                  com_frameMsec, totalTime, serverTime, clientTime);
    }
    
    // Print average times periodically
    static int lastReport = 0;
    if (com_speeds->integer >= 3 && 
        timeAfterClient - lastReport > 5000) {
        
        Com_Printf("Avg times - Server: %.1fms, Client: %.1fms\n",
                  pc_server.average, pc_client.average);
        lastReport = timeAfterClient;
    }
}

void UpdatePerfCounter(perfCounter_t* pc, float time) {
    pc->total += time;
    pc->count++;
    
    if (pc->count == 1) {
        pc->min = pc->max = time;
    } else {
        if (time < pc->min) pc->min = time;
        if (time > pc->max) pc->max = time;
    }
    
    pc->average = pc->total / pc->count;
    
    // Reset counters periodically
    if (pc->count > 1000) {
        pc->total *= 0.5f;
        pc->count /= 2;
    }
}

// Memory usage monitoring
void Com_Meminfo_f(void) {
    int hunkUsed = Hunk_MemoryRemaining();
    int zoneUsed = Z_MemoryUsage();
    
    Com_Printf("Memory usage:\n");
    Com_Printf("  Hunk: %d MB\n", hunkUsed / (1024 * 1024));
    Com_Printf("  Zone: %d KB\n", zoneUsed / 1024);
    Com_Printf("  Total: %d MB\n", (hunkUsed + zoneUsed) / (1024 * 1024));
}</code></pre>
    </div>
    
    <h3>Bottleneck Detection</h3>
    <div class="code-block">
        <pre><code>// Automated bottleneck detection and reporting
typedef struct {
    const char* name;
    float threshold;        // Warning threshold in milliseconds
    float criticalThreshold; // Critical threshold
    int warningCount;
    int criticalCount;
} bottleneckMonitor_t;

static bottleneckMonitor_t monitors[] = {
    {"Server", 8.0f, 16.0f, 0, 0},
    {"Client", 12.0f, 20.0f, 0, 0},
    {"Renderer", 10.0f, 16.0f, 0, 0},
    {"Sound", 2.0f, 5.0f, 0, 0},
    {"Network", 3.0f, 8.0f, 0, 0}
};

void Com_CheckBottlenecks(void) {
    static int lastCheck = 0;
    int currentTime = Sys_Milliseconds();
    
    // Check every second
    if (currentTime - lastCheck < 1000) {
        return;
    }
    
    for (int i = 0; i < ARRAY_LEN(monitors); i++) {
        bottleneckMonitor_t* monitor = &monitors[i];
        perfCounter_t* counter = GetPerfCounter(i);
        
        if (counter->average > monitor->criticalThreshold) {
            monitor->criticalCount++;
            if (monitor->criticalCount >= 3) {
                Com_Printf("^1CRITICAL: %s bottleneck detected (%.1fms avg)\n",
                          monitor->name, counter->average);
            }
        } else if (counter->average > monitor->threshold) {
            monitor->warningCount++;
            if (monitor->warningCount >= 5) {
                Com_Printf("^3WARNING: %s performance issue (%.1fms avg)\n",
                          monitor->name, counter->average);
            }
        } else {
            // Performance is good, reset counters
            monitor->warningCount = 0;
            monitor->criticalCount = 0;
        }
    }
    
    lastCheck = currentTime;
}

// Adaptive quality system
void Com_AdaptiveQuality(void) {
    static float targetFrameTime = 16.67f; // 60 FPS
    static int adaptationDelay = 0;
    
    if (pc_client.average > targetFrameTime * 1.2f) {
        // Performance is poor, reduce quality
        if (adaptationDelay-- <= 0) {
            if (r_picmip->integer < 3) {
                Cvar_SetValue("r_picmip", r_picmip->integer + 1);
                Com_Printf("Reduced texture quality for performance\n");
                adaptationDelay = 300; // 5 second delay
            }
        }
    } else if (pc_client.average < targetFrameTime * 0.8f) {
        // Performance is good, try to increase quality
        if (adaptationDelay-- <= 0) {
            if (r_picmip->integer > 0) {
                Cvar_SetValue("r_picmip", r_picmip->integer - 1);
                Com_Printf("Increased texture quality\n");
                adaptationDelay = 300;
            }
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Advanced Main Loop Patterns</h2>
    
    <h3>Fixed Timestep with Interpolation</h3>
    <div class="code-block">
        <pre><code>// Advanced timing for consistent physics simulation
#define FIXED_TIMESTEP 16  // 62.5 FPS for physics

static float accumulator = 0.0f;
static gamestate_t previousState;
static gamestate_t currentState;

void Com_AdvancedFrame(int msec) {
    float frameTime = msec / 1000.0f;
    float maxFrameTime = 0.25f; // Prevent spiral of death
    
    if (frameTime > maxFrameTime) {
        frameTime = maxFrameTime;
    }
    
    accumulator += frameTime;
    
    // Fixed timestep physics simulation
    while (accumulator >= FIXED_TIMESTEP) {
        // Save previous state for interpolation
        previousState = currentState;
        
        // Run fixed timestep simulation
        SV_FixedFrame(FIXED_TIMESTEP);
        Physics_Simulate(FIXED_TIMESTEP);
        
        accumulator -= FIXED_TIMESTEP;
    }
    
    // Calculate interpolation factor
    float alpha = accumulator / FIXED_TIMESTEP;
    
    // Interpolate state for rendering
    gamestate_t interpolatedState;
    InterpolateGameState(&previousState, &currentState, alpha, &interpolatedState);
    
    // Render with interpolated state
    CL_RenderFrame(&interpolatedState);
}

void InterpolateGameState(gamestate_t* prev, gamestate_t* current, 
                         float alpha, gamestate_t* result) {
    // Interpolate entity positions
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!prev->entities[i].active || !current->entities[i].active) {
            continue;
        }
        
        // Linear interpolation of position
        VectorLerp(prev->entities[i].origin, current->entities[i].origin, 
                   alpha, result->entities[i].origin);
        
        // Spherical interpolation of rotation
        QuatSlerp(prev->entities[i].rotation, current->entities[i].rotation,
                  alpha, result->entities[i].rotation);
    }
    
    // Interpolate camera
    VectorLerp(prev->camera.origin, current->camera.origin, 
               alpha, result->camera.origin);
    VectorLerp(prev->camera.angles, current->camera.angles,
               alpha, result->camera.angles);
}</code></pre>
    </div>
    
    <h3>Multi-threaded Frame Processing</h3>
    <div class="code-block">
        <pre><code>// Multi-threaded main loop for modern CPUs
#include <pthread.h>

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    qboolean active;
    qboolean workReady;
    void (*workFunction)(void);
} workerThread_t;

static workerThread_t renderThread;
static workerThread_t soundThread;
static workerThread_t physicsThread;

void Com_InitThreadedLoop(void) {
    // Initialize worker threads
    InitWorkerThread(&renderThread, R_ThreadedFrame);
    InitWorkerThread(&soundThread, S_ThreadedUpdate);
    InitWorkerThread(&physicsThread, Physics_ThreadedUpdate);
}

void Com_ThreadedFrame(int msec) {
    // Main thread handles game logic and coordination
    SV_Frame(msec);
    CL_LogicFrame(msec);
    
    // Signal worker threads
    SignalWorkerThread(&renderThread);
    SignalWorkerThread(&soundThread);
    SignalWorkerThread(&physicsThread);
    
    // Wait for all threads to complete
    WaitForWorkerThread(&renderThread);
    WaitForWorkerThread(&soundThread);
    WaitForWorkerThread(&physicsThread);
    
    // Present final frame
    R_SwapBuffers();
}

void InitWorkerThread(workerThread_t* worker, void (*workFunc)(void)) {
    worker->workFunction = workFunc;
    worker->active = qtrue;
    worker->workReady = qfalse;
    
    pthread_mutex_init(&worker->mutex, NULL);
    pthread_cond_init(&worker->condition, NULL);
    
    pthread_create(&worker->thread, NULL, WorkerThreadMain, worker);
}

void* WorkerThreadMain(void* arg) {
    workerThread_t* worker = (workerThread_t*)arg;
    
    while (worker->active) {
        pthread_mutex_lock(&worker->mutex);
        
        // Wait for work
        while (!worker->workReady && worker->active) {
            pthread_cond_wait(&worker->condition, &worker->mutex);
        }
        
        if (worker->active && worker->workReady) {
            // Execute work function
            worker->workFunction();
            worker->workReady = qfalse;
        }
        
        pthread_mutex_unlock(&worker->mutex);
    }
    
    return NULL;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/engine-subsystems">Engine Subsystems</a></li>
        <li><a href="core/memory-management">Memory Management</a></li>
        <li><a href="modernization/profiling-tools">Performance Profiling</a></li>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="networking/networking">Networking</a></li>
    </ul>
</div>