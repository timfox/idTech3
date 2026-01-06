/*
=============================================================================
Backwards Compatibility Implementation

Enhanced legacy mode system with automatic detection for maintaining
compatibility with existing Quake 3 mods and content.
=============================================================================
*/

// Stub implementations for standalone testing
#ifdef UNIT_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifndef qboolean
#define qboolean int
#endif
#ifndef qtrue
#define qtrue 1
#endif
#ifndef qfalse
#define qfalse 0
#endif

#ifndef cvar_t
typedef struct {
    char string[256];
} cvar_t;
#endif

#ifndef msg_t
typedef struct msg_s {
    byte *data;
    int maxsize;
    int cursize;
    int readcount;
    int bit;
} msg_t;
#endif

static void Com_Printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static void Com_DPrintf(const char *fmt, ...) {
    // Debug printf - do nothing in tests
}

static cvar_t* Cvar_Get(const char *name, const char *default_value, int flags) {
    static cvar_t cvar;
    strcpy(cvar.string, default_value);
    return &cvar;
}

static int Sys_Milliseconds(void) {
    return 0;
}

static void Q_strncpyz(char *dest, const char *src, int destsize) {
    int len = strlen(src);
    if (len >= destsize) {
        len = destsize - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static int Q_stricmp(const char *s1, const char *s2) {
    return strcasecmp(s1, s2);
}

static int FS_FOpenFileRead(const char *path, int *handle, qboolean unique) {
    return -1; // File not found
}

static int FS_Read(void *buffer, int size, int handle) {
    return 0;
}

static void FS_FCloseFile(int handle) {
}

static const char* COM_GetExtension(const char *path) {
    const char *ext = strrchr(path, '.');
    return ext ? ext + 1 : "";
}

static char* COM_SkipPath(char *pathname) {
    char *last = strrchr(pathname, '/');
    if (last) {
        return last + 1;
    }
    return pathname;
}

static int Com_sprintf(char *dest, int size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = vsnprintf(dest, size, fmt, args);
    va_end(args);
    return result;
}

// Message buffer stubs for testing
static void MSG_BeginReadingOOB(msg_t *msg) {
    msg->readcount = 0;
    msg->bit = 0;
}

static int MSG_ReadLong(msg_t *msg) {
    if (msg->readcount + 4 > msg->cursize) return 0;
    int value = *(int*)(msg->data + msg->readcount);
    msg->readcount += 4;
    return value;
}

static int MSG_ReadShort(msg_t *msg) {
    if (msg->readcount + 2 > msg->cursize) return 0;
    short value = *(short*)(msg->data + msg->readcount);
    msg->readcount += 2;
    return value;
}

static void MSG_InitOOB(msg_t *msg, byte *data, int length) {
    msg->data = data;
    msg->maxsize = length;
    msg->cursize = 0;
    msg->readcount = 0;
    msg->bit = 0;
}

static void MSG_WriteLong(msg_t *msg, int value) {
    if (msg->cursize + 4 > msg->maxsize) return;
    *(int*)(msg->data + msg->cursize) = value;
    msg->cursize += 4;
}

static void MSG_WriteShort(msg_t *msg, int value) {
    if (msg->cursize + 2 > msg->maxsize) return;
    *(short*)(msg->data + msg->cursize) = (short)value;
    msg->cursize += 2;
}

static void MSG_WriteData(msg_t *msg, const void *data, int length) {
    if (msg->cursize + length > msg->maxsize) return;
    memcpy(msg->data + msg->cursize, data, length);
    msg->cursize += length;
}

// Additional stubs needed for net_chan.c
static int LittleLong(int value) {
    // Assuming little-endian, just return value
    return value;
}

static void Com_Memcpy(void *dest, const void *src, size_t size) {
    memcpy(dest, src, size);
}

static int NET_OutOfBandCompress(int port, byte *data, int length) {
    // Stub - just return original length
    (void)port; (void)data;
    return length;
}

static int NETCHAN_GENCHECKSUM(int challenge, int sequence) {
    // Simple checksum stub
    return challenge ^ sequence;
}

// CVAR stubs
static cvar_t *qport = NULL;
static cvar_t *showpackets = NULL;

#endif // UNIT_TEST

#include "backwards_compatibility.h"
#ifndef UNIT_TEST
#include "qcommon.h"
#endif
#include <string.h>
#include <ctype.h>

// Legacy mode signatures and patterns
static const struct {
    const char *signature;
    legacy_mode_t mode;
    const char *description;
} legacy_signatures[] = {
    // Quake 3 vanilla signatures
    {"quake3.exe", LEGACY_MODE_Q3_VANILLA, "Quake 3 Arena vanilla executable"},
    {"pak0.pk3", LEGACY_MODE_Q3_VANILLA, "Original Quake 3 pak0.pk3"},
    {"q3config.cfg", LEGACY_MODE_Q3_VANILLA, "Vanilla Quake 3 config"},

    // OpenArena signatures
    {"openarena.exe", LEGACY_MODE_OA_COMPATIBLE, "OpenArena executable"},
    {"baseoa", LEGACY_MODE_OA_COMPATIBLE, "OpenArena base directory"},
    {"missionpackoa", LEGACY_MODE_OA_COMPATIBLE, "OpenArena mission pack"},

    // Mod detection patterns
    {"osp", LEGACY_MODE_MOD_GENERIC, "Orange Smoothie Productions mod"},
    {"cpma", LEGACY_MODE_MOD_GENERIC, "Challenge Pro Mode Arena"},
    {"excessive", LEGACY_MODE_MOD_GENERIC, "Excessive Plus mod"},
    {"threewave", LEGACY_MODE_MOD_GENERIC, "Threewave CTF mod"},
    {"freeze", LEGACY_MODE_MOD_GENERIC, "Freeze Tag mod"},
    {"instagib", LEGACY_MODE_MOD_GENERIC, "Instagib mod"},

    {NULL, LEGACY_MODE_NONE, NULL} // Terminator
};

// VM version signatures
static const struct {
    int vm_version;
    legacy_mode_t mode;
    const char *description;
} vm_signatures[] = {
    {100, LEGACY_MODE_Q3_VANILLA, "Quake 3 VM version 100"},
    {101, LEGACY_MODE_Q3_POINT_RELEASE, "Quake 3 point release VM"},
    {102, LEGACY_MODE_OA_COMPATIBLE, "OpenArena compatible VM"},
    {0, LEGACY_MODE_NONE, NULL}
};

// Network protocol signatures
static const struct {
    int protocol;
    legacy_mode_t mode;
    const char *description;
} protocol_signatures[] = {
    {66, LEGACY_MODE_Q3_VANILLA, "Original Quake 3 protocol 66"},
    {67, LEGACY_MODE_Q3_VANILLA, "Quake 3 protocol 67"},
    {68, LEGACY_MODE_Q3_POINT_RELEASE, "Point release protocol 68"},
    {69, LEGACY_MODE_Q3_POINT_RELEASE, "Point release protocol 69"},
    {70, LEGACY_MODE_OA_COMPATIBLE, "OpenArena protocol 70"},
    {71, LEGACY_MODE_OA_COMPATIBLE, "OpenArena protocol 71"},
    {0, LEGACY_MODE_NONE, NULL}
};

// Asset format signatures
static const struct {
    const char *extension;
    const char *signature;
    legacy_mode_t mode;
    const char *description;
} asset_signatures[] = {
    {".shader", "textures/", LEGACY_MODE_Q3_VANILLA, "Vanilla shader format"},
    {".skin", "tag_", LEGACY_MODE_Q3_VANILLA, "Vanilla skin format"},
    {".cfg", " seta ", LEGACY_MODE_Q3_VANILLA, "Vanilla config format"},
    {".arena", "type", LEGACY_MODE_Q3_VANILLA, "Vanilla arena format"},
    {NULL, NULL, LEGACY_MODE_NONE, NULL}
};

// Global CVars
cvar_t *bc_enable_detection;
cvar_t *bc_auto_switch;
cvar_t *bc_strict_mode;
cvar_t *bc_forced_mode;

/*
===============
BC_Init

Initialize backwards compatibility system
===============
*/
qboolean BC_Init(legacy_context_t *ctx) {
    if (!ctx) return qfalse;

    Com_Printf("BC_Init called\n");
    memset(ctx, 0, sizeof(*ctx));

    // Set default configuration
    ctx->config.enable_legacy_detection = qtrue;
    ctx->config.auto_switch_modes = qtrue;
    ctx->config.strict_compatibility = qfalse;
    ctx->config.forced_mode = LEGACY_MODE_NONE;

    ctx->config.allow_legacy_vm_calls = qtrue;
    ctx->config.enable_vm_shims = qtrue;
    ctx->config.vm_strict_mode = qfalse;

    ctx->config.convert_legacy_shaders = qtrue;
    ctx->config.fix_legacy_textures = qtrue;
    ctx->config.enable_asset_fallbacks = qtrue;

    ctx->config.allow_legacy_protocols = qtrue;
    ctx->config.enable_protocol_shims = qtrue;
    ctx->config.max_legacy_clients = 32;

    ctx->config.enable_legacy_renderer_features = qtrue;
    ctx->config.allow_deprecated_renderer_calls = qtrue;
    ctx->config.force_legacy_render_path = qfalse;

    // Initialize statistics
    atomic_init(&ctx->legacy_modes_detected, 0);
    atomic_init(&ctx->compatibility_issues, 0);
    atomic_init(&ctx->shims_applied, 0);

    // Register CVars
    bc_enable_detection = Cvar_Get("bc_enable_detection", "1", CVAR_ARCHIVE);
    bc_auto_switch = Cvar_Get("bc_auto_switch", "1", CVAR_ARCHIVE);
    bc_strict_mode = Cvar_Get("bc_strict_mode", "0", CVAR_ARCHIVE);
    bc_forced_mode = Cvar_Get("bc_forced_mode", "0", CVAR_ARCHIVE);

    Com_Printf("Backwards compatibility system initialized\n");
    return qtrue;
}

/*
===============
BC_Shutdown

Shutdown backwards compatibility system
===============
*/
void BC_Shutdown(legacy_context_t *ctx) {
    if (!ctx) return;

    memset(ctx, 0, sizeof(*ctx));
    Com_Printf("Backwards compatibility system shutdown\n");
}

/*
===============
BC_UpdateDetection

Update compatibility detection (called periodically)
===============
*/
void BC_UpdateDetection(legacy_context_t *ctx) {
    if (!ctx || !ctx->config.enable_legacy_detection) return;

    // Check if we need to start detection
    if (!ctx->detection_active) {
        ctx->detection_active = qtrue;
        ctx->detection_start_time = Sys_Milliseconds();
        Com_DPrintf("Starting backwards compatibility detection\n");
    }

    // Update detection logic
    int current_time = Sys_Milliseconds();
    int detection_time = current_time - ctx->detection_start_time;

    // After 30 seconds of detection, make a determination
    if (detection_time > 30000 && ctx->current_result.detected_mode == LEGACY_MODE_NONE) {
        // No legacy content detected, stay in modern mode
        ctx->current_result.detected_mode = LEGACY_MODE_NONE;
        ctx->current_result.requires_legacy_mode = qfalse;
        ctx->current_result.compatibility_score = 1.0f;
        Q_strncpyz(ctx->current_result.compatibility_notes,
                  "No legacy content detected, using modern mode", sizeof(ctx->current_result.compatibility_notes));

        ctx->detection_active = qfalse;
        Com_Printf("Backwards compatibility detection complete: Modern mode\n");
    }
}

/*
===============
BC_DetectContentCompatibility

Detect compatibility requirements for content
===============
*/
compatibility_result_t BC_DetectContentCompatibility(const char *content_path) {
    compatibility_result_t result = {0};
    result.detected_mode = LEGACY_MODE_NONE;
    result.compatibility_score = 1.0f; // Assume modern by default

    if (!content_path || !*content_path) {
        Q_strncpyz(result.compatibility_notes, "No content path provided", sizeof(result.compatibility_notes));
        return result;
    }

    // Check file extensions and signatures
    const char *ext = COM_GetExtension(content_path);
    if (ext) {
        // Check asset signatures
        for (int i = 0; asset_signatures[i].extension; i++) {
            if (Q_stricmp(ext, asset_signatures[i].extension) == 0) {
                // Read file and check for signature
                fileHandle_t f;
                int len = FS_FOpenFileRead(content_path, &f, qfalse);
                if (len > 0 && len < 1024) { // Only check small files
                    char buffer[1024];
                    FS_Read(buffer, len, f);
                    buffer[len] = '\0';

                    if (strstr(buffer, asset_signatures[i].signature)) {
                        result.detected_mode = asset_signatures[i].mode;
                        result.requires_legacy_mode = qtrue;
                        result.compatibility_score = 0.8f; // Good compatibility
                        Com_sprintf(result.compatibility_notes, sizeof(result.compatibility_notes),
                                  "Detected %s", asset_signatures[i].description);
                        break;
                    }
                }
                FS_FCloseFile(f);
            }
        }
    }

    // Check filename patterns
    const char *filename = COM_SkipPath(content_path);
    for (int i = 0; legacy_signatures[i].signature; i++) {
        if (strstr(filename, legacy_signatures[i].signature)) {
            result.detected_mode = legacy_signatures[i].mode;
            result.requires_legacy_mode = qtrue;
            result.compatibility_score = 0.9f; // High compatibility
            Q_strncpyz(result.detected_mod_name, legacy_signatures[i].signature,
                      sizeof(result.detected_mod_name));
            Com_sprintf(result.compatibility_notes, sizeof(result.compatibility_notes),
                      "Detected %s", legacy_signatures[i].description);
            break;
        }
    }

    return result;
}

/*
===============
BC_DetectModCompatibility

Detect compatibility for specific mods
===============
*/
compatibility_result_t BC_DetectModCompatibility(const char *mod_name) {
    compatibility_result_t result = {0};
    result.detected_mode = LEGACY_MODE_NONE;
    result.compatibility_score = 0.5f; // Unknown mods get medium compatibility

    if (!mod_name || !*mod_name) {
        Q_strncpyz(result.compatibility_notes, "No mod name provided", sizeof(result.compatibility_notes));
        return result;
    }

    // Check against known mod signatures
    for (int i = 0; legacy_signatures[i].signature; i++) {
        if (Q_stricmp(mod_name, legacy_signatures[i].signature) == 0) {
            result.detected_mode = legacy_signatures[i].mode;
            result.requires_legacy_mode = qtrue;
            result.compatibility_score = 0.95f; // Known mods get high compatibility
            Q_strncpyz(result.detected_mod_name, mod_name, sizeof(result.detected_mod_name));
            Com_sprintf(result.compatibility_notes, sizeof(result.compatibility_notes),
                      "Recognized mod: %s", legacy_signatures[i].description);
            break;
        }
    }

    // If not found in signatures, check for common mod patterns
    if (result.detected_mode == LEGACY_MODE_NONE) {
        // Check for common mod directory patterns
        if (strstr(mod_name, "osp") || strstr(mod_name, "cpma") ||
            strstr(mod_name, "excessive") || strstr(mod_name, "threewave")) {
            result.detected_mode = LEGACY_MODE_MOD_GENERIC;
            result.requires_legacy_mode = qtrue;
            result.compatibility_score = 0.85f;
            Q_strncpyz(result.detected_mod_name, mod_name, sizeof(result.detected_mod_name));
            Com_sprintf(result.compatibility_notes, sizeof(result.compatibility_notes),
                      "Detected competitive mod: %s", mod_name);
        } else {
            // Unknown mod - assume it needs legacy support
            result.detected_mode = LEGACY_MODE_MOD_GENERIC;
            result.requires_legacy_mode = qtrue;
            result.compatibility_score = 0.7f;
            Q_strncpyz(result.detected_mod_name, mod_name, sizeof(result.detected_mod_name));
            Com_sprintf(result.compatibility_notes, sizeof(result.compatibility_notes),
                      "Unknown mod detected: %s (assuming legacy compatibility needed)", mod_name);
        }
    }

    return result;
}

/*
===============
BC_DetectNetworkCompatibility

Detect network protocol compatibility
===============
*/
compatibility_result_t BC_DetectNetworkCompatibility(int protocol_version) {
    compatibility_result_t result = {0};
    result.detected_mode = LEGACY_MODE_NONE;
    result.compatibility_score = 1.0f;

    // Check protocol signatures
    for (int i = 0; protocol_signatures[i].protocol; i++) {
        if (protocol_version == protocol_signatures[i].protocol) {
            result.detected_mode = protocol_signatures[i].mode;
            result.requires_legacy_mode = qtrue;
            result.compatibility_score = 0.9f;
            Com_sprintf(result.compatibility_notes, sizeof(result.compatibility_notes),
                      "Detected %s", protocol_signatures[i].description);
            break;
        }
    }

    // Modern protocols don't need legacy mode
    if (protocol_version >= 100) {
        result.detected_mode = LEGACY_MODE_NONE;
        result.requires_legacy_mode = qfalse;
        result.compatibility_score = 1.0f;
        Com_sprintf(result.compatibility_notes, sizeof(result.compatibility_notes),
                  "Modern protocol version %d", protocol_version);
    }

    return result;
}

/*
===============
BC_DetectVMCompatibility

Detect VM compatibility requirements
===============
*/
compatibility_result_t BC_DetectVMCompatibility(int vm_version) {
    compatibility_result_t result = {0};
    result.detected_mode = LEGACY_MODE_NONE;
    result.compatibility_score = 1.0f;

    // Check VM signatures
    for (int i = 0; vm_signatures[i].vm_version; i++) {
        if (vm_version == vm_signatures[i].vm_version) {
            result.detected_mode = vm_signatures[i].mode;
            result.requires_legacy_mode = qtrue;
            result.compatibility_score = 0.9f;
            Com_sprintf(result.compatibility_notes, sizeof(result.compatibility_notes),
                      "Detected %s", vm_signatures[i].description);
            break;
        }
    }

    return result;
}

/*
===============
BC_GetCurrentMode

Get the currently active legacy mode
===============
*/
legacy_mode_t BC_GetCurrentMode(const legacy_context_t *ctx) {
    if (!ctx) return LEGACY_MODE_NONE;

    if (ctx->config.forced_mode != LEGACY_MODE_NONE) {
        return ctx->config.forced_mode;
    }

    return ctx->current_result.detected_mode;
}

/*
===============
BC_SetLegacyMode

Set the active legacy mode
===============
*/
qboolean BC_SetLegacyMode(legacy_context_t *ctx, legacy_mode_t mode) {
    if (!ctx) return qfalse;

    ctx->config.forced_mode = mode;

    if (mode != LEGACY_MODE_NONE) {
        atomic_fetch_add(&ctx->legacy_modes_detected, 1);
        Com_Printf("Backwards compatibility: Enabled %s mode\n", BC_LegacyModeToString(mode));
    } else {
        Com_Printf("Backwards compatibility: Using modern mode\n");
    }

    return qtrue;
}

/*
===============
BC_IsLegacyModeActive

Check if legacy mode is currently active
===============
*/
qboolean BC_IsLegacyModeActive(const legacy_context_t *ctx) {
    return BC_GetCurrentMode(ctx) != LEGACY_MODE_NONE;
}

/*
===============
BC_ApplyVMShim

Apply VM compatibility shim
===============
*/
void *BC_ApplyVMShim(const char *function_name, void *original_function) {
    if (!function_name || !original_function) return original_function;

    // For now, just return the original function
    // In a full implementation, this would create wrapper functions
    // that handle API differences between legacy and modern VMs

    Com_DPrintf("VM shim applied for: %s\n", function_name);
    return original_function;
}

/*
===============
BC_ApplyAssetShim

Apply asset compatibility shim
===============
*/
qboolean BC_ApplyAssetShim(const char *asset_path, char *output_path, int output_size) {
    if (!asset_path || !output_path || output_size <= 0) return qfalse;

    // For now, just copy the path
    // In a full implementation, this would handle asset format conversions
    Q_strncpyz(output_path, asset_path, output_size);

    Com_DPrintf("Asset shim applied for: %s\n", asset_path);
    return qtrue;
}

/*
===============
BC_ApplyNetworkShim

Apply network protocol shim
===============
*/
qboolean BC_ApplyNetworkShim(byte *data, int *length, int max_length) {
    if (!data || !length || *length <= 0) return qfalse;

    // For now, just validate bounds
    // In a full implementation, this would handle protocol conversions
    if (*length > max_length) {
        *length = max_length;
        return qfalse; // Truncated
    }

    return qtrue;
}

/*
===============
BC_AnalyzePK3Compatibility

Analyze PK3 file for compatibility requirements
===============
*/
qboolean BC_AnalyzePK3Compatibility(const char *pk3_path, compatibility_result_t *result) {
    if (!pk3_path || !result) return qfalse;

    // Initialize result
    memset(result, 0, sizeof(*result));
    result->compatibility_score = 1.0f;

    // Check if file exists
    fileHandle_t f;
    int len = FS_FOpenFileRead(pk3_path, &f, qfalse);
    if (len <= 0) {
        Q_strncpyz(result->compatibility_notes, "PK3 file not found", sizeof(result->compatibility_notes));
        return qfalse;
    }

    // Read PK3 header to check format
    byte header[30];
    if (len >= sizeof(header)) {
        FS_Read(header, sizeof(header), f);

        // Check for ZIP/PK3 signature (PK\x03\x04)
        if (header[0] == 'P' && header[1] == 'K' && header[2] == 0x03 && header[3] == 0x04) {
            // Valid PK3/ZIP format
            result->detected_mode = LEGACY_MODE_Q3_VANILLA;
            result->requires_legacy_mode = qtrue;
            result->compatibility_score = 0.95f;
            Q_strncpyz(result->compatibility_notes, "Valid PK3 format detected", sizeof(result->compatibility_notes));
        } else {
            result->detected_mode = LEGACY_MODE_NONE;
            result->requires_legacy_mode = qfalse;
            Q_strncpyz(result->compatibility_notes, "Unknown archive format", sizeof(result->compatibility_notes));
        }
    }

    FS_FCloseFile(f);
    return qtrue;
}

/*
===============
BC_AnalyzeModCompatibility

Analyze mod directory for compatibility
===============
*/
qboolean BC_AnalyzeModCompatibility(const char *mod_path, compatibility_result_t *result) {
    if (!mod_path || !result) return qfalse;

    memset(result, 0, sizeof(*result));

    // Check for common mod files
    char vm_path[MAX_QPATH];
    char cfg_path[MAX_QPATH];

    Com_sprintf(vm_path, sizeof(vm_path), "%s/vm/qagame.qvm", mod_path);
    Com_sprintf(cfg_path, sizeof(cfg_path), "%s/default.cfg", mod_path);

    // Check for VM file
    fileHandle_t f;
    if (FS_FOpenFileRead(vm_path, &f, qfalse) >= 0) {
        FS_FCloseFile(f);
        result->detected_mode = LEGACY_MODE_MOD_GENERIC;
        result->requires_legacy_mode = qtrue;
        result->compatibility_score = 0.9f;
        Q_strncpyz(result->detected_mod_name, mod_path, sizeof(result->detected_mod_name));
        Q_strncpyz(result->compatibility_notes, "QVM file detected - legacy mod", sizeof(result->compatibility_notes));
        return qtrue;
    }

    // Check for config file
    if (FS_FOpenFileRead(cfg_path, &f, qfalse) >= 0) {
        FS_FCloseFile(f);
        result->detected_mode = LEGACY_MODE_MOD_GENERIC;
        result->requires_legacy_mode = qtrue;
        result->compatibility_score = 0.8f;
        Q_strncpyz(result->detected_mod_name, mod_path, sizeof(result->detected_mod_name));
        Q_strncpyz(result->compatibility_notes, "Config file detected - possible legacy mod", sizeof(result->compatibility_notes));
        return qtrue;
    }

    // No specific mod files found
    result->detected_mode = LEGACY_MODE_NONE;
    result->requires_legacy_mode = qfalse;
    result->compatibility_score = 1.0f;
    Q_strncpyz(result->compatibility_notes, "No legacy mod files detected", sizeof(result->compatibility_notes));

    return qtrue;
}

/*
===============
BC_AnalyzeVMCompatibility

Analyze VM file for compatibility
===============
*/
qboolean BC_AnalyzeVMCompatibility(const char *vm_path, compatibility_result_t *result) {
    if (!vm_path || !result) return qfalse;

    memset(result, 0, sizeof(*result));

    fileHandle_t f;
    int len = FS_FOpenFileRead(vm_path, &f, qfalse);
    if (len <= 0) {
        Q_strncpyz(result->compatibility_notes, "VM file not found", sizeof(result->compatibility_notes));
        return qfalse;
    }

    // Read VM header (first 4 bytes should be the magic number)
    int magic;
    FS_Read(&magic, sizeof(magic), f);
    FS_FCloseFile(f);

    // Check VM magic numbers
    if (magic == 0x12721444) { // QVM magic
        result->detected_mode = LEGACY_MODE_Q3_VANILLA;
        result->requires_legacy_mode = qtrue;
        result->compatibility_score = 0.9f;
        Q_strncpyz(result->compatibility_notes, "QVM format detected - legacy VM", sizeof(result->compatibility_notes));
    } else {
        result->detected_mode = LEGACY_MODE_NONE;
        result->requires_legacy_mode = qfalse;
        result->compatibility_score = 1.0f;
        Q_strncpyz(result->compatibility_notes, "Modern or unknown VM format", sizeof(result->compatibility_notes));
    }

    return qtrue;
}

/*
===============
BC_GetStats

Get backwards compatibility statistics
===============
*/
void BC_GetStats(const legacy_context_t *ctx, char *buffer, int buffer_size) {
    if (!ctx || !buffer || buffer_size <= 0) return;

    Com_sprintf(buffer, buffer_size,
               "Backwards Compatibility Stats:\n"
               "  Legacy modes detected: %d\n"
               "  Compatibility issues: %d\n"
               "  Shims applied: %d\n"
               "  Current mode: %s\n"
               "  Detection active: %s\n",
               atomic_load(&ctx->legacy_modes_detected),
               atomic_load(&ctx->compatibility_issues),
               atomic_load(&ctx->shims_applied),
               BC_LegacyModeToString(BC_GetCurrentMode(ctx)),
               ctx->detection_active ? "Yes" : "No");
}

/*
===============
BC_ResetStats

Reset compatibility statistics
===============
*/
void BC_ResetStats(legacy_context_t *ctx) {
    if (!ctx) return;

    atomic_store(&ctx->legacy_modes_detected, 0);
    atomic_store(&ctx->compatibility_issues, 0);
    atomic_store(&ctx->shims_applied, 0);
}

/*
===============
BC_LogCompatibilityIssue

Log a compatibility issue
===============
*/
void BC_LogCompatibilityIssue(const char *issue_description, legacy_mode_t mode) {
    if (!issue_description) return;

    Com_Printf("COMPATIBILITY: %s (Mode: %s)\n", issue_description, BC_LegacyModeToString(mode));
}

/*
===============
BC_LegacyModeToString

Convert legacy mode to string
===============
*/
const char *BC_LegacyModeToString(legacy_mode_t mode) {
    switch (mode) {
        case LEGACY_MODE_NONE: return "None (Modern)";
        case LEGACY_MODE_Q3_VANILLA: return "Quake 3 Vanilla";
        case LEGACY_MODE_Q3_POINT_RELEASE: return "Quake 3 Point Release";
        case LEGACY_MODE_MOD_GENERIC: return "Generic Mod";
        case LEGACY_MODE_OA_COMPATIBLE: return "OpenArena Compatible";
        case LEGACY_MODE_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

/*
===============
BC_StringToLegacyMode

Convert string to legacy mode
===============
*/
legacy_mode_t BC_StringToLegacyMode(const char *mode_str) {
    if (!mode_str) return LEGACY_MODE_NONE;

    if (Q_stricmp(mode_str, "none") == 0 || Q_stricmp(mode_str, "modern") == 0) {
        return LEGACY_MODE_NONE;
    } else if (Q_stricmp(mode_str, "vanilla") == 0 || Q_stricmp(mode_str, "q3") == 0) {
        return LEGACY_MODE_Q3_VANILLA;
    } else if (Q_stricmp(mode_str, "point") == 0 || Q_stricmp(mode_str, "pr") == 0) {
        return LEGACY_MODE_Q3_POINT_RELEASE;
    } else if (Q_stricmp(mode_str, "mod") == 0 || Q_stricmp(mode_str, "generic") == 0) {
        return LEGACY_MODE_MOD_GENERIC;
    } else if (Q_stricmp(mode_str, "oa") == 0 || Q_stricmp(mode_str, "openarena") == 0) {
        return LEGACY_MODE_OA_COMPATIBLE;
    } else if (Q_stricmp(mode_str, "custom") == 0) {
        return LEGACY_MODE_CUSTOM;
    }

    return LEGACY_MODE_NONE;
}

/*
===============
BC_IsCompatibleMode

Check if a mode is compatible
===============
*/
qboolean BC_IsCompatibleMode(legacy_mode_t mode) {
    return mode >= LEGACY_MODE_NONE && mode <= LEGACY_MODE_CUSTOM;
}

/*
===============
BC_CalculateCompatibilityScore

Calculate overall compatibility score
===============
*/
float BC_CalculateCompatibilityScore(const compatibility_result_t *result) {
    if (!result) return 0.0f;

    // Base score from result
    float score = result->compatibility_score;

    // Adjust based on mode
    switch (result->detected_mode) {
        case LEGACY_MODE_NONE:
            score = 1.0f; // Perfect compatibility
            break;
        case LEGACY_MODE_Q3_VANILLA:
            score *= 0.9f; // High compatibility
            break;
        case LEGACY_MODE_Q3_POINT_RELEASE:
            score *= 0.95f; // Very high compatibility
            break;
        case LEGACY_MODE_OA_COMPATIBLE:
            score *= 0.85f; // Good compatibility
            break;
        case LEGACY_MODE_MOD_GENERIC:
            score *= 0.7f; // Moderate compatibility
            break;
        case LEGACY_MODE_CUSTOM:
            score *= 0.5f; // Lower compatibility
            break;
    }

    // Clamp to valid range
    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;

    return score;
}

/*
===============
BC_SetConfig

Set backwards compatibility configuration
===============
*/
void BC_SetConfig(legacy_context_t *ctx, const legacy_config_t *config) {
    if (!ctx || !config) return;
    ctx->config = *config;
}

/*
===============
BC_GetConfig

Get backwards compatibility configuration
===============
*/
void BC_GetConfig(const legacy_context_t *ctx, legacy_config_t *config) {
    if (!ctx || !config) return;
    *config = ctx->config;
}