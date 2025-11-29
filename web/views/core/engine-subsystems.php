<?php
/**
 * Engine Subsystems - id Tech 3 Architecture
 */
$title = 'Engine Subsystems - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/engine-subsystems' => 'Engine Subsystems'
];
?>

<h1>Engine Subsystems Architecture</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine is built around a modular subsystem architecture where major components operate semi-independently while communicating through well-defined interfaces. Understanding these subsystems and their interactions is crucial for engine development and optimization.</p>
    
    <div class="feature-list">
        <h3>Core Subsystems</h3>
        <ul>
            <li><strong>Common:</strong> Shared utilities, memory management, and core services</li>
            <li><strong>Renderer:</strong> Graphics pipeline, culling, and visual effects</li>
            <li><strong>Sound:</strong> Audio mixing, 3D positioning, and codec support</li>
            <li><strong>Input:</strong> Keyboard, mouse, and gamepad handling</li>
            <li><strong>Networking:</strong> Client-server communication and protocol handling</li>
            <li><strong>Game Logic:</strong> Entity management and gameplay systems</li>
        </ul>
    </div>
    
    <div class="feature-list">
        <h3>Recent Enhancements</h3>
        <p>The engine has undergone significant refactoring and modernization:</p>
        <ul>
            <li><strong>Memory Safety:</strong> AddressSanitizer, UndefinedBehaviorSanitizer, and comprehensive memory tracking</li>
            <li><strong>Structured Logging:</strong> Modern logging system with levels, categories, and JSON output</li>
            <li><strong>Enhanced Networking:</strong> HTTP/2, connection pooling, rate limiting, and WebSocket support</li>
            <li><strong>Code Quality:</strong> Fixed unsafe string operations, added header guards, improved error handling</li>
            <li><strong>Build System:</strong> Static analysis support, unit test framework, modern C++ features</li>
        </ul>
        <p>See <a href="core/memory-safety">Memory Safety & Profiling</a>, <a href="core/structured-logging">Structured Logging</a>, and <a href="networking/networking">Networking</a> for details.</p>
    </div>
</div>

<div class="section">
    <h2>Subsystem Architecture Overview</h2>
    
    <h3>Initialization Order and Dependencies</h3>
    <div class="code-block">
        <pre><code>// Engine startup sequence in Com_Init()
void Com_Init(char *commandLine) {
    // 1. Core systems first
    Cvar_Init();           // Configuration variables
    Cmd_Init();            // Console commands
    FS_InitFilesystem();   // File system
    
    // 2. Platform layer
    Sys_Init();            // Platform-specific initialization
    
    // 3. Network foundation
    NET_Init();            // Network subsystem
    
    // 4. Initialize major subsystems
    CL_Init();             // Client subsystem
    SV_Init();             // Server subsystem
    
    // 5. Renderer initialization (deferred until needed)
    // R_Init() called when creating window/context
    
    // 6. Sound system
    S_Init();              // Audio subsystem
    
    // 7. Input system
    IN_Init();             // Input handling
    
    Com_Printf("Engine initialization complete\n");
}</code></pre>
    </div>
    
    <h3>Subsystem Communication Patterns</h3>
    <div class="code-block">
        <pre><code>// Common communication mechanisms between subsystems

// 1. Direct function calls (tight coupling)
void CL_Frame(void) {
    // Client directly calls renderer
    R_RenderScene(&cl.refdef);
    
    // Client directly calls sound
    S_Update();
    
    // Client directly calls input
    IN_Frame();
}

// 2. Event-driven communication (loose coupling)
typedef enum {
    EV_NONE,
    EV_ENTITY_SPAWN,
    EV_SOUND_PLAY,
    EV_RENDERER_RESTART
} eventType_t;

typedef struct {
    eventType_t type;
    int time;
    int parm1, parm2;
    void* data;
} event_t;

// Event queue for subsystem communication
static event_t eventQueue[MAX_EVENTS];
static int eventHead, eventTail;

void Event_Post(eventType_t type, int parm1, int parm2, void* data) {
    event_t* event = &eventQueue[eventHead];
    event->type = type;
    event->time = Sys_Milliseconds();
    event->parm1 = parm1;
    event->parm2 = parm2;
    event->data = data;
    
    eventHead = (eventHead + 1) % MAX_EVENTS;
}

