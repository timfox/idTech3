<?php
/**
 * Console System - id Tech 3 Engine Documentation
 */
$title = 'Console System - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/console-system' => 'Console System'
];
?>

<h1>Console System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 console system provides a powerful command-line interface for game configuration, debugging, and real-time control. It includes CVars (Console Variables) for configuration management and a comprehensive command system for game control.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>CVars:</strong> Persistent configuration variables with type safety</li>
            <li><strong>Commands:</strong> Runtime functions accessible via console</li>
            <li><strong>Scripting:</strong> Batch execution and configuration files</li>
            <li><strong>Remote Access:</strong> RCON for server administration</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Console Variables (CVars)</h2>
    
    <h3>CVar System Architecture</h3>
    <div class="code-block">
        <pre><code>// cvar.h - Console Variable system
typedef struct cvar_s {
    char*       name;           // Variable name
    char*       string;         // Current string value
    char*       resetString;    // Default value
    char*       latchedString;  // Value pending restart
    int         flags;          // CVAR flags
    qboolean    modified;       // Set when changed
    float       value;          // Floating point value
    int         integer;        // Integer value
    struct cvar_s* next;        // Linked list
    struct cvar_s* hashNext;    // Hash table
} cvar_t;

// CVar flags
#define CVAR_ARCHIVE        0x0001  // Save to config
#define CVAR_USERINFO       0x0002  // Send to server
#define CVAR_SERVERINFO     0x0004  // Server broadcasts
#define CVAR_SYSTEMINFO     0x0008  // System info
#define CVAR_INIT           0x0010  // Can't be changed after init
#define CVAR_LATCH          0x0020  // Latched until restart
#define CVAR_ROM            0x0040  // Read-only
#define CVAR_USER_CREATED   0x0080  // Created by user
#define CVAR_TEMP           0x0100  // Temporary variable
#define CVAR_CHEAT          0x0200  // Cheat protection
#define CVAR_NORESTART      0x0400  // Don't clear on restart</code></pre>
    </div>
    
    <h3>CVar Registration and Management</h3>
    <div class="code-block">
        <pre><code>// Registering CVars in the engine
