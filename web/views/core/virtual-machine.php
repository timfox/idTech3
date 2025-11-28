<?php
/**
 * Virtual Machine System - id Tech 3 Engine Documentation
 */
$title = 'Virtual Machine System - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/virtual-machine' => 'Virtual Machine'
];
?>

<h1>Quake Virtual Machine (QVM)</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The Quake Virtual Machine (QVM) is a custom bytecode virtual machine that enables secure execution of game logic modules in id Tech 3. It provides a sandboxed environment for mods while maintaining platform independence and security. The QVM system allows game logic to run in a controlled environment with limited access to system resources.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Sandboxed Execution:</strong> Isolated environment for mod code</li>
            <li><strong>Platform Independence:</strong> Same bytecode runs on all platforms</li>
            <li><strong>Security Model:</strong> Controlled syscall interface</li>
            <li><strong>Performance:</strong> Optimized bytecode interpreter</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>QVM Architecture</h2>
    
    <h3>Virtual Machine Components</h3>
    <div class="code-block">
        <pre><code>// vm.h - Virtual Machine core definitions
typedef struct vm_s {
    // System fields
    int             programStack;       // Stack pointer
    int             *instructionPointers; // Jump table  
    byte            *dataBase;          // Data segment base
    int             dataMask;           // Data segment mask
    
    // QVM specific
    int             instructionCount;   // Number of instructions
    byte            *codeBase;          // Code segment base
    int             codeLength;         // Code segment length
    
    // Runtime state
    intptr_t        *opStack;          // Operand stack
    int             opStackOfs;         // Stack offset
    int             stackOfs;          // Program stack offset
    
    // Module information
    char            name[MAX_QPATH];    // Module name (cgame, game, ui)
    vmIndex_t       index;              // Module index
    dllSyscall_t    syscall;           // Syscall interface
    
    // Security
    qboolean        forceDataMask;      // Force data masking
    int             dataMaskValue;      // Data mask value
    
    // Performance
    int             instructionsExecuted; // Instruction counter
    int             exactDataMask;      // Exact data masking
    
} vm_t;

// QVM instruction opcodes
typedef enum {
    OP_UNDEF,
    OP_IGNORE,
    OP_BREAK,
    OP_ENTER,       // Enter function
    OP_LEAVE,       // Leave function
    OP_CALL,        // Function call
    OP_PUSH,        // Push to stack
    OP_POP,         // Pop from stack
    OP_CONST,       // Load constant
    OP_LOCAL,       // Local variable access
    OP_JUMP,        // Unconditional jump
    
    // Conditional jumps
    OP_EQ,          // Jump if equal
    OP_NE,          // Jump if not equal
    OP_LTI,         // Jump if less than (signed)
    OP_LEI,         // Jump if less or equal (signed)
    OP_GTI,         // Jump if greater than (signed)
    OP_GEI,         // Jump if greater or equal (signed)
    OP_LTU,         // Jump if less than (unsigned)
    OP_LEU,         // Jump if less or equal (unsigned)
    OP_GTU,         // Jump if greater than (unsigned)
    OP_GEU,         // Jump if greater or equal (unsigned)
    OP_EQF,         // Jump if equal (float)
    OP_NEF,         // Jump if not equal (float)
    OP_LTF,         // Jump if less than (float)
    OP_LEF,         // Jump if less or equal (float)
    OP_GTF,         // Jump if greater than (float)
    OP_GEF,         // Jump if greater or equal (float)
    
    // Arithmetic
    OP_LOAD1,       // Load 1 byte
    OP_LOAD2,       // Load 2 bytes
    OP_LOAD4,       // Load 4 bytes
    OP_STORE1,      // Store 1 byte
    OP_STORE2,      // Store 2 bytes
    OP_STORE4,      // Store 4 bytes
    OP_ARG,         // Function argument
    OP_BLOCK_COPY,  // Memory block copy
    
    // Math operations
    OP_SEX8,        // Sign extend 8->32
    OP_SEX16,       // Sign extend 16->32
    OP_NEGI,        // Negate integer
    OP_ADD,         // Add
    OP_SUB,         // Subtract
    OP_DIVI,        // Divide (signed)
    OP_DIVU,        // Divide (unsigned)
    OP_MODI,        // Modulo (signed)
    OP_MODU,        // Modulo (unsigned)
    OP_MULI,        // Multiply (signed)
    OP_MULU,        // Multiply (unsigned)
    
    OP_BAND,        // Bitwise AND
    OP_BOR,         // Bitwise OR
    OP_BXOR,        // Bitwise XOR
    OP_BCOM,        // Bitwise complement
    
    OP_LSH,         // Left shift
    OP_RSHI,        // Right shift (signed)
    OP_RSHU,        // Right shift (unsigned)
    
    OP_NEGF,        // Negate float
    OP_ADDF,        // Add float
    OP_SUBF,        // Subtract float
    OP_DIVF,        // Divide float
    OP_MULF,        // Multiply float
    
    OP_CVIF,        // Convert int to float
    OP_CVFI,        // Convert float to int
    
    OP_MAX
} opcode_t;</code></pre>
    </div>
    
    <h3>QVM File Format</h3>
    <div class="code-block">
        <pre><code>// QVM file format structure
typedef struct {
    int     vmMagic;            // VM_MAGIC
    int     instructionCount;   // Number of instructions
    int     codeOffset;         // Offset to code section
    int     codeLength;         // Length of code section
    int     dataOffset;         // Offset to data section
    int     dataLength;         // Length of data section
    int     litLength;          // Length of literal pool
    int     bssLength;          // Length of BSS section
} vmHeader_t;

#define VM_MAGIC            0x12721444
#define VM_MAGIC_VER2       0x12721445