// 3. Shared state through global structures
extern clientStatic_t cls;    // Client state
extern serverStatic_t svs;    // Server state
extern refdef_t refdef;       // Renderer state
extern glconfig_t glConfig;   // Graphics configuration</code></pre>
    </div>
</div>

<div class="section">
    <h2>Common Subsystem</h2>
    
    <h3>Core Services and Utilities</h3>
    <div class="code-block">
        <pre><code>// qcommon.h - Common subsystem interface
// Provides fundamental services to all other subsystems

// Console variable system
typedef struct cvar_s {
    char* name;
    char* string;
    char* resetString;
    char* latchedString;
    int flags;
    float value;
    int integer;
    struct cvar_s* next;
    struct cvar_s* hashNext;
} cvar_t;

// Global variables accessible to all subsystems
extern cvar_t* com_speeds;        // Performance monitoring
extern cvar_t* com_developer;     // Debug mode
extern cvar_t* com_timescale;     // Time scaling
extern cvar_t* com_fixedtime;     // Fixed timestep
extern cvar_t* com_maxfps;        // Frame rate limiting

// Command system for inter-subsystem communication
typedef void (*xcommand_t)(void);

typedef struct cmd_function_s {
    struct cmd_function_s* next;
    char* name;
    xcommand_t function;
} cmd_function_t;

// File system abstraction
typedef enum {
    FS_READ,
    FS_WRITE,
    FS_APPEND,
    FS_APPEND_SYNC
} fsMode_t;

typedef struct {
    FILE* o;
    FILE* z;
    qboolean zipFile;
    char name[MAX_ZPATH];
} fileInPack_t;</code></pre>
    </div>
    
    <h3>Message System</h3>
    <div class="code-block">
        <pre><code>// Message buffer system for network and save game serialization
typedef struct {
    qboolean allowoverflow;  // Allow buffer overflow
    qboolean overflowed;     // Set when buffer overflows
    byte* data;              // Buffer data
    int maxsize;             // Maximum buffer size
    int cursize;             // Current buffer size
    int readcount;           // Read position
    int bit;                 // Bit position for bit packing
} msg_t;

// Message writing functions used by all subsystems
void MSG_Init(msg_t* buf, byte* data, int length);
void MSG_WriteChar(msg_t* sb, int c);
void MSG_WriteByte(msg_t* sb, int c);
void MSG_WriteShort(msg_t* sb, int c);
void MSG_WriteLong(msg_t* sb, int c);
void MSG_WriteFloat(msg_t* sb, float f);
void MSG_WriteString(msg_t* sb, const char* s);
void MSG_WriteBits(msg_t* msg, int value, int bits);

// Delta compression for entity states
void MSG_WriteDeltaEntity(msg_t* msg, struct entityState_s* from, 
                         struct entityState_s* to, qboolean force);

// Reading functions
void MSG_BeginReading(msg_t* sb);
int MSG_ReadChar(msg_t* sb);
int MSG_ReadByte(msg_t* sb);
int MSG_ReadShort(msg_t* sb);
int MSG_ReadLong(msg_t* sb);
float MSG_ReadFloat(msg_t* sb);
char* MSG_ReadString(msg_t* sb);
int MSG_ReadBits(msg_t* msg, int bits);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Renderer Subsystem</h2>
    
    <h3>Renderer Interface and State</h3>
    <div class="code-block">
        <pre><code>// tr_public.h - Renderer public interface
// Clean separation between engine and renderer

typedef struct {
    refEntityType_t reType;
    int renderfx;
    
    qhandle_t hModel;          // opaque type outside refresh
    
    // Most recent data
    vec3_t lightingOrigin;     // So multi-part models can be lit identically
    float shadowPlane;         // Projection on shadowPlane
    
    vec3_t axis[3];            // Rotation vectors
    vec3_t origin;             // Position
    
    int frame;                 // Model animation frame
    int oldframe;              // Previous frame for interpolation
    float backlerp;            // Interpolation factor
    
    int skinNum;               // Model skin number
    int customSkin;            // Custom skin handle
    byte shaderRGBA[4];        // Color and alpha modulation
    
    float shaderTexCoord[2];   // Texture coordinate offset
    float shaderTime;          // Shader time offset
} refEntity_t;