void Cvar_RegisterVariables(void) {
    // Graphics CVars
    r_mode = Cvar_Get("r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH);
    r_fullscreen = Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_gamma = Cvar_Get("r_gamma", "1.0", CVAR_ARCHIVE);
    r_swapInterval = Cvar_Get("r_swapInterval", "0", CVAR_ARCHIVE);
    
    // Network CVars
    net_port = Cvar_Get("net_port", va("%i", PORT_SERVER), CVAR_LATCH);
    rate = Cvar_Get("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE);
    snaps = Cvar_Get("snaps", "40", CVAR_USERINFO | CVAR_ARCHIVE);
    
    // Game CVars
    g_gametype = Cvar_Get("g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH);
    fraglimit = Cvar_Get("fraglimit", "20", CVAR_SERVERINFO);
    timelimit = Cvar_Get("timelimit", "0", CVAR_SERVERINFO);
    
    // Cheat-protected CVars
    g_godmode = Cvar_Get("g_godmode", "0", CVAR_CHEAT);
    g_noclip = Cvar_Get("g_noclip", "0", CVAR_CHEAT);
    
    // System information CVars
    com_version = Cvar_Get("version", Q3_VERSION, CVAR_ROM | CVAR_SERVERINFO);
    com_protocol = Cvar_Get("protocol", va("%i", PROTOCOL_VERSION), CVAR_ROM);
}

// CVar access functions
cvar_t* Cvar_Get(const char* var_name, const char* value, int flags) {
    cvar_t* var = Cvar_FindVar(var_name);
    
    if (var) {
        // Update existing variable
        var->flags |= flags;
        if (!var->resetString) {
            var->resetString = CopyString(value);
        }
        return var;
    }
    
    // Create new variable
    var = Hunk_Alloc(sizeof(*var), h_dontcare);
    var->name = CopyString(var_name);
    var->string = CopyString(value);
    var->resetString = CopyString(value);
    var->value = atof(var->string);
    var->integer = atoi(var->string);
    var->flags = flags;
    var->modified = qtrue;
    
    // Add to hash table
    int hash = generateHashValue(var_name);
    var->hashNext = hashTable[hash];
    hashTable[hash] = var;
    
    // Add to linked list
    var->next = cvar_vars;
    cvar_vars = var;
    
    return var;
}

// Setting CVar values
void Cvar_Set(const char* var_name, const char* value) {
    cvar_t* var = Cvar_FindVar(var_name);
    
    if (!var) {
        // Create if it doesn't exist
        var = Cvar_Get(var_name, value, CVAR_USER_CREATED);
        return;
    }
    
    Cvar_Set2(var, value, qfalse);
}

void Cvar_Set2(cvar_t* var, const char* value, qboolean force) {
    if (!var) {
        return;
    }
    
    // Check if read-only
    if (var->flags & CVAR_ROM && !force) {
        Com_Printf("%s is read only.\n", var->name);
        return;
    }
    
    // Check if cheat-protected
    if (var->flags & CVAR_CHEAT && !Cvar_VariableIntegerValue("sv_cheats")) {
        Com_Printf("%s is cheat protected.\n", var->name);
        return;
    }
    
    // Check if already set to same value
    if (!strcmp(value, var->string)) {
        return;
    }
    
    // Handle latched variables
    if (var->flags & CVAR_LATCH) {
        if (var->latchedString) {
            if (!strcmp(value, var->latchedString)) {
                return;
            }
            Z_Free(var->latchedString);
        } else {
            if (!strcmp(value, var->string)) {
                return;
            }
        }
        
        var->latchedString = CopyString(value);
        var->modified = qtrue;
        
        Com_Printf("%s will be changed upon restarting.\n", var->name);
        return;
    }
    
    // Set the value
    if (var->string) {
        Z_Free(var->string);
    }
    
    var->string = CopyString(value);
    var->value = atof(var->string);
    var->integer = atoi(var->string);
    var->modified = qtrue;
    
    // Update user info if needed
    if (var->flags & CVAR_USERINFO) {
        CL_SetUserinfo();
    }
    
    // Update server info if needed
    if (var->flags & CVAR_SERVERINFO) {
        SV_SetConfigstring(CS_SERVERINFO, Cvar_InfoString(CVAR_SERVERINFO));
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Command System</h2>
    
    <h3>Command Registration</h3>
    <div class="code-block">
        <pre><code>// cmd.h - Command system structures
typedef struct cmd_function_s {
    struct cmd_function_s* next;
    char* name;
    xcommand_t function;
    completionFunc_t complete;
} cmd_function_t;

// Command registration
void Cmd_AddCommand(const char* cmd_name, xcommand_t function) {
    cmd_function_t* cmd;
    
    // Check if command already exists
    if (Cmd_FindCommand(cmd_name)) {
        Com_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
        return;
    }
    
    // Allocate new command
    cmd = S_Malloc(sizeof(cmd_function_t));
    cmd->name = CopyString(cmd_name);
    cmd->function = function;
    cmd->complete = NULL;
    cmd->next = cmd_functions;
    cmd_functions = cmd;
}

// Example command implementations
void Cmd_Map_f(void) {
    char* cmd = Cmd_Argv(0);
    char* map = Cmd_Argv(1);
    
    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <mapname>\n", cmd);
        return;
    }
    
    // Change to specified map
    Cbuf_AddText(va("gamemap \"%s\"\n", map));
}

void Cmd_Status_f(void) {
    int i;
    client_t* cl;
    playerState_t* ps;
    
    Com_Printf("map: %s\n", sv_mapname->string);
    Com_Printf("num score ping name            lastmsg address               qport rate\n");
    Com_Printf("--- ----- ---- --------------- ------- --------------------- ----- -----\n");
    
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (!cl->state) {
            continue;
        }
        
        ps = SV_GameClientNum(i);
        Com_Printf("%3i %5i %4i %-15s %7i %-21s %5i %5i\n",
                  i, ps->persistant[PERS_SCORE], cl->ping,
                  cl->name, svs.time - cl->lastPacketTime,
                  NET_AdrToString(cl->netchan.remoteAddress),
                  cl->netchan.qport, cl->rate);
    }
}

void Cmd_ServerInfo_f(void) {
    Com_Printf("Server info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_SERVERINFO));
}

// Command registration in engine initialization
void Cmd_RegisterCommands(void) {
    // Map commands
    Cmd_AddCommand("map", Cmd_Map_f);
    Cmd_AddCommand("devmap", Cmd_Devmap_f);
    Cmd_AddCommand("gamemap", SV_Map_f);
    
    // Server commands
    Cmd_AddCommand("status", Cmd_Status_f);
    Cmd_AddCommand("serverinfo", Cmd_ServerInfo_f);
    Cmd_AddCommand("heartbeat", SV_Heartbeat_f);
    
    // Client commands
    Cmd_AddCommand("connect", CL_Connect_f);
    Cmd_AddCommand("disconnect", CL_Disconnect_f);
    Cmd_AddCommand("record", CL_Record_f);
    Cmd_AddCommand("demo", CL_PlayDemo_f);
    
    // System commands
    Cmd_AddCommand("quit", Com_Quit_f);
    Cmd_AddCommand("exit", Com_Quit_f);
    Cmd_AddCommand("exec", Cmd_Exec_f);
    Cmd_AddCommand("echo", Cmd_Echo_f);
    
    // CVar commands
    Cmd_AddCommand("set", Cvar_Set_f);
    Cmd_AddCommand("seta", Cvar_SetA_f);
    Cmd_AddCommand("setu", Cvar_SetU_f);
    Cmd_AddCommand("sets", Cvar_SetS_f);
    Cmd_AddCommand("reset", Cvar_Reset_f);
    Cmd_AddCommand("toggle", Cvar_Toggle_f);
    Cmd_AddCommand("cycle", Cvar_Cycle_f);
    Cmd_AddCommand("cvarlist", Cvar_List_f);
    
    // Debug commands
    Cmd_AddCommand("cmdlist", Cmd_List_f);
    Cmd_AddCommand("meminfo", Z_MemInfo_f);
    Cmd_AddCommand("hunkmegs", Com_Meminfo_f);
}</code></pre>
    </div>
    
    <h3>Command Buffer System</h3>
    <div class="code-block">
        <pre><code>// Command buffer for queued execution
typedef struct {
    int     cursize;
    int     maxsize;
    byte*   data;
} cbuf_t;

static cbuf_t cmd_text;
static byte   cmd_text_buf[MAX_CMD_BUFFER];

void Cbuf_Init(void) {
    cmd_text.data = cmd_text_buf;
    cmd_text.maxsize = MAX_CMD_BUFFER;
    cmd_text.cursize = 0;
}

void Cbuf_AddText(const char* text) {
    int len = strlen(text);
    
    if (cmd_text.cursize + len >= cmd_text.maxsize) {
        Com_Printf("Cbuf_AddText: overflow\n");
        return;
    }
    
    memcpy(&cmd_text.data[cmd_text.cursize], text, len);
    cmd_text.cursize += len;
}

void Cbuf_Execute(void) {
    int     i;
    char*   text;
    char    line[MAX_CMD_LINE];
    int     quotes;
    
    while (cmd_text.cursize) {
        // Find a \n or ; line break
        text = (char*)cmd_text.data;
        quotes = 0;
        
        for (i = 0; i < cmd_text.cursize; i++) {
            if (text[i] == '"') {
                quotes++;
            }
            if (!(quotes & 1) && text[i] == ';') {
                break; // Don't break if inside a quoted string
            }
            if (text[i] == '\n' || text[i] == '\r') {
                break;
            }
        }
        
        if (i >= sizeof(line) - 1) {
            i = sizeof(line) - 1;
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
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Console Interface</h2>
    
    <h3>Console Input Handling</h3>
    <div class="code-block">
        <pre><code>// console.c - Console display and input
typedef struct {
    qboolean    initialized;
    short       text[CON_TEXTSIZE];
    int         current;        // Line currently being displayed
    int         x;              // Offset in current line for next print
    int         display;        // Bottom of console displays this line
    int         linewidth;      // Characters across screen
    int         totallines;     // Total lines in console scrollback
    
    float       displayFrac;    // Approaches finalFrac at scr_conspeed
    float       finalFrac;      // 0.0 to 1.0 lines of console to display
    int         fracTime;       // Time of last displayFrac update
    
    // The edit line
    field_t     field;
    int         historyLine;    // The line being displayed from history buffer
    field_t     historyBuffer[COMMAND_HISTORY];
    
    // Console completion
    field_t     completionField;
    char        completionString[MAX_TOKEN_CHARS];
    int         completionCount;
    
} console_t;

static console_t con;

void Con_Init(void) {
    int i;
    
    con.linewidth = -1;
    Con_CheckResize();
    
    // Initialize console field
    Field_Clear(&con.field);
    con.field.widthInChars = con.linewidth;
    
    // Initialize history
    for (i = 0; i < COMMAND_HISTORY; i++) {
        Field_Clear(&con.historyBuffer[i]);
        con.historyBuffer[i].widthInChars = con.linewidth;
    }
    
    Con_ClearNotify();
    con.initialized = qtrue;
}

void Con_KeyEvent(int key) {
    // Handle special keys
    if (key == K_ENTER || key == K_KP_ENTER) {
        // Execute command
        char* cmd = con.field.buffer;
        
        Com_Printf("]%s\n", cmd);
        
        // Copy line to history buffer
        Con_SaveField();
        
        // Execute the command
        if (strlen(cmd)) {
            Cbuf_AddText(cmd);
            Cbuf_AddText("\n");
        }
        
        Field_Clear(&con.field);
        con.field.widthInChars = con.linewidth;
        
        // Force console closed if we're in game
        if (cls.state == CA_ACTIVE) {
            Con_Close();
        }
        
        return;
    }
    
    // Handle completion
    if (key == K_TAB) {
        Con_CompleteCommand();
        return;
    }
    
    // Handle history navigation
    if (key == K_UPARROW || key == K_KP_UPARROW) {
        Con_HistoryGetPrev();
        return;
    }
    
    if (key == K_DOWNARROW || key == K_KP_DOWNARROW) {
        Con_HistoryGetNext();
        return;
    }
    
    // Handle page scrolling
    if (key == K_PGUP || key == K_KP_PGUP) {
        con.display -= 2;
        if (con.current - con.display >= con.totallines) {
            con.display = con.current - con.totallines + 1;
        }
        return;
    }
    
    if (key == K_PGDN || key == K_KP_PGDN) {
        con.display += 2;
        if (con.display > con.current) {
            con.display = con.current;
        }
        return;
    }
    
    // Regular character input
    Field_KeyDownEvent(&con.field, key);
}</code></pre>
    </div>
    
    <h3>Command Completion</h3>
    <div class="code-block">
        <pre><code>// Command and CVar auto-completion
void Con_CompleteCommand(void) {
    char* cmd;
    char* s;
    int completionArgument = 0;
    
    // Copy the current field to completionField
    Field_AutoComplete(&con.completionField, &con.field);
    completionArgument = 0;
    
    // Parse arguments
    cmd = Cmd_Argv(0);
    if (cmd[0] == '\\' || cmd[0] == '/') {
        cmd++;
    }
    
    // Check for argument completion
    for (s = con.completionField.buffer; *s; s++) {
        if (*s == ' ') {
            completionArgument++;
        }
    }
    
    // Complete command name
    if (completionArgument == 0) {
        Con_CompleteCommandName();
        return;
    }
    
    // Complete command arguments
    Con_CompleteArgument(cmd, completionArgument);
}

void Con_CompleteCommandName(void) {
    cmd_function_t* cmd;
    cvar_t* cvar;
    int matches = 0;
    char shortestMatch[MAX_TOKEN_CHARS];
    char* match;
    
    shortestMatch[0] = 0;
    
    // Check commands
    for (cmd = cmd_functions; cmd; cmd = cmd->next) {
        if (!Q_stricmpn(cmd->name, con.completionString, strlen(con.completionString))) {
            match = cmd->name;
            
            if (matches == 0) {
                Q_strncpyz(shortestMatch, match, sizeof(shortestMatch));
            } else {
                // Find common prefix
                int i;
                for (i = 0; shortestMatch[i] && match[i]; i++) {
                    if (tolower(shortestMatch[i]) != tolower(match[i])) {
                        shortestMatch[i] = 0;
                        break;
                    }
                }
                shortestMatch[i] = 0;
            }
            
            matches++;
            
            if (matches == 1) {
                Com_Printf("]%s\n", con.completionString);
            }
            
            Com_Printf("    %s\n", match);
        }
    }
    
    // Check CVars
    for (cvar = cvar_vars; cvar; cvar = cvar->next) {
        if (!Q_stricmpn(cvar->name, con.completionString, strlen(con.completionString))) {
            match = cvar->name;
            
            if (matches == 0) {
                Q_strncpyz(shortestMatch, match, sizeof(shortestMatch));
            } else {
                // Find common prefix
                int i;
                for (i = 0; shortestMatch[i] && match[i]; i++) {
                    if (tolower(shortestMatch[i]) != tolower(match[i])) {
                        shortestMatch[i] = 0;
                        break;
                    }
                }
                shortestMatch[i] = 0;
            }
            
            matches++;
            
            if (matches == 1) {
                Com_Printf("]%s\n", con.completionString);
            }
            
            Com_Printf("    %s ^7\"^3%s^7\"\n", match, cvar->string);
        }
    }
    
    if (matches > 1) {
        Com_Printf("%d possible matches.\n", matches);
        
        if (shortestMatch[0]) {
            Q_strncpyz(con.completionField.buffer, shortestMatch, sizeof(con.completionField.buffer));
            con.completionField.cursor = strlen(shortestMatch);
        }
    } else if (matches == 1) {
        Q_strncpyz(con.completionField.buffer, shortestMatch, sizeof(con.completionField.buffer));
        con.completionField.cursor = strlen(shortestMatch);
        Q_strcat(con.completionField.buffer, sizeof(con.completionField.buffer), " ");
        con.completionField.cursor++;
    }
    
    memcpy(&con.field, &con.completionField, sizeof(field_t));
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Configuration Files</h2>
    
    <h3>Config File System</h3>
    <div class="code-block">
        <pre><code>// Configuration file loading and saving
void Com_WriteConfiguration(void) {
    fileHandle_t f;
    cvar_t* var;
    char filename[MAX_QPATH];
    
    // Create config filename
    if (com_basegame->string[0]) {
        Com_sprintf(filename, sizeof(filename), "%s/q3config.cfg", com_basegame->string);
    } else {
        strcpy(filename, "q3config.cfg");
    }
    
    f = FS_FOpenFileWrite(filename);
    if (!f) {
        Com_Printf("Couldn't write %s.\n", filename);
        return;
    }
    
    FS_Printf(f, "// generated by quake, do not modify\n");
    
    // Write archived CVars
    for (var = cvar_vars; var; var = var->next) {
        if (var->flags & CVAR_ARCHIVE) {
            FS_Printf(f, "seta %s \"%s\"\n", var->name, var->string);
        }
    }
    
    // Write key bindings
    Key_WriteBindings(f);
    
    FS_FCloseFile(f);
}

void Com_ReadConfiguration(void) {
    // Execute default config
    Cbuf_AddText("exec default.cfg\n");
    Cbuf_Execute();
    
    // Execute user config
    Cbuf_AddText("exec q3config.cfg\n");
    
    // Execute autoexec
    Cbuf_AddText("exec autoexec.cfg\n");
    
    Cbuf_Execute();
}

// Specialized config commands
void Cmd_Exec_f(void) {
    char* f;
    int len;
    char filename[MAX_QPATH];
    
    if (Cmd_Argc() != 2) {
        Com_Printf("exec <filename> : execute a script file\n");
        return;
    }
    
    Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
    COM_DefaultExtension(filename, sizeof(filename), ".cfg");
    
    len = FS_ReadFile(filename, (void**)&f);
    if (!f) {
        Com_Printf("couldn't exec %s\n", filename);
        return;
    }
    
    Com_Printf("execing %s\n", filename);
    
    Cbuf_InsertText(f);
    
    FS_FreeFile(f);
}

// Autoexec and config search paths
void Com_InitConfiguration(void) {
    // Set default values for CVars that need them
    Cvar_Get("arch", ARCH_STRING, CVAR_ROM);
    Cvar_Get("username", Sys_GetCurrentUser(), CVAR_ROM);
    
    // Read configuration files
    Com_ReadConfiguration();
    
    // Set any command line CVars
    Com_StartupVariable(NULL);
    
    // Write config if any settings changed
    if (com_modified) {
        com_modified = qfalse;
        Com_WriteConfiguration();
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>RCON (Remote Console)</h2>
    
    <h3>Remote Administration</h3>
    <div class="code-block">
        <pre><code>// RCON system for remote server administration
void SVC_RemoteCommand(netadr_t from, msg_t* msg) {
    qboolean valid;
    char remaining[1024];
    char sv_outputbuf[SV_OUTPUTBUF_LENGTH];
    static unsigned int time = 0;
    static int hash = 0;
    int time_int;
    
    // Prevent using rcon as an amplifier and make dictionary attacks impractical
    if (Sys_Milliseconds() - time < 500) {
        return;
    }
    
    time = Sys_Milliseconds();
    
    if (!strlen(rconPassword->string) || strcmp(Cmd_Argv(1), rconPassword->string)) {
        valid = qfalse;
        Com_Printf("Bad rcon from %s:\n%s\n", NET_AdrToString(from), Cmd_Argv(2));
    } else {
        valid = qtrue;
        Com_Printf("Rcon from %s:\n%s\n", NET_AdrToString(from), Cmd_Argv(2));
    }
    
    // Start redirecting all print outputs to the packet
    svs.redirectAddress = from;
    Com_BeginRedirect(sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);
    
    if (!strlen(rconPassword->string)) {
        Com_Printf("No rconpassword set on the server.\n");
    } else if (!valid) {
        Com_Printf("Bad rconpassword.\n");
    } else {
        remaining[0] = 0;
        
        // Execute the command
        for (int i = 2; i < Cmd_Argc(); i++) {
            strcat(remaining, Cmd_Argv(i));
            strcat(remaining, " ");
        }
        
        Cmd_ExecuteString(remaining);
    }
    
    Com_EndRedirect();
}

// RCON packet handling
void SV_PacketEvent(netadr_t from, msg_t* msg) {
    int i;
    char* s;
    char* c;
    
    MSG_BeginReadingOOB(msg);
    i = MSG_ReadLong(msg);
    
    if (i == -1) {
        // Connectionless packet
        s = MSG_ReadStringLine(msg);
        
        Cmd_TokenizeString(s);
        c = Cmd_Argv(0);
        
        if (!Q_stricmp(c, "rcon")) {
            SVC_RemoteCommand(from, msg);
        } else if (!Q_stricmp(c, "connect")) {
            SVC_DirectConnect(from);
        } else if (!Q_stricmp(c, "getstatus")) {
            SVC_Status(from);
        } else if (!Q_stricmp(c, "getinfo")) {
            SVC_Info(from);
        } else {
            Com_DPrintf("Bad connectionless packet from %s:\n%s\n", 
                       NET_AdrToString(from), s);
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Essential Console Commands</h2>
    
    <h3>Common Commands Reference</h3>
    <div class="code-block">
        <pre><code>// Essential console commands for users

// ===== SYSTEM COMMANDS =====
quit                    // Exit the game
exit                    // Exit the game
exec <filename>         // Execute a configuration file
echo <text>             // Print text to console
clear                   // Clear console text
cmdlist [filter]        // List all commands
cvarlist [filter]       // List all CVars
toggle <cvar>           // Toggle a CVar between 0 and 1
cycle <cvar> <values>   // Cycle through CVar values
reset <cvar>            // Reset CVar to default value

// ===== CONNECTION COMMANDS =====
connect <address>       // Connect to server
disconnect              // Disconnect from server
reconnect               // Reconnect to last server
rcon <password> <cmd>   // Execute remote console command

// ===== GAME COMMANDS =====
map <mapname>           // Load a map (cheat mode)
devmap <mapname>        // Load a map in developer mode
gamemap <mapname>       // Change map on server
kick <player>           // Kick a player (server)
clientkick <clientnum>  // Kick by client number
say <message>           // Say message to all players
say_team <message>      // Say message to team
tell <player> <msg>     // Private message to player

// ===== DEMO COMMANDS =====
record <filename>       // Start recording demo
stop                    // Stop recording demo
demo <filename>         // Play back demo
nextdemo               // Skip to next demo in loop

// ===== DEBUG COMMANDS =====
status                  // Show server status
serverinfo              // Show server info CVars
userinfo                // Show user info CVars
meminfo                 // Show memory usage
developer <0|1>         // Enable developer mode
logfile <0|1|2>         // Enable console logging
condump <filename>      // Dump console to file

// ===== RENDERING COMMANDS =====
vid_restart             // Restart video system
r_speeds <0|1|2>        // Show rendering statistics
screenshot              // Take a screenshot
gfxinfo                 // Show graphics information

// ===== SOUND COMMANDS =====
s_restart               // Restart sound system
s_info                  // Show sound system info
soundinfo               // Alias for s_info

// ===== NETWORK COMMANDS =====
net_restart             // Restart network
ping                    // Show ping to server
net_stats               // Show network statistics

// ===== CHEAT COMMANDS (require sv_cheats 1) =====
god                     // Toggle god mode
noclip                  // Toggle noclip mode
notarget                // Toggle notarget mode
give <item>             // Give item to player
/kill                   // Suicide
teleport <x> <y> <z>    // Teleport to coordinates</code></pre>
    </div>
    
    <h3>Advanced Console Usage</h3>
    <div class="code-block">
        <pre><code>// Advanced console scripting examples

// Conditional execution
seta developer "1"
if $developer == 1 (
    echo "Developer mode enabled"
    logfile 2
    r_showcluster 1
)

// Looping and variables
for i in 1 2 3 4 5 (
    echo "Loading bot $i"
    addbot crash $i
)

// Key binding with complex commands
bind F1 "toggle r_showtris; toggle r_shownormals"
bind F2 "vstr nextdemo"
bind MOUSE1 "+attack; +button2"

// Configuration aliases
alias fastforward "timescale 2; echo Fast forward enabled"
alias normalspeed "timescale 1; echo Normal speed"
alias slowmotion "timescale 0.5; echo Slow motion enabled"

// Server rotation script
seta map1 "map q3dm1; set nextmap vstr map2"
seta map2 "map q3dm6; set nextmap vstr map3"  
seta map3 "map q3dm17; set nextmap vstr map1"
seta nextmap "vstr map1"

// Performance monitoring
alias showfps "cg_drawfps 1; r_speeds 1; echo FPS display enabled"
alias hidefps "cg_drawfps 0; r_speeds 0; echo FPS display disabled"

// Development shortcuts
alias quicktest "devmap q3dm1; god; noclip; give all"
alias resetgame "map_restart; echo Game reset"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/core/engine-subsystems">Engine Subsystems</a></li>
        <li><a href="/development/debugging">Debugging Tools</a></li>
        <li><a href="/development/scripting">Scripting Guide</a></li>
        <li><a href="/server/setup">Server Setup</a></li>
        <li><a href="/networking/networking">Networking System</a></li>
    </ul>
</div>