// QVM file loading
vm_t* VM_Create(vmIndex_t index, syscall_t systemCalls, vmInterpret_t interpret) {
    vm_t*           vm;
    vmHeader_t      header;
    int             length;
    int             dataLength;
    int             i;
    char            filename[MAX_QPATH];
    union {
        vmHeader_t  h;
        int         i[1024];
    } headerBuffer;
    
    if (!systemCalls) {
        Com_Error(ERR_FATAL, "VM_Create: bad parms");
    }
    
    // Clear the VM until we're done creating it
    vm = &vmTable[index];
    memset(vm, 0, sizeof(*vm));
    
    Com_sprintf(filename, sizeof(filename), "vm/%s.qvm", vmName[index]);
    
    Com_Printf("Loading vm file %s...\n", filename);
    
    length = FS_ReadFile(filename, (void**)&header);
    if (!header) {
        Com_Printf("Failed to load vm file %s\n", filename);
        return NULL;
    }
    
    // Byte swap header
    for (i = 0; i < sizeof(vmHeader_t) / 4; i++) {
        ((int*)&header)[i] = LittleLong(((int*)&header)[i]);
    }
    
    // Check magic number
    if (header.vmMagic == VM_MAGIC_VER2) {
        Com_Printf("...which has vmMagic VM_MAGIC_VER2\n");
        
        // Check instruction count
        if (header.instructionCount <= 0 || header.instructionCount >= MAX_QPATH) {
            Com_Error(ERR_DROP, "%s has bad instruction count", filename);
        }
        
        // Allocate space for the jump targets
        vm->instructionCount = header.instructionCount;
        vm->instructionPointers = Hunk_Alloc(vm->instructionCount * 4, h_high);
        
        // Copy or load the code
        vm->codeBase = Hunk_Alloc(header.codeLength, h_high);
        memcpy(vm->codeBase, (byte*)&header + header.codeOffset, header.codeLength);
        vm->codeLength = header.codeLength;
        
        // Allocate space for the data segment
        dataLength = header.dataLength + header.litLength + header.bssLength;
        vm->dataBase = Hunk_Alloc(dataLength, h_high);
        vm->dataMask = dataLength - 1;
        
        // Copy the data segment
        memcpy(vm->dataBase, (byte*)&header + header.dataOffset, header.dataLength + header.litLength);
        
        // Clear BSS section
        memset(vm->dataBase + header.dataLength + header.litLength, 0, header.bssLength);
        
        vm->syscall = systemCalls;
        return vm;
    }
    
    if (header.vmMagic != VM_MAGIC) {
        Com_Error(ERR_FATAL, "%s does not have a recognizable magic number", filename);
    }
    
    FS_FreeFile(header);
    return NULL;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>QVM Interpreter</h2>
    
    <h3>Bytecode Execution Engine</h3>
    <div class="code-block">
        <pre><code>// QVM interpreter main loop
int VM_CallInterpreted(vm_t* vm, int* args) {
    byte            stack[OPSTACK_SIZE + 15];
    int*            opStack;
    uint8_t         opStackOfs;
    int             programCounter;
    int             programStack;
    int             stackOnEntry;
    byte*           image;
    int*            codeImage;
    int             v1;
    int             dataMask;
    int             arg;
    
    // Interpret the code
    vm->currentlyInterpreting = qtrue;
    
    // Initialize stack
    programStack = vm->programStack;
    stackOnEntry = programStack;
    
    // Set up data segment
    image = vm->dataBase;
    dataMask = vm->dataMask;
    
    // Set up code segment  
    codeImage = (int*)vm->codeBase;
    
    // Initialize operand stack
    opStack = PADP(stack, 16);
    opStackOfs = 0;
    
    programCounter = 0;
    
    // Pass arguments
    for (arg = 0; arg < 8; arg++) {
        if (!args[arg]) {
            break;
        }
        opStack[opStackOfs] = args[arg];
        opStackOfs++;
    }
    
    while (1) {
        int opcode, r0, r1;
        
nextInstruction:
        r0 = opStack[opStackOfs];
        r1 = opStack[opStackOfs - 1];
        
        opcode = codeImage[programCounter++];
        
        switch (opcode) {
        case OP_BREAK:
            vm->breakCount++;
            goto nextInstruction;
            
        case OP_CONST:
            opStackOfs++;
            r1 = codeImage[programCounter];
            opStack[opStackOfs] = r1;
            programCounter++;
            goto nextInstruction;
            
        case OP_LOCAL:
            opStackOfs++;
            r1 = codeImage[programCounter];
            r1 = programStack + r1;
            opStack[opStackOfs] = r1;
            programCounter++;
            goto nextInstruction;
            
        case OP_LOAD4:
            r0 = opStack[opStackOfs] & dataMask;
            r0 = *(int*)&image[r0];
            opStack[opStackOfs] = r0;
            goto nextInstruction;
            
        case OP_LOAD2:
            r0 = opStack[opStackOfs] & dataMask;
            r0 = *(unsigned short*)&image[r0];
            opStack[opStackOfs] = r0;
            goto nextInstruction;
            
        case OP_LOAD1:
            r0 = opStack[opStackOfs] & dataMask;
            r0 = image[r0];
            opStack[opStackOfs] = r0;
            goto nextInstruction;
            
        case OP_STORE4:
            r1 = opStack[opStackOfs - 1] & dataMask;
            *(int*)&image[r1] = r0;
            opStackOfs -= 2;
            goto nextInstruction;
            
        case OP_STORE2:
            r1 = opStack[opStackOfs - 1] & dataMask;
            *(short*)&image[r1] = r0;
            opStackOfs -= 2;
            goto nextInstruction;
            
        case OP_STORE1:
            r1 = opStack[opStackOfs - 1] & dataMask;
            image[r1] = r0;
            opStackOfs -= 2;
            goto nextInstruction;
            
        case OP_ARG:
            r1 = codeImage[programCounter];
            r1 = programStack + r1;
            *(int*)&image[r1] = r0;
            opStackOfs--;
            programCounter++;
            goto nextInstruction;
            
        case OP_CALL:
            // Save state
            opStack[opStackOfs] = programCounter;
            
            // Jump to subroutine
            programCounter = r0;
            opStackOfs++;
            goto nextInstruction;
            
        case OP_PUSH:
            opStackOfs++;
            goto nextInstruction;
            
        case OP_POP:
            opStackOfs--;
            goto nextInstruction;
            
        case OP_ENTER:
            // Allocate space for local variables
            r1 = codeImage[programCounter];
            programCounter++;
            programStack -= r1;
            
            // Store return address and old frame pointer
            r1 = programStack + 4;
            *(int*)&image[r1 & dataMask] = programStack + r1;
            *(int*)&image[programStack & dataMask] = programCounter;
            goto nextInstruction;
            
        case OP_LEAVE:
            // Restore stack and return
            r1 = programStack;
            programStack = *(int*)&image[r1 & dataMask];
            programCounter = *(int*)&image[(r1 + 4) & dataMask];
            goto nextInstruction;
            
        // Arithmetic operations
        case OP_ADD:
            opStackOfs--;
            opStack[opStackOfs] = r1 + r0;
            goto nextInstruction;
            
        case OP_SUB:
            opStackOfs--;
            opStack[opStackOfs] = r1 - r0;
            goto nextInstruction;
            
        case OP_DIVI:
            opStackOfs--;
            opStack[opStackOfs] = r1 / r0;
            goto nextInstruction;
            
        case OP_DIVU:
            opStackOfs--;
            opStack[opStackOfs] = ((unsigned)r1) / ((unsigned)r0);
            goto nextInstruction;
            
        case OP_MODI:
            opStackOfs--;
            opStack[opStackOfs] = r1 % r0;
            goto nextInstruction;
            
        case OP_MODU:
            opStackOfs--;
            opStack[opStackOfs] = ((unsigned)r1) % ((unsigned)r0);
            goto nextInstruction;
            
        case OP_MULI:
            opStackOfs--;
            opStack[opStackOfs] = r1 * r0;
            goto nextInstruction;
            
        case OP_MULU:
            opStackOfs--;
            opStack[opStackOfs] = ((unsigned)r1) * ((unsigned)r0);
            goto nextInstruction;
            
        // Floating point operations
        case OP_ADDF:
            opStackOfs--;
            ((float*)opStack)[opStackOfs] = ((float*)opStack)[opStackOfs] + ((float*)opStack)[opStackOfs + 1];
            goto nextInstruction;
            
        case OP_SUBF:
            opStackOfs--;
            ((float*)opStack)[opStackOfs] = ((float*)opStack)[opStackOfs] - ((float*)opStack)[opStackOfs + 1];
            goto nextInstruction;
            
        case OP_DIVF:
            opStackOfs--;
            ((float*)opStack)[opStackOfs] = ((float*)opStack)[opStackOfs] / ((float*)opStack)[opStackOfs + 1];
            goto nextInstruction;
            
        case OP_MULF:
            opStackOfs--;
            ((float*)opStack)[opStackOfs] = ((float*)opStack)[opStackOfs] * ((float*)opStack)[opStackOfs + 1];
            goto nextInstruction;
            
        // System call
        case OP_SYSCALL:
            // Call into engine
            vm->programStack = programStack - 4;
            *(int*)&image[(programStack + 4) & dataMask] = programCounter;
            
            if (!vm->syscall) {
                Com_Error(ERR_FATAL, "VM_CallInterpreted: NULL vm->syscall");
            }
            
            {
                // Extract arguments
                intptr_t* args = (intptr_t*)&image[programStack + 4];
                r0 = vm->syscall(args);
            }
            
            opStack[opStackOfs] = r0;
            programCounter = *(int*)&image[(programStack + 4) & dataMask];
            goto nextInstruction;
            
        // Conditional jumps
        case OP_EQ:
            opStackOfs -= 2;
            if (r1 == r0) {
                programCounter = codeImage[programCounter];
            } else {
                programCounter++;
            }
            goto nextInstruction;
            
        case OP_NE:
            opStackOfs -= 2;
            if (r1 != r0) {
                programCounter = codeImage[programCounter];
            } else {
                programCounter++;
            }
            goto nextInstruction;
            
        // ... more conditional jumps
        
        default:
            Com_Error(ERR_DROP, "Bad VM instruction");
        }
    }
    
done:
    vm->currentlyInterpreting = qfalse;
    
    if (opStackOfs != 1) {
        Com_Error(ERR_DROP, "Interpreter error: opStack corrupted");
    }
    
    vm->programStack = stackOnEntry;
    
    return opStack[0];
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Game Module Integration</h2>
    
    <h3>Module Types and Interfaces</h3>
    <div class="code-block">
        <pre><code>// Game module system - three main modules
typedef enum {
    VM_GAME,        // Server-side game logic
    VM_CGAME,       // Client-side game presentation  
    VM_UI,          // User interface
    VM_COUNT
} vmIndex_t;

// Module syscall interfaces
typedef intptr_t (*syscall_t)(intptr_t arg, ...);

// Game module (server-side)
intptr_t SV_GameSystemCall(intptr_t* args) {
    switch (args[0]) {
    case G_PRINT:
        Com_Printf("%s", (char*)VMA(1));
        return 0;
        
    case G_ERROR:
        Com_Error(ERR_DROP, "%s", (char*)VMA(1));
        return 0;
        
    case G_MILLISECONDS:
        return Sys_Milliseconds();
        
    case G_CVAR_REGISTER:
        Cvar_Register((vmCvar_t*)VMA(1), (char*)VMA(2), (char*)VMA(3), args[4]);
        return 0;
        
    case G_CVAR_UPDATE:
        Cvar_Update((vmCvar_t*)VMA(1));
        return 0;
        
    case G_CVAR_SET:
        Cvar_Set((char*)VMA(1), (char*)VMA(2));
        return 0;
        
    case G_ARGC:
        return Cmd_Argc();
        
    case G_ARGV:
        Cmd_ArgvBuffer(args[1], (char*)VMA(2), args[3]);
        return 0;
        
    case G_SEND_CONSOLE_COMMAND:
        Cbuf_AddText((char*)VMA(1));
        return 0;
        
    case G_FS_FOPEN_FILE:
        return FS_FOpenFileByMode((char*)VMA(1), (fileHandle_t*)VMA(2), (fsMode_t)args[3]);
        
    case G_FS_READ:
        FS_Read(VMA(1), args[2], args[3]);
        return 0;
        
    case G_FS_WRITE:
        FS_Write(VMA(1), args[2], args[3]);
        return 0;
        
    case G_FS_FCLOSE_FILE:
        FS_FCloseFile(args[1]);
        return 0;
        
    case G_FS_GETFILELIST:
        return FS_GetFileList((char*)VMA(1), (char*)VMA(2), (char*)VMA(3), args[4]);
        
    // Entity management
    case G_GENTITY_FOR_NUM:
        return SV_GentityNum(args[1]);
        
    case G_NUM_FOR_GENTITY:
        return SV_NumForGentity((sharedEntity_t*)VMA(1));
        
    case G_SET_BRUSH_MODEL:
        SV_SetBrushModel((sharedEntity_t*)VMA(1), (char*)VMA(2));
        return 0;
        
    case G_TRACE:
        SV_Trace((trace_t*)VMA(1), (float*)VMA(2), (float*)VMA(3), (float*)VMA(4), 
                (float*)VMA(5), args[6], args[7]);
        return 0;
        
    case G_POINT_CONTENTS:
        return SV_PointContents((float*)VMA(1), args[2]);
        
    // Networking
    case G_SET_CONFIGSTRING:
        SV_SetConfigstring(args[1], (char*)VMA(2));
        return 0;
        
    case G_GET_CONFIGSTRING:
        SV_GetConfigstring(args[1], (char*)VMA(2), args[3]);
        return 0;
        
    case G_SET_USERINFO:
        SV_SetUserinfo(args[1], (char*)VMA(2));
        return 0;
        
    case G_GET_USERINFO:
        SV_GetUserinfo(args[1], (char*)VMA(2), args[3]);
        return 0;
        
    case G_GET_SERVERINFO:
        SV_GetServerinfo((char*)VMA(1), args[2]);
        return 0;
        
    case G_LOCATE_GAME_DATA:
        SV_LocateGameData((sharedEntity_t*)VMA(1), args[2], args[3], 
                         (playerState_t*)VMA(4), args[5]);
        return 0;
        
    case G_DROP_CLIENT:
        SV_GameDropClient(args[1], (char*)VMA(2));
        return 0;
        
    case G_SEND_SERVER_COMMAND:
        SV_GameSendServerCommand(args[1], (char*)VMA(2));
        return 0;
        
    case G_LINKENTITY:
        SV_LinkEntity((sharedEntity_t*)VMA(1));
        return 0;
        
    case G_UNLINKENTITY:
        SV_UnlinkEntity((sharedEntity_t*)VMA(1));
        return 0;
        
    case G_AREAS_CONNECTED:
        return CM_AreasConnected(args[1], args[2]);
        
    case G_ENTITIES_IN_BOX:
        return SV_AreaEntities((float*)VMA(1), (float*)VMA(2), (int*)VMA(3), args[4]);
        
    case G_ENTITY_CONTACT:
        return SV_EntityContact((float*)VMA(1), (float*)VMA(2), (sharedEntity_t*)VMA(3));
        
    case G_BOT_ALLOCATE_CLIENT:
        return SV_BotAllocateClient();
        
    case G_BOT_FREE_CLIENT:
        SV_BotFreeClient(args[1]);
        return 0;
        
    case G_GET_USERCMD:
        SV_GetUsercmd(args[1], (usercmd_t*)VMA(2));
        return 0;
        
    case G_GET_ENTITY_TOKEN:
        return SV_GetEntityToken((char*)VMA(1), args[2]);
        
    case G_DEBUG_POLYGON_CREATE:
        return BotImport_DebugPolygonCreate(args[1], args[2], (vec3_t*)VMA(3));
        
    case G_DEBUG_POLYGON_DELETE:
        BotImport_DebugPolygonDelete(args[1]);
        return 0;
        
    case G_REAL_TIME:
        return Com_RealTime((qtime_t*)VMA(1));
        
    case G_SNAPVECTOR:
        Sys_SnapVector((float*)VMA(1));
        return 0;
        
    default:
        Com_Error(ERR_DROP, "Bad game system trap: %ld", (long int)args[0]);
        return 0;
    }
}</code></pre>
    </div>
    
    <h3>Module Loading and Management</h3>
    <div class="code-block">
        <pre><code>// QVM module lifecycle management
void SV_InitGameVM(qboolean restart) {
    int i;
    
    // Start the entity parsing at the beginning
    sv.entityParsePoint = CM_EntityString();
    
    // Clear all gentity pointers that might still be set from
    // a previous level
    for (i = 0; i < sv_maxclients->integer; i++) {
        svs.clients[i].gentity = NULL;
    }
    
    // Use the current msec count for a random seed
    Cvar_Set("sv_serverid", va("%i", sv.serverId));
    
    // Initialize the game module
    gvm = VM_Create(VM_GAME, SV_GameSystemCall, VMI_NATIVE);
    if (!gvm) {
        Com_Error(ERR_DROP, "VM_Create on game failed");
    }
    
    // Initialize the game
    VM_Call(gvm, GAME_INIT, sv.time, Com_Milliseconds(), restart);
}

void CL_InitCGameVM(void) {
    const char* info;
    const char* mapname;
    int t1, t2;
    
    t1 = Sys_Milliseconds();
    
    // Put away the console
    Con_Close();
    
    // Find the current mapname
    info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
    mapname = Info_ValueForKey(info, "mapname");
    Com_sprintf(cl.mapname, sizeof(cl.mapname), "maps/%s.bsp", mapname);
    
    // Load the cgame module
    cgvm = VM_Create(VM_CGAME, CL_CgameSystemCall, cls.glconfig.hardwareType == GLHW_RAGEPRO ? VMI_NATIVE : VMI_COMPILED);
    
    if (!cgvm) {
        Com_Error(ERR_DROP, "VM_Create on cgame failed");
    }
    
    cls.state = CA_LOADING;
    
    // Initialize the CGame
    VM_Call(cgvm, CG_INIT, clc.serverMessageSequence, clc.serverCommandSequence, clc.clientNum);
    
    // We will send a usercmd this frame, which
    // will cause the server to send us the first snapshot
    cls.state = CA_PRIMED;
    
    t2 = Sys_Milliseconds();
    Com_Printf("CL_InitCGameVM: %5.2f seconds\n", (t2 - t1) / 1000.0);
    
    // Have the renderer touch all its images, so they are present
    // on the card even if the driver does deferred loading
    re.EndRegistration();
    
    // Make sure everything is paged in
    if (!Sys_LowPhysicalMemory()) {
        Com_TouchMemory();
    }
    
    // Clear anything that got printed
    Con_ClearNotify();
}

void UI_InitVM(void) {
    // Load the UI module
    uivm = VM_Create(VM_UI, UI_SystemCall, VMI_NATIVE);
    if (!uivm) {
        Com_Error(ERR_FATAL, "VM_Create on UI failed");
    }
    
    // Sanity check
    if (VM_Call(uivm, UI_GETAPIVERSION) != UI_API_VERSION) {
        Com_Error(ERR_DROP, "User Interface is version %d, expected %d",
                 VM_Call(uivm, UI_GETAPIVERSION), UI_API_VERSION);
        cls.uiStarted = qfalse;
    } else {
        // Initialize the UI
        VM_Call(uivm, UI_INIT);
        cls.uiStarted = qtrue;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>QVM Compilation Process</h2>
    
    <h3>From C Source to QVM Bytecode</h3>
    <div class="code-block">
        <pre><code>// QVM compilation pipeline using q3asm and q3lcc

/*
Step 1: Compile C source to intermediate .asm files using q3lcc
q3lcc is a modified version of lcc (little c compiler) that generates
QVM-specific assembly code instead of native assembly.

Example compilation:
q3lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g -I..\..\cgame -I..\..\game -I..\..\ui %1.c

Step 2: Assemble .asm files to .qvm using q3asm
q3asm reads the assembly files and generates the final QVM bytecode.

Example assembly:
q3asm -f ../cgame
*/

// Example .asm file structure generated by q3lcc
/*
export cg_main
code

align 4
LABELV cg_main
line 89
;89:intptr_t vmMain( int command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11 ) {
ADDRLP4 0
INDIRI4
CNSTI4 0
EQI4 $57
ADDRLP4 0
INDIRI4
CNSTI4 1
EQI4 $58
ADDRLP4 0
INDIRI4
CNSTI4 2
EQI4 $59
ADDRGP4 $56
JUMPV
LABELV $57
line 92
;92:		return CG_INIT( arg0, arg1, arg2 );
ADDRFP4 4
INDIRI4
ARGI4
ADDRFP4 8
INDIRI4
ARGI4
ADDRFP4 12
INDIRI4
ARGI4
ADDRGP4 CG_INIT
CALLI4
ASGNI4
ADDRLP4 4
INDIRI4
RETI4
ADDRGP4 $55
JUMPV
*/

// QVM build scripts for automated compilation
// Windows batch file (build.bat)
/*
@echo off
set cc=q3lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g -I..\..\cgame -I..\..\game -I..\..\ui

if "%1"=="" goto :eof

if not exist vm mkdir vm
cd vm

%cc% ../bg_misc.c
%cc% ../bg_pmove.c
%cc% ../bg_slidemove.c
%cc% ../bg_lib.c
%cc% ../q_math.c
%cc% ../q_shared.c

%cc% ../cg_main.c
%cc% ../cg_consolecmds.c
%cc% ../cg_draw.c
%cc% ../cg_drawtools.c
%cc% ../cg_effects.c
%cc% ../cg_ents.c
%cc% ../cg_event.c
%cc% ../cg_info.c
%cc% ../cg_localents.c
%cc% ../cg_marks.c
%cc% ../cg_players.c
%cc% ../cg_playerstate.c
%cc% ../cg_predict.c
%cc% ../cg_scoreboard.c
%cc% ../cg_servercmds.c
%cc% ../cg_snapshot.c
%cc% ../cg_view.c
%cc% ../cg_weapons.c

q3asm -f ../cgame
cd ..
*/

// Linux/Unix shell script (build.sh)
/*
#!/bin/bash
CC="q3lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g -I../../cgame -I../../game -I../../ui"

if [ $# -eq 0 ]; then
    exit 0
fi

mkdir -p vm
cd vm

$CC ../bg_misc.c
$CC ../bg_pmove.c
$CC ../bg_slidemove.c
$CC ../bg_lib.c
$CC ../q_math.c
$CC ../q_shared.c

$CC ../cg_main.c
$CC ../cg_consolecmds.c
$CC ../cg_draw.c
$CC ../cg_drawtools.c
$CC ../cg_effects.c
$CC ../cg_ents.c
$CC ../cg_event.c
$CC ../cg_info.c
$CC ../cg_localents.c
$CC ../cg_marks.c
$CC ../cg_players.c
$CC ../cg_playerstate.c
$CC ../cg_predict.c
$CC ../cg_scoreboard.c
$CC ../cg_servercmds.c
$CC ../cg_snapshot.c
$CC ../cg_view.c
$CC ../cg_weapons.c

q3asm -f ../cgame
cd ..
*/</code></pre>
    </div>
    
    <h3>QVM Assembly Language</h3>
    <div class="code-block">
        <pre><code>// QVM assembly language reference

/*
Basic instruction format:
OPCODE [operand]

Data types:
- CNSTI4: 32-bit signed integer constant
- CNSTF4: 32-bit float constant  
- CNSTU4: 32-bit unsigned integer constant
- ADDRGP4: Global address (32-bit)
- ADDRLP4: Local address (32-bit)
- INDIRI4: Indirect load 32-bit integer
- INDIRF4: Indirect load 32-bit float
- INDIRU4: Indirect load 32-bit unsigned
- ASGNF4: Assign 32-bit float
- ASGNI4: Assign 32-bit integer
- ASGNU4: Assign 32-bit unsigned
*/

// Example QVM assembly function
/*
export CG_DrawActiveFrame
code

align 4
LABELV CG_DrawActiveFrame
line 1425
;1425:void CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback ) {
ADDRLP4 0
ADDRFP4 0
INDIRI4
ASGNI4
ADDRLP4 4
ADDRFP4 4
INDIRI4
ASGNI4
ADDRLP4 8
ADDRFP4 8
INDIRI4
ASGNI4
line 1428
;1428:	cg.time = serverTime;
ADDRLP4 0
INDIRI4
ADDRGP4 cg+8
ASGNI4
line 1429
;1429:	cg.demoPlayback = demoPlayback;
ADDRLP4 8
INDIRI4
ADDRGP4 cg+16
ASGNI4
line 1432
;1432:	CG_RunLerpFrame( &cg_entities[0].lerpFrame );
ADDRGP4 cg_entities+304
ARGP4
ADDRGP4 CG_RunLerpFrame
CALLV
line 1435
;1435:	CG_DrawWorld();
ADDRGP4 CG_DrawWorld
CALLV
line 1438
;1438:	CG_DrawEntities();
ADDRGP4 CG_DrawEntities
CALLV
line 1441
;1441:	CG_DrawEffects();
ADDRGP4 CG_DrawEffects
CALLV
line 1444
;1444:	CG_DrawSnapshot();
ADDRGP4 CG_DrawSnapshot
CALLV
line 1447
;1447:}
LABELV $142
endproc CG_DrawActiveFrame 12 4
*/

// Common QVM assembly patterns
/*
Function call:
ADDRGP4 function_name
CALLI4                  // Call returning int
CALLV                   // Call returning void
CALLF                   // Call returning float

Conditional jump:
CNSTI4 0
EQI4 label             // Jump if equal to 0
NEI4 label             // Jump if not equal to 0
LTI4 label             // Jump if less than 0
GTI4 label             // Jump if greater than 0

Load/Store operations:
ADDRGP4 variable_name  // Load address of global variable
INDIRI4                // Load 32-bit int from address
ASGNI4                 // Store 32-bit int to address

Stack operations:
ADDRLP4 0              // Load local variable address
ARGP4                  // Push pointer argument
ARGI4                  // Push int argument
RETI4                  // Return int value
*/</code></pre>
    </div>
</div>

<div class="section">
    <h2>Security Model</h2>
    
    <h3>Sandboxing and Syscall Interface</h3>
    <div class="code-block">
        <pre><code>// QVM security features and limitations

// Memory protection through data masking
#define VM_Data(vm, addr) ((vm)->dataBase + ((addr) & (vm)->dataMask))

// Safe memory access macros
#define VMA(x) VM_ArgPtr(args[x])
#define VMF(x) ((float*)VMA(x))

void* VM_ArgPtr(intptr_t intValue) {
    if (!intValue) {
        return NULL;
    }
    
    // Check bounds
    if (currentVM->entryPoint) {
        if (intValue >= currentVM->dataLength) {
            Com_Error(ERR_DROP, "VM_ArgPtr: out of range");
        }
    }
    
    return (void*)(currentVM->dataBase + intValue);
}

// Syscall validation and filtering
intptr_t VM_DllSyscall(intptr_t arg, ...) {
    intptr_t args[16];
    va_list ap;
    
    args[0] = arg;
    
    va_start(ap, arg);
    for (int i = 1; i < ARRAY_LEN(args); i++) {
        args[i] = va_arg(ap, intptr_t);
    }
    va_end(ap);
    
    // Validate syscall number
    if (arg < 0 || arg >= MAX_SYSCALLS) {
        Com_Error(ERR_DROP, "VM_DllSyscall: invalid syscall %ld", (long)arg);
        return 0;
    }
    
    // Log dangerous syscalls in debug mode
    if (vm_debug->integer) {
        switch (arg) {
        case G_FS_FOPEN_FILE:
        case G_FS_WRITE:
        case G_SEND_CONSOLE_COMMAND:
            Com_DPrintf("VM syscall: %ld\n", (long)arg);
            break;
        }
    }
    
    return currentVM->syscall(args);
}

// File system restrictions for QVM
qboolean FS_CheckFilenameIsNotExecutable(const char* filename) {
    char* ext;
    
    ext = strrchr(filename, '.');
    if (!ext) {
        return qtrue;
    }
    
    // Block executable extensions
    if (!Q_stricmp(ext, ".exe") ||
        !Q_stricmp(ext, ".dll") ||
        !Q_stricmp(ext, ".so") ||
        !Q_stricmp(ext, ".dylib") ||
        !Q_stricmp(ext, ".bat") ||
        !Q_stricmp(ext, ".sh") ||
        !Q_stricmp(ext, ".com") ||
        !Q_stricmp(ext, ".scr")) {
        return qfalse;
    }
    
    return qtrue;
}

// Restricted file access for QVM modules
fileHandle_t FS_FOpenFileByModeVM(const char* qpath, fileHandle_t* f, fsMode_t mode) {
    // Only allow certain file operations for QVM
    switch (mode) {
    case FS_READ:
        // Allow reading most files
        break;
        
    case FS_WRITE:
    case FS_APPEND:
        // Only allow writing to specific directories
        if (Q_stricmpn(qpath, "demos/", 6) &&
            Q_stricmpn(qpath, "screenshots/", 12) &&
            Q_stricmpn(qpath, "profiles/", 9)) {
            Com_Printf("FS_FOpenFileByModeVM: write access denied to %s\n", qpath);
            *f = 0;
            return -1;
        }
        break;
        
    default:
        Com_Printf("FS_FOpenFileByModeVM: invalid mode %d\n", mode);
        *f = 0;
        return -1;
    }
    
    // Check filename for security
    if (!FS_CheckFilenameIsNotExecutable(qpath)) {
        Com_Printf("FS_FOpenFileByModeVM: executable file access denied: %s\n", qpath);
        *f = 0;
        return -1;
    }
    
    return FS_FOpenFileByMode(qpath, f, mode);
}</code></pre>
    </div>
    
    <h3>Performance and Security Trade-offs</h3>
    <div class="code-block">
        <pre><code>// QVM vs Native DLL comparison

/*
QVM Advantages:
- Platform independence (same bytecode on all platforms)
- Security through sandboxing
- No crashes from bad pointers (masked memory access)
- Version compatibility (no ABI issues)
- Easy distribution (single .qvm file)

QVM Disadvantages:  
- Performance overhead (interpreted execution)
- Limited memory access patterns
- No direct system calls
- Debugging complexity
- Larger file sizes vs native code
*/

// Performance optimization for QVM
void VM_Compile(vm_t* vm, vmHeader_t* header) {
    int op;
    int maxOp;
    int v;
    int i;
    
    // Compile to optimized instruction pointers
    maxOp = 0;
    for (i = 0; i < header->instructionCount; i++) {
        op = ((int*)vm->codeBase)[i];
        if (op < 0 || op >= OP_MAX) {
            Com_Error(ERR_DROP, "VM_Compile: bad opcode %i at instruction %i", op, i);
        }
        if (op > maxOp) {
            maxOp = op;
        }
    }
    
    // Build jump table for faster dispatch
    vm->instructionPointers = Hunk_Alloc(header->instructionCount * 4, h_high);
    
    for (i = 0; i < header->instructionCount; i++) {
        op = ((int*)vm->codeBase)[i];
        vm->instructionPointers[i] = op;
    }
    
    Com_Printf("VM file %s compiled to %i instructions\n", vm->name, header->instructionCount);
}

// JIT compilation option (advanced)
#ifdef VM_JIT_COMPILE
// Just-in-time compilation to native code for better performance
void VM_CompileJIT(vm_t* vm) {
    // This would compile QVM bytecode to native machine code
    // for much better performance while maintaining security
    // through controlled memory access and syscall filtering
    
    // Implementation would involve:
    // 1. Translate QVM opcodes to native instructions
    // 2. Maintain sandboxed memory model
    // 3. Insert bounds checks for memory access
    // 4. Hook syscalls through controlled interface
    
    Com_Printf("JIT compilation not implemented\n");
}
#endif

// Alternative: Native DLL loading with restrictions
vm_t* VM_CreateNative(vmIndex_t index, syscall_t systemCalls) {
    vm_t* vm;
    const char* dllName;
    char filename[MAX_QPATH];
    void* libHandle;
    
    // Determine DLL name based on platform
#ifdef _WIN32
    dllName = "dll";
#elif defined(__linux__)
    dllName = "so";  
#elif defined(__APPLE__)
    dllName = "dylib";
#else
    Com_Error(ERR_FATAL, "VM_CreateNative: unsupported platform");
#endif
    
    Com_sprintf(filename, sizeof(filename), "%s.%s", vmName[index], dllName);
    
    // Load native library
    libHandle = Sys_LoadLibrary(filename);
    if (!libHandle) {
        Com_Printf("VM_CreateNative: failed to load %s\n", filename);
        return NULL;
    }
    
    // Get required exports
    vm = Z_Malloc(sizeof(*vm));
    vm->dllHandle = libHandle;
    vm->entryPoint = Sys_GetProcAddress(libHandle, "vmMain");
    
    if (!vm->entryPoint) {
        Com_Printf("VM_CreateNative: %s does not have vmMain export\n", filename);
        Sys_UnloadLibrary(libHandle);
        Z_Free(vm);
        return NULL;
    }
    
    vm->syscall = systemCalls;
    
    Com_Printf("VM_CreateNative: loaded %s\n", filename);
    return vm;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>QVM Debugging</h2>
    
    <h3>Debug Information and Tools</h3>
    <div class="code-block">
        <pre><code>// QVM debugging support

// Debug builds include line number information
typedef struct {
    int instruction;        // Instruction number
    int line;              // Source line number
    char* filename;        // Source filename
} debugInfo_t;

// QVM stack trace for crashes
void VM_StackTrace(vm_t* vm, int programCounter, int opStackOfs) {
    int i;
    int* codeImage;
    
    if (!vm || !vm->codeBase) {
        return;
    }
    
    codeImage = (int*)vm->codeBase;
    
    Com_Printf("VM stack trace:\n");
    Com_Printf("Program counter: %d\n", programCounter);
    Com_Printf("Operand stack offset: %d\n", opStackOfs);
    
    // Print nearby instructions
    for (i = Q_max(0, programCounter - 5); i < Q_min(vm->instructionCount, programCounter + 5); i++) {
        if (i == programCounter) {
            Com_Printf(">>> %4d: %s\n", i, VM_OpcodeToString(codeImage[i]));
        } else {
            Com_Printf("    %4d: %s\n", i, VM_OpcodeToString(codeImage[i]));
        }
    }
}

const char* VM_OpcodeToString(int opcode) {
    switch (opcode) {
    case OP_UNDEF: return "OP_UNDEF";
    case OP_IGNORE: return "OP_IGNORE";
    case OP_BREAK: return "OP_BREAK";
    case OP_ENTER: return "OP_ENTER";
    case OP_LEAVE: return "OP_LEAVE";
    case OP_CALL: return "OP_CALL";
    case OP_PUSH: return "OP_PUSH";
    case OP_POP: return "OP_POP";
    case OP_CONST: return "OP_CONST";
    case OP_LOCAL: return "OP_LOCAL";
    case OP_JUMP: return "OP_JUMP";
    case OP_EQ: return "OP_EQ";
    case OP_NE: return "OP_NE";
    case OP_LTI: return "OP_LTI";
    case OP_LEI: return "OP_LEI";
    case OP_GTI: return "OP_GTI";
    case OP_GEI: return "OP_GEI";
    case OP_LTU: return "OP_LTU";
    case OP_LEU: return "OP_LEU";
    case OP_GTU: return "OP_GTU";
    case OP_GEU: return "OP_GEU";
    case OP_EQF: return "OP_EQF";
    case OP_NEF: return "OP_NEF";
    case OP_LTF: return "OP_LTF";
    case OP_LEF: return "OP_LEF";
    case OP_GTF: return "OP_GTF";
    case OP_GEF: return "OP_GEF";
    case OP_LOAD1: return "OP_LOAD1";
    case OP_LOAD2: return "OP_LOAD2";
    case OP_LOAD4: return "OP_LOAD4";
    case OP_STORE1: return "OP_STORE1";
    case OP_STORE2: return "OP_STORE2";
    case OP_STORE4: return "OP_STORE4";
    case OP_ARG: return "OP_ARG";
    case OP_BLOCK_COPY: return "OP_BLOCK_COPY";
    case OP_SEX8: return "OP_SEX8";
    case OP_SEX16: return "OP_SEX16";
    case OP_NEGI: return "OP_NEGI";
    case OP_ADD: return "OP_ADD";
    case OP_SUB: return "OP_SUB";
    case OP_DIVI: return "OP_DIVI";
    case OP_DIVU: return "OP_DIVU";
    case OP_MODI: return "OP_MODI";
    case OP_MODU: return "OP_MODU";
    case OP_MULI: return "OP_MULI";
    case OP_MULU: return "OP_MULU";
    case OP_BAND: return "OP_BAND";
    case OP_BOR: return "OP_BOR";
    case OP_BXOR: return "OP_BXOR";
    case OP_BCOM: return "OP_BCOM";
    case OP_LSH: return "OP_LSH";
    case OP_RSHI: return "OP_RSHI";
    case OP_RSHU: return "OP_RSHU";
    case OP_NEGF: return "OP_NEGF";
    case OP_ADDF: return "OP_ADDF";
    case OP_SUBF: return "OP_SUBF";
    case OP_DIVF: return "OP_DIVF";
    case OP_MULF: return "OP_MULF";
    case OP_CVIF: return "OP_CVIF";
    case OP_CVFI: return "OP_CVFI";
    default: return "UNKNOWN";
    }
}

// QVM profiling
typedef struct {
    int instruction;
    int count;
    int cycles;
} vmProfile_t;

static vmProfile_t vmProfiles[OP_MAX];

void VM_Profile(int opcode) {
    if (opcode >= 0 && opcode < OP_MAX) {
        vmProfiles[opcode].count++;
        vmProfiles[opcode].cycles += Sys_GetProcessorCycles();
    }
}

void VM_PrintProfile(void) {
    int i;
    int totalInstructions = 0;
    
    Com_Printf("QVM Instruction Profile:\n");
    Com_Printf("%-15s %8s %12s\n", "Instruction", "Count", "Cycles");
    Com_Printf("----------------------------------------\n");
    
    for (i = 0; i < OP_MAX; i++) {
        if (vmProfiles[i].count > 0) {
            Com_Printf("%-15s %8d %12d\n", 
                      VM_OpcodeToString(i), 
                      vmProfiles[i].count,
                      vmProfiles[i].cycles);
            totalInstructions += vmProfiles[i].count;
        }
    }
    
    Com_Printf("----------------------------------------\n");
    Com_Printf("Total instructions: %d\n", totalInstructions);
}

// Console commands for QVM debugging
void VM_Debug_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: vm_debug <module>\n");
        Com_Printf("Modules: game, cgame, ui\n");
        return;
    }
    
    const char* module = Cmd_Argv(1);
    vm_t* vm = NULL;
    
    if (!Q_stricmp(module, "game")) {
        vm = gvm;
    } else if (!Q_stricmp(module, "cgame")) {
        vm = cgvm;
    } else if (!Q_stricmp(module, "ui")) {
        vm = uivm;
    } else {
        Com_Printf("Unknown module: %s\n", module);
        return;
    }
    
    if (!vm) {
        Com_Printf("Module %s not loaded\n", module);
        return;
    }
    
    Com_Printf("VM Debug Info for %s:\n", module);
    Com_Printf("Instructions: %d\n", vm->instructionCount);
    Com_Printf("Code length: %d bytes\n", vm->codeLength);
    Com_Printf("Data mask: 0x%08x\n", vm->dataMask);
    Com_Printf("Program stack: %d\n", vm->programStack);
    Com_Printf("Stack offset: %d\n", vm->stackOfs);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/core/engine-subsystems">Engine Subsystems</a></li>
        <li><a href="/core/memory-management">Memory Management</a></li>
        <li><a href="/development/modding">Modding Guide</a></li>
        <li><a href="/development/scripting">Scripting</a></li>
        <li><a href="/platform/cross-platform">Cross-Platform Development</a></li>
    </ul>
</div>