// Renderer interface functions called by client
typedef struct {
    void (*Shutdown)(qboolean destroyWindow);
    void (*BeginRegistration)(glconfig_t* glconfig);
    qhandle_t (*RegisterModel)(const char* name);
    qhandle_t (*RegisterSkin)(const char* name);
    qhandle_t (*RegisterShader)(const char* name);
    qhandle_t (*RegisterShaderNoMip)(const char* name);
    void (*LoadWorld)(const char* name);
    void (*SetWorldVisData)(const byte* vis);
    void (*EndRegistration)(void);
    void (*ClearScene)(void);
    void (*AddRefEntityToScene)(const refEntity_t* re);
    void (*AddPolyToScene)(qhandle_t hShader, int numVerts, const polyVert_t* verts);
    void (*AddLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
    void (*RenderScene)(const refdef_t* fd);
    void (*SetColor)(const float* rgba);
    void (*DrawStretchPic)(float x, float y, float w, float h, 
                          float s1, float t1, float s2, float t2, qhandle_t hShader);
    void (*DrawStretchRaw)(int x, int y, int w, int h, int cols, int rows, 
                          const byte* data, int client, qboolean dirty);
    void (*UploadCinematic)(int w, int h, int cols, int rows, 
                           const byte* data, int client, qboolean dirty);
    void (*BeginFrame)(stereoFrame_t stereoFrame);
    void (*EndFrame)(int* frontEndMsec, int* backEndMsec);
    int (*MarkFragments)(int numPoints, const vec3_t* points, const vec3_t projection,
                        int maxPoints, vec3_t pointBuffer, int maxFragments, 
                        markFragment_t* fragmentBuffer);
} refexport_t;</code></pre>
    </div>
    
    <h3>Renderer Initialization and Context</h3>
    <div class="code-block">
        <pre><code>// Renderer subsystem initialization
refexport_t* GetRefAPI(int apiVersion, refimport_t* rimp) {
    static refexport_t re;
    
    // Verify API version compatibility
    if (apiVersion != REF_API_VERSION) {
        ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", 
                 REF_API_VERSION, apiVersion);
        return NULL;
    }
    
    // Store engine import functions
    ri = *rimp;
    
    // Export renderer functions to engine
    re.Shutdown = RE_Shutdown;
    re.BeginRegistration = RE_BeginRegistration;
    re.RegisterModel = RE_RegisterModel;
    re.RegisterSkin = RE_RegisterSkin;
    re.RegisterShader = RE_RegisterShader;
    re.LoadWorld = RE_LoadWorld;
    re.SetWorldVisData = RE_SetWorldVisData;
    re.EndRegistration = RE_EndRegistration;
    re.BeginFrame = RE_BeginFrame;
    re.EndFrame = RE_EndFrame;
    re.ClearScene = R_ClearScene;
    re.AddRefEntityToScene = RE_AddRefEntityToScene;
    re.RenderScene = RE_RenderScene;
    re.SetColor = RE_SetColor;
    re.DrawStretchPic = RE_StretchPic;
    re.DrawStretchRaw = RE_StretchRaw;
    re.UploadCinematic = RE_UploadCinematic;
    re.MarkFragments = R_MarkFragments;
    
    return &re;
}

// Graphics configuration shared between engine and renderer
typedef struct {
    char renderer_string[MAX_STRING_CHARS];
    char vendor_string[MAX_STRING_CHARS];
    char version_string[MAX_STRING_CHARS];
    char extensions_string[MAX_STRING_CHARS];
    
    int maxTextureSize;        // Queried from GL
    int maxActiveTextures;     // Multitexture support
    
    int colorBits, depthBits, stencilBits;
    
    qboolean deviceSupportsGamma;
    textureCompression_t textureCompression;
    qboolean textureEnvAddAvailable;
    
    int vidWidth, vidHeight;   // Screen resolution
    float windowAspect;        // Aspect ratio
    
    qboolean isFullscreen;
    qboolean stereoEnabled;
    qboolean smpActive;        // SMP renderer active
} glconfig_t;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Sound Subsystem</h2>
    
    <h3>Audio System Architecture</h3>
    <div class="code-block">
        <pre><code>// s_public.h - Sound subsystem interface

typedef struct {
    sfxHandle_t (*RegisterSound)(const char* sample, qboolean compressed);
    void (*StartLocalSound)(sfxHandle_t sfx, int channelNum);
    void (*StartSound)(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
    void (*StartBackgroundTrack)(const char* intro, const char* loop);
    void (*StopBackgroundTrack)(void);
    void (*RawSamples)(int stream, int samples, int rate, int width, 
                      int channels, const byte* data, float volume);
    void (*StopAllSounds)(void);
    void (*ClearLoopingSounds)(qboolean killall);
    void (*AddLoopingSound)(int entityNum, const vec3_t origin, 
                           const vec3_t velocity, sfxHandle_t sfx);
    void (*AddRealLoopingSound)(int entityNum, const vec3_t origin, 
                               const vec3_t velocity, sfxHandle_t sfx);
    void (*Update)(void);
    void (*DisableSounds)(void);
    void (*BeginRegistration)(void);
    void (*EndRegistration)(void);
} soundInterface_t;

// Sound channel management
#define MAX_CHANNELS 96

typedef struct {
    int allocTime;
    int startSample;    // START_SAMPLE_IMMEDIATE = set immediately on next mix
    int entnum;         // To allow overriding a specific sound
    int entchannel;     // To allow overriding a specific sound
    int leftvol;        // 0-255 volume after spatialization
    int rightvol;       // 0-255 volume after spatialization
    int master_vol;     // 0-255 master volume
    float dopplerScale;
    float oldDopplerScale;
    vec3_t origin;      // Only use if fixed_origin is set
    qboolean fixed_origin; // Use origin instead of fetching entnum's origin
    sfx_t* thesfx;      // sfx structure
    qboolean doppler;
} channel_t;

// 3D Audio spatialization
void S_SpatializeOrigin(vec3_t origin, int master_vol, int* left_vol, int* right_vol) {
    vec3_t listener_origin;
    vec3_t source_vec;
    float dist;
    float scale;
    
    // Get listener position from client
    VectorCopy(listener_origin, origin);
    VectorSubtract(origin, listener_origin, source_vec);
    
    dist = VectorNormalize(source_vec);
    dist -= SOUND_FULLVOLUME;
    
    if (dist < 0) {
        dist = 0; // Close enough to be at full volume
    }
    
    dist *= SOUND_ATTENUATE;
    scale = 1.0 - dist;
    
    if (scale < 0) {
        scale = 0;
    }
    
    // Calculate stereo separation based on angle
    float dot = DotProduct(listener_right, source_vec);
    
    *right_vol = (int)(master_vol * scale * (1.0 + dot));
    *left_vol = (int)(master_vol * scale * (1.0 - dot));
    
    if (*left_vol < 0) *left_vol = 0;
    if (*right_vol < 0) *right_vol = 0;
    if (*left_vol > 255) *left_vol = 255;
    if (*right_vol > 255) *right_vol = 255;
}</code></pre>
    </div>
    
    <h3>Audio Format and Codec Support</h3>
    <div class="code-block">
        <pre><code>// Audio format handling and codec integration
typedef struct {
    int rate;           // Sample rate (22050, 44100, etc.)
    int width;          // 1 = 8-bit, 2 = 16-bit
    int channels;       // 1 = mono, 2 = stereo
    int samples;        // Total samples (not frames)
    int dataofs;        // Chunk starts here
    int datalen;        // Chunk length
    byte data[1];       // Variable sized
} wavinfo_t;

// Sound format loading
typedef struct sfx_s {
    char soundName[MAX_QPATH];
    int soundLength;
    soundData_t soundData;
    qboolean defaultSound; // Couldn't be loaded, use default
    qboolean inMemory;     // Not streamable
    qboolean soundCompressed; // Not streamable
} sfx_t;

// Codec registration and loading
void S_CodecInit(void) {
    S_CodecRegister(&wav_codec);
    S_CodecRegister(&ogg_codec);
    
#ifdef USE_CODEC_MP3
    S_CodecRegister(&mp3_codec);
#endif

#ifdef USE_CODEC_OPUS
    S_CodecRegister(&opus_codec);
#endif
}

// Streaming audio for music and large sounds
typedef struct sndStream_s {
    struct sndStream_s* next;
    int file;
    snd_codec_t* codec;
    char filename[MAX_QPATH];
    int length;         // Stream length
    int pos;            // Current position
    void* ptr;          // Codec-specific data
} sndStream_t;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Input Subsystem</h2>
    
    <h3>Input Event Processing</h3>
    <div class="code-block">
        <pre><code>// Input subsystem handles all user input devices
// Abstracts platform-specific input into common events

// Input event types
typedef enum {
    SE_NONE = 0,    // No event
    SE_KEY,         // Keyboard key
    SE_CHAR,        // Character input
    SE_MOUSE,       // Mouse movement
    SE_JOYSTICK_AXIS, // Joystick/gamepad axis
    SE_CONSOLE      // Console input
} sysEventType_t;

typedef struct {
    sysEventType_t evType;
    int evValue;
    int evValue2;
    int evPtrLength;    // Bytes of data pointed to by evPtr
    void* evPtr;        // Used for SE_CONSOLE
    int evTime;
} sysEvent_t;

// Input processing in main loop
void Com_EventLoop(void) {
    sysEvent_t ev;
    
    while (1) {
        ev = Com_GetEvent();
        
        // Handle quit events immediately
        if (ev.evType == SE_NONE) {
            break;
        }
        
        switch (ev.evType) {
        case SE_KEY:
            CL_KeyEvent(ev.evValue, ev.evValue2, ev.evTime);
            break;
        case SE_CHAR:
            CL_CharEvent(ev.evValue);
            break;
        case SE_MOUSE:
            CL_MouseEvent(ev.evValue, ev.evValue2, ev.evTime);
            break;
        case SE_JOYSTICK_AXIS:
            CL_JoystickEvent(ev.evValue, ev.evValue2, ev.evTime);
            break;
        case SE_CONSOLE:
            Cbuf_AddText((char*)ev.evPtr);
            Cbuf_AddText("\n");
            break;
        }
    }
}

// Key binding and command execution
typedef struct {
    qboolean down;
    int repeats;        // If > 1, it's a repeat
    char* binding;
} qkey_t;

extern qkey_t keys[MAX_KEYS];

void Key_Event(int key, qboolean down, int time) {
    char* kb;
    char cmd[1024];
    
    // Update key state
    keys[key].down = down;
    
    if (down) {
        keys[key].repeats++;
        if (keys[key].repeats == 1) {
            // First press
        } else {
            // Key repeat
            if (key != K_BACKSPACE && key != K_PAUSE && key != K_PGUP && 
                key != K_KP_PGUP && key != K_PGDN && key != K_KP_PGDN) {
                return; // Ignore repeats for most keys
            }
        }
    } else {
        keys[key].repeats = 0;
    }
    
    // Get key binding
    kb = keys[key].binding;
    if (kb) {
        if (kb[0] == '+') {
            // Button command
            Com_sprintf(cmd, sizeof(cmd), "%c%s %d %d\n", 
                       (down) ? '+' : '-', kb + 1, key, time);
            Cbuf_AddText(cmd);
        } else if (down) {
            // Normal command, only execute on key down
            Cbuf_AddText(kb);
            Cbuf_AddText("\n");
        }
    }
}</code></pre>
    </div>
    
    <h3>Mouse and Joystick Handling</h3>
    <div class="code-block">
        <pre><code>// Mouse input processing with sensitivity and acceleration
void IN_MouseMove(usercmd_t* cmd) {
    float mx, my;
    
    // Get raw mouse movement
    mx = mouse_x * m_sensitivity->value;
    my = mouse_y * m_sensitivity->value;
    
    // Apply mouse acceleration
    if (m_accel->value) {
        float accelSensitivity;
        float rate;
        
        rate = sqrt(mx * mx + my * my) / (float)frame_msec;
        accelSensitivity = m_sensitivity->value + rate * m_accel->value;
        
        mx *= accelSensitivity;
        my *= accelSensitivity;
    }
    
    // Apply mouse filter for smoothing
    if (m_filter->value) {
        mx = (mx + old_mouse_x) * 0.5;
        my = (my + old_mouse_y) * 0.5;
        old_mouse_x = mx;
        old_mouse_y = my;
    }
    
    // Convert to view angles
    if (mx) {
        cl.viewangles[YAW] -= m_yaw->value * mx;
    }
    
    if (my) {
        if (m_pitch->value < 0) {
            cl.viewangles[PITCH] += m_pitch->value * my;
        } else {
            cl.viewangles[PITCH] -= m_pitch->value * my;
        }
    }
    
    // Convert view angles to movement commands
    CL_ClampPitch();
    CL_KeyMove(cmd);
}

// Joystick/gamepad support
typedef struct {
    qboolean avail;
    int buttons;
    int oldbuttons;
    float axis[MAX_JOYSTICK_AXIS];
    char name[128];
} joystick_t;

void IN_JoyMove(usercmd_t* cmd) {
    float speed, aspeed;
    float fAxisValue;
    
    // Get movement speed
    speed = cl_anglespeedkey->value;
    if (in_run.active) {
        speed *= cl_movespeedkey->value;
    }
    
    aspeed = speed * cls.frametime;
    
    // Process each axis
    for (int i = 0; i < MAX_JOYSTICK_AXIS; i++) {
        fAxisValue = joy.axis[i];
        
        // Apply deadzone
        if (fabs(fAxisValue) < joy_threshold->value) {
            fAxisValue = 0.0;
        }
        
        switch (i) {
        case JOY_AXIS_X: // Move side to side
            cmd->rightmove = ClampChar(cmd->rightmove + fAxisValue * cl_sidespeed->value);
            break;
        case JOY_AXIS_Y: // Move forward and back
            cmd->forwardmove = ClampChar(cmd->forwardmove + fAxisValue * cl_forwardspeed->value);
            break;
        case JOY_AXIS_Z: // Look up and down
            cl.viewangles[PITCH] += fAxisValue * aspeed * cl_pitchspeed->value;
            break;
        case JOY_AXIS_R: // Turn left and right
            cl.viewangles[YAW] += fAxisValue * aspeed * cl_yawspeed->value;
            break;
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Networking Subsystem</h2>
    
    <h3>Network Architecture</h3>
    <div class="code-block">
        <pre><code>// net.h - Network subsystem interface
// Handles both client-server and peer-to-peer communication

typedef enum {
    NA_LOOPBACK,
    NA_BROADCAST,
    NA_IP,
    NA_IPX,
    NA_BROADCAST_IPX
} netadrtype_t;

typedef struct {
    netadrtype_t type;
    byte ip[4];
    byte ipx[10];
    unsigned short port;
} netadr_t;

// Network message types
typedef enum {
    NS_CLIENT,      // Client to server
    NS_SERVER       // Server to client
} netsrc_t;

// Packet structure
typedef struct {
    netadr_t adr;           // Sender address
    int cursize;            // Message size
    byte data[MAX_MSGLEN];  // Message data
} netmsg_t;

// Reliable packet transmission
typedef struct {
    qboolean reliable;      // Set if transmission is reliable
    netadr_t addr;          // Address of remote end
    int dropped;            // Between last packet and previous
    int last_received;      // For delta compression
    int last_sent;          // For delta compression
    netchan_t netchan;      // Network channel state
} netadr_t;

// Network channel for reliable transmission
typedef struct {
    netsrc_t sock;
    int dropped;            // Packets dropped before this one
    netadr_t remote_address;
    int qport;              // QPort value to write when transmitting
    
    // Sequencing variables
    int incoming_sequence;
    int incoming_acknowledged;
    int incoming_reliable_acknowledged;
    int incoming_reliable_sequence;
    int outgoing_sequence;
    int reliable_sequence;  // Single bit
    int last_reliable_sequence; // Sequence number of last reliable message
    
    // Reliable staging and holding areas
    sizebuf_t message;      // Writing buffer to send to server
    byte message_buf[MAX_MSGLEN];
    
    int reliable_length;
    byte reliable_buf[MAX_MSGLEN]; // Unacked reliable message
} netchan_t;</code></pre>
    </div>
    
    <h3>Client-Server Communication</h3>
    <div class="code-block">
        <pre><code>// Client-server message protocol
void CL_SendCmd(void) {
    usercmd_t* cmd, *oldcmd;
    usercmd_t nullcmd;
    usercmd_t cmds[CMD_BACKUP];
    int i;
    
    // Build command for this frame
    CL_CreateNewCommands();
    CL_CreateCmd();
    
    // Send reliable commands
    if (clc.reliableSequence - clc.reliableAcknowledge >= MAX_RELIABLE_COMMANDS) {
        // Don't send anything if we have too many unacknowledged reliables
        return;
    }
    
    // Write packet header
    MSG_WriteByte(&buf, clc_move);
    MSG_WriteLong(&buf, clc.reliableSequence);
    MSG_WriteLong(&buf, clc.serverMessageSequence);
    
    // Write user commands
    oldcmd = &nullcmd;
    for (i = 0; i < count; i++) {
        cmd = &cmds[i];
        MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd);
        oldcmd = cmd;
    }
    
    // Send the datagram
    NET_SendPacket(NS_CLIENT, buf.cursize, buf.data, clc.serverAddress);
}

// Server processes client commands
void SV_ExecuteClientMessage(client_t* cl, msg_t* msg) {
    int serverId, messageAcknowledge;
    int qport;
    int reliableAcknowledge;
    usercmd_t nullcmd;
    usercmd_t cmds[MAX_PACKET_USERCMDS];
    int numCmds;
    
    MSG_BeginReadingOOB(msg);
    MSG_ReadLong(msg); // Sequence number
    
    serverId = MSG_ReadLong(msg);
    messageAcknowledge = MSG_ReadLong(msg);
    reliableAcknowledge = MSG_ReadLong(msg);
    
    if (serverId != sv.serverId) {
        // Client is connecting to wrong server
        return;
    }
    
    // Read user commands
    memset(&nullcmd, 0, sizeof(nullcmd));
    for (numCmds = 0; numCmds < MAX_PACKET_USERCMDS; numCmds++) {
        if (MSG_ReadBits(msg, 1) == 0) {
            break; // No more commands
        }
        
        usercmd_t* cmd = &cmds[numCmds];
        MSG_ReadDeltaUsercmd(msg, &nullcmd, cmd);
        
        if (cmd->serverTime > cmds[numCmds-1].serverTime) {
            cmds[numCmds-1] = *cmd; // Time went backwards
        }
    }
    
    // Execute commands
    for (int i = 0; i < numCmds; i++) {
        SV_ClientThink(cl, &cmds[i]);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Subsystem Coordination</h2>
    
    <h3>Timing and Synchronization</h3>
    <div class="code-block">
        <pre><code>// Frame timing coordination between subsystems
static int com_frameTime;
static int com_frameMsec;

void Com_Frame(void) {
    int timeBeforeFirstEvents;
    int timeBeforeServer;
    int timeBeforeEvents;
    int timeBeforeClient;
    int timeAfter;
    
    // Frame timing
    timeBeforeFirstEvents = Sys_Milliseconds();
    
    // Handle console commands and events
    Com_EventLoop();
    
    // Frame rate limiting
    if (com_maxfps->integer > 0) {
        int minMsec = 1000 / com_maxfps->integer;
        
        while (1) {
            timeBeforeFirstEvents = Sys_Milliseconds();
            if (timeBeforeFirstEvents - com_frameTime >= minMsec) {
                break;
            }
            // Sleep briefly to avoid busy waiting
            Sys_Sleep(1);
        }
    }
    
    com_frameTime = timeBeforeFirstEvents;
    com_frameMsec = com_frameTime - lastTime;
    
    // Clamp frame time for stability
    if (com_frameMsec < 1) {
        com_frameMsec = 1;
    }
    if (com_frameMsec > 200) {
        com_frameMsec = 200;
    }
    
    // Run server frame
    timeBeforeServer = Sys_Milliseconds();
    SV_Frame(com_frameMsec);
    timeAfterServer = Sys_Milliseconds();
    
    // Run client frame
    timeBeforeClient = Sys_Milliseconds();
    CL_Frame(com_frameMsec);
    timeAfterClient = Sys_Milliseconds();
    
    // Performance monitoring
    if (com_speeds->integer) {
        Com_Printf("frame:%i all:%i sv:%i cl:%i\n",
                  com_frameMsec,
                  timeAfterClient - timeBeforeFirstEvents,
                  timeAfterServer - timeBeforeServer,
                  timeAfterClient - timeBeforeClient);
    }
    
    lastTime = com_frameTime;
}

// Subsystem frame functions coordinate their own timing
void CL_Frame(int msec) {
    static int extratime = 0;
    static int lastFrame = 0;
    int frameMsec;
    
    extratime += msec;
    
    if (!cl_timedemo->integer) {
        if (cls.state == CA_CONNECTED && extratime < 100) {
            return; // Don't flood server with packets
        }
    }
    
    frameMsec = extratime;
    extratime = 0;
    
    // Update subsystems
    S_Update();     // 3D audio
    CL_UpdateExplosions(); // Visual effects
    CL_UpdateDLights();    // Dynamic lights
    CL_UpdateLocalEntities(); // Local entities
    
    // Generate user commands
    CL_SendCmd();
    
    // Receive and process server messages
    CL_ReadPackets();
    
    // Predict player movement
    CL_PredictMovement();
    
    // Update screen
    SCR_UpdateScreen();
}</code></pre>
    </div>
    
    <h3>Error Handling and Recovery</h3>
    <div class="code-block">
        <pre><code>// Subsystem error handling and graceful degradation
typedef enum {
    ERR_FATAL,          // Quit game
    ERR_DROP,           // Drop to console
    ERR_SERVERDISCONNECT, // Disconnect from server
    ERR_DISCONNECT      // Disconnect and drop to main menu
} errorParm_t;

void Com_Error(int code, const char* fmt, ...) {
    va_list argptr;
    static int lastErrorTime;
    static int errorCount;
    int currentTime;
    static qboolean calledSystemError = qfalse;
    
    if (calledSystemError) {
        exit(-1); // Prevent recursive errors
    }
    
    calledSystemError = qtrue;
    
    // Format error message
    va_start(argptr, fmt);
    Q_vsnprintf(com_errorMessage, sizeof(com_errorMessage), fmt, argptr);
    va_end(argptr);
    
    // Error frequency checking
    currentTime = Sys_Milliseconds();
    if (currentTime - lastErrorTime < 100) {
        if (++errorCount > 3) {
            Sys_Error("Recursive error after: %s", com_errorMessage);
        }
    } else {
        errorCount = 0;
    }
    lastErrorTime = currentTime;
    
    // Handle different error types
    switch (code) {
    case ERR_DISCONNECT:
        CL_Disconnect(qtrue);
        CL_FlushMemory();
        calledSystemError = qfalse;
        throw 0; // Use setjmp/longjmp for error recovery
        break;
    case ERR_DROP:
        Com_Printf("********************\nERROR: %s\n********************\n", 
                   com_errorMessage);
        SV_Shutdown("Server crashed");
        CL_Disconnect(qtrue);
        CL_FlushMemory();
        calledSystemError = qfalse;
        throw 0;
        break;
    case ERR_SERVERDISCONNECT:
        SV_Shutdown("Server disconnected");
        CL_Disconnect(qtrue);
        CL_FlushMemory();
        calledSystemError = qfalse;
        throw 0;
        break;
    default:
    case ERR_FATAL:
        Sys_Error("%s", com_errorMessage);
        break;
    }
}

// Subsystem cleanup on shutdown
void Com_Shutdown(void) {
    if (logfile) {
        FS_FCloseFile(logfile);
        logfile = 0;
    }
    
    if (com_journalFile) {
        FS_FCloseFile(com_journalFile);
        com_journalFile = 0;
    }
    
    // Shutdown subsystems in reverse order
    CL_Shutdown();
    SV_Shutdown("Server quit");
    S_Shutdown();
    NET_Shutdown();
    FS_Shutdown(qtrue);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/main-loop">Main Loop Analysis</a></li>
        <li><a href="core/memory-management">Memory Management</a></li>
        <li><a href="core/memory-safety">Memory Safety & Profiling</a></li>
        <li><a href="core/structured-logging">Structured Logging</a></li>
        <li><a href="core/entity-system">Entity System</a></li>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="networking/networking">Networking</a></li>
    </ul>
</div>