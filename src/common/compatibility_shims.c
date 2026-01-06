/*
=============================================================================
Compatibility Shims Implementation

Wrapper functions and compatibility layers for maintaining backwards
compatibility with legacy Quake 3 mods and content.
=============================================================================
*/

#include "compatibility_shims.h"
#include "qcommon.h"
#include <string.h>

// Stub renderer interface for testing
#ifdef UNIT_TEST

#define qboolean int
#define qtrue 1
#define qfalse 0

typedef struct {
    float x, y, w, h, s1, t1, s2, t2;
    int hShader;
} refdef_t;

typedef struct {
    void (*RegisterShader)(const char *name);
    void (*RegisterShaderNoMip)(const char *name);
    void (*DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int hShader);
} refexport_t;

static void Stub_RegisterShader(const char *name) {}
static void Stub_RegisterShaderNoMip(const char *name) {}
static void Stub_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int hShader) {}

static refexport_t re = {
    .RegisterShader = Stub_RegisterShader,
    .RegisterShaderNoMip = Stub_RegisterShaderNoMip,
    .DrawStretchPic = Stub_DrawStretchPic
};

#endif // UNIT_TEST

// Global CVars
cvar_t *shim_enable_vm_shims;
cvar_t *shim_enable_asset_shims;
cvar_t *shim_enable_network_shims;
cvar_t *shim_strict_compatibility;

// Static shim contexts for different modes
static shim_context_t q3_vanilla_shims;
static shim_context_t openarena_shims;
static shim_context_t mod_generic_shims;

// Current active shim context
static shim_context_t *active_shims = NULL;

/*
===============
Shim_Init

Initialize compatibility shims system
===============
*/
qboolean Shim_Init(shim_context_t *ctx, legacy_mode_t mode) {
    if (!ctx) return qfalse;

    memset(ctx, 0, sizeof(*ctx));
    ctx->active_mode = mode;

    // Initialize statistics
    atomic_init(&ctx->vm_shims_applied, 0);
    atomic_init(&ctx->asset_shims_applied, 0);
    atomic_init(&ctx->network_shims_applied, 0);
    atomic_init(&ctx->api_calls_redirected, 0);

    // Initialize mode-specific shims
    switch (mode) {
        case LEGACY_MODE_Q3_VANILLA:
            Shim_Init_Q3Vanilla(ctx);
            break;
        case LEGACY_MODE_OA_COMPATIBLE:
            Shim_Init_OpenArena(ctx);
            break;
        case LEGACY_MODE_MOD_GENERIC:
            Shim_Init_ModGeneric(ctx);
            break;
        default:
            // No special shims needed for modern mode
            break;
    }

    // Register CVars
    shim_enable_vm_shims = Cvar_Get("shim_enable_vm_shims", "1", CVAR_ARCHIVE);
    shim_enable_asset_shims = Cvar_Get("shim_enable_asset_shims", "1", CVAR_ARCHIVE);
    shim_enable_network_shims = Cvar_Get("shim_enable_network_shims", "1", CVAR_ARCHIVE);
    shim_strict_compatibility = Cvar_Get("shim_strict_compatibility", "0", CVAR_ARCHIVE);

    Com_Printf("Compatibility shims initialized for mode: %s\n", BC_LegacyModeToString(mode));
    return qtrue;
}

/*
===============
Shim_Shutdown

Shutdown compatibility shims system
===============
*/
void Shim_Shutdown(shim_context_t *ctx) {
    if (!ctx) return;

    // Clean up mode-specific resources
    switch (ctx->active_mode) {
        case LEGACY_MODE_Q3_VANILLA:
            // Q3 vanilla cleanup
            break;
        case LEGACY_MODE_OA_COMPATIBLE:
            // OpenArena cleanup
            break;
        case LEGACY_MODE_MOD_GENERIC:
            // Generic mod cleanup
            break;
        default:
            break;
    }

    memset(ctx, 0, sizeof(*ctx));
    Com_Printf("Compatibility shims shutdown\n");
}

/*
===============
Shim_SetMode

Set the active compatibility mode
===============
*/
void Shim_SetMode(shim_context_t *ctx, legacy_mode_t mode) {
    if (!ctx || ctx->active_mode == mode) return;

    legacy_mode_t old_mode = ctx->active_mode;
    ctx->active_mode = mode;

    // Reinitialize shims for new mode
    Shim_Shutdown(ctx);
    Shim_Init(ctx, mode);

    Com_Printf("Compatibility shims switched from %s to %s\n",
               BC_LegacyModeToString(old_mode), BC_LegacyModeToString(mode));

    active_shims = ctx;
}

/*
===============
Shim_VMFunction

Apply VM function shim
===============
*/
void *Shim_VMFunction(const char *function_name, void *original_function) {
    if (!active_shims || !shim_enable_vm_shims || !shim_enable_vm_shims->integer) {
        return original_function;
    }

    if (!function_name || !original_function) {
        return original_function;
    }

    void *shimmed_function = NULL;

    // Apply mode-specific VM shimming
    switch (active_shims->active_mode) {
        case LEGACY_MODE_Q3_VANILLA:
            // Q3 vanilla VM shims
            if (strcmp(function_name, "syscall") == 0) {
                // Wrap syscall to handle legacy VM calls
                shimmed_function = original_function; // For now, pass through
            }
            break;

        case LEGACY_MODE_OA_COMPATIBLE:
            // OpenArena VM shims
            break;

        case LEGACY_MODE_MOD_GENERIC:
            // Generic mod VM shims
            break;

        default:
            break;
    }

    if (shimmed_function && shimmed_function != original_function) {
        atomic_fetch_add(&active_shims->vm_shims_applied, 1);
        Shim_LogShimApplication("VM", function_name, active_shims->active_mode);
    }

    return shimmed_function ? shimmed_function : original_function;
}

/*
===============
Shim_RegisterShader

Shim for shader registration
===============
*/
qhandle_t Shim_RegisterShader(const char *name) {
    if (!active_shims || !shim_enable_asset_shims || !shim_enable_asset_shims->integer) {
        return re.RegisterShader(name);
    }

    // Apply shader name transformations for legacy compatibility
    char transformed_name[MAX_QPATH];
    if (Shim_LoadShader(name, transformed_name, sizeof(transformed_name))) {
        atomic_fetch_add(&active_shims->asset_shims_applied, 1);
        Shim_LogShimApplication("Shader", name, active_shims->active_mode);
        return re.RegisterShader(transformed_name);
    }

    return re.RegisterShader(name);
}

/*
===============
Shim_RegisterShaderNoMip

Shim for shader registration without mipmaps
===============
*/
qhandle_t Shim_RegisterShaderNoMip(const char *name) {
    if (!active_shims || !shim_enable_asset_shims || !shim_enable_asset_shims->integer) {
        return re.RegisterShaderNoMip(name);
    }

    // Apply shader name transformations for legacy compatibility
    char transformed_name[MAX_QPATH];
    if (Shim_LoadShader(name, transformed_name, sizeof(transformed_name))) {
        atomic_fetch_add(&active_shims->asset_shims_applied, 1);
        Shim_LogShimApplication("ShaderNoMip", name, active_shims->active_mode);
        return re.RegisterShaderNoMip(transformed_name);
    }

    return re.RegisterShaderNoMip(name);
}

/*
===============
Shim_DrawStretchPic

Shim for drawing stretched pictures
===============
*/
void Shim_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
    if (!active_shims) {
        re.DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
        return;
    }

    // Apply legacy coordinate transformations if needed
    switch (active_shims->active_mode) {
        case LEGACY_MODE_Q3_VANILLA:
            Shim_DrawStretchPic_Q3Vanilla(x, y, w, h, s1, t1, s2, t2, hShader);
            break;

        default:
            re.DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
            break;
    }

    atomic_fetch_add(&active_shims->api_calls_redirected, 1);
}

/*
===============
Shim_LoadShader

Apply shader loading transformations
===============
*/
qboolean Shim_LoadShader(const char *shader_name, char *output_path, int output_size) {
    if (!shader_name || !output_path || output_size <= 0) return qfalse;

    // Copy original name as default
    Q_strncpyz(output_path, shader_name, output_size);

    if (!active_shims) return qfalse;

    // Apply mode-specific transformations
    switch (active_shims->active_mode) {
        case LEGACY_MODE_Q3_VANILLA:
            // Q3 vanilla shader transformations
            // Convert modern shader references to legacy equivalents
            if (strcmp(shader_name, "white") == 0) {
                Q_strncpyz(output_path, "white", output_size);
            } else if (strstr(shader_name, "menu/")) {
                // Menu shaders might need different paths in legacy
                Q_strncpyz(output_path, shader_name, output_size);
            }
            return qtrue;

        case LEGACY_MODE_OA_COMPATIBLE:
            return Shim_LoadShader_OpenArena(shader_name, output_path, output_size);

        case LEGACY_MODE_MOD_GENERIC:
            // Generic mod shader transformations
            // Some mods expect different shader naming conventions
            return qtrue;

        default:
            return qfalse;
    }
}

/*
===============
Shim_LoadModel

Apply model loading transformations
===============
*/
qboolean Shim_LoadModel(const char *model_name, char *output_path, int output_size) {
    if (!model_name || !output_path || output_size <= 0) return qfalse;

    Q_strncpyz(output_path, model_name, output_size);

    if (!active_shims) return qfalse;

    // Apply model path transformations for legacy compatibility
    switch (active_shims->active_mode) {
        case LEGACY_MODE_Q3_VANILLA:
            // Q3 vanilla model transformations
            // Some models might have different paths or formats
            return qtrue;

        case LEGACY_MODE_OA_COMPATIBLE:
            // OpenArena model transformations
            return qtrue;

        default:
            return qfalse;
    }
}

/*
===============
Shim_LoadSound

Apply sound loading transformations
===============
*/
qboolean Shim_LoadSound(const char *sound_name, char *output_path, int output_size) {
    if (!sound_name || !output_path || output_size <= 0) return qfalse;

    Q_strncpyz(output_path, sound_name, output_size);

    if (!active_shims) return qfalse;

    // Apply sound path transformations for legacy compatibility
    switch (active_shims->active_mode) {
        case LEGACY_MODE_Q3_VANILLA:
            // Q3 vanilla sound transformations
            // WAV vs OGG format handling, path differences
            return qtrue;

        default:
            return qfalse;
    }
}

/*
===============
Shim_NetworkMessage

Apply network message transformations
===============
*/
qboolean Shim_NetworkMessage(byte *data, int *length, int max_length) {
    if (!data || !length || !active_shims) return qtrue;

    if (!shim_enable_network_shims || !shim_enable_network_shims->integer) {
        return qtrue;
    }

    // Apply mode-specific network transformations
    switch (active_shims->active_mode) {
        case LEGACY_MODE_Q3_VANILLA:
            // Q3 vanilla network message transformations
            return qtrue;

        case LEGACY_MODE_OA_COMPATIBLE:
            return Shim_NetworkMessage_OpenArena(data, length, max_length);

        default:
            return qtrue;
    }
}

/*
===============
Shim_Cvar_Get

Shim for CVar registration
===============
*/
cvar_t *Shim_Cvar_Get(const char *var_name, const char *value, int flags) {
    if (!active_shims) {
        return Cvar_Get(var_name, value, flags);
    }

    // Apply mode-specific CVar handling
    switch (active_shims->active_mode) {
        case LEGACY_MODE_MOD_GENERIC:
            return Shim_Cvar_Get_ModGeneric(var_name, value, flags);

        default:
            return Cvar_Get(var_name, value, flags);
    }
}

/*
===============
Shim_Cmd_AddCommand

Shim for command registration
===============
*/
void Shim_Cmd_AddCommand(const char *cmd_name, void(*function)(void)) {
    if (!active_shims) {
        Cmd_AddCommand(cmd_name, function);
        return;
    }

    // Apply mode-specific command handling
    switch (active_shims->active_mode) {
        case LEGACY_MODE_MOD_GENERIC:
            Shim_Cmd_AddCommand_ModGeneric(cmd_name, function);
            break;

        default:
            Cmd_AddCommand(cmd_name, function);
            break;
    }

    atomic_fetch_add(&active_shims->api_calls_redirected, 1);
}

/*
===============
Shim_Init_Q3Vanilla

Initialize Quake 3 vanilla compatibility shims
===============
*/
void Shim_Init_Q3Vanilla(shim_context_t *ctx) {
    if (!ctx) return;

    // Set up Q3 vanilla specific function pointers
    ctx->renderer_shims.registerShader = Shim_RegisterShader_Q3Vanilla;
    ctx->renderer_shims.drawStretchPic = Shim_DrawStretchPic_Q3Vanilla;

    Com_Printf("Initialized Quake 3 vanilla compatibility shims\n");
}

/*
===============
Shim_Init_OpenArena

Initialize OpenArena compatibility shims
===============
*/
void Shim_Init_OpenArena(shim_context_t *ctx) {
    if (!ctx) return;

    // Set up OpenArena specific function pointers
    ctx->generic_shims.asset_shim = Shim_LoadShader_OpenArena;
    ctx->generic_shims.network_shim = Shim_NetworkMessage_OpenArena;

    Com_Printf("Initialized OpenArena compatibility shims\n");
}

/*
===============
Shim_Init_ModGeneric

Initialize generic mod compatibility shims
===============
*/
void Shim_Init_ModGeneric(shim_context_t *ctx) {
    if (!ctx) return;

    // Set up generic mod compatibility shims
    // These are more permissive and handle common mod patterns

    Com_Printf("Initialized generic mod compatibility shims\n");
}

/*
===============
Mode-specific shim implementations
===============
*/

qhandle_t Shim_RegisterShader_Q3Vanilla(const char *name) {
    // Q3 vanilla shader registration
    // Handle legacy shader naming conventions
    return re.RegisterShader(name);
}

void Shim_DrawStretchPic_Q3Vanilla(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
    // Q3 vanilla coordinate system adjustments
    // Legacy Q3 might use different coordinate origins
    re.DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
}

qboolean Shim_LoadShader_OpenArena(const char *shader_name, char *output_path, int output_size) {
    // OpenArena shader loading transformations
    // Handle OA-specific shader paths and naming
    Q_strncpyz(output_path, shader_name, output_size);

    // OA might have different menu shader locations
    if (strstr(shader_name, "menu/")) {
        // Transform menu shader paths if needed
    }

    return qtrue;
}

qboolean Shim_NetworkMessage_OpenArena(byte *data, int *length, int max_length) {
    // OpenArena network message transformations
    // Handle OA-specific protocol differences
    return qtrue;
}

cvar_t *Shim_Cvar_Get_ModGeneric(const char *var_name, const char *value, int flags) {
    // Generic mod CVar handling
    // Some mods expect different default values or flags
    return Cvar_Get(var_name, value, flags);
}

void Shim_Cmd_AddCommand_ModGeneric(const char *cmd_name, void(*function)(void)) {
    // Generic mod command handling
    // Some mods might have conflicting command names
    Cmd_AddCommand(cmd_name, function);
}

/*
===============
Shim_GetStats

Get shim statistics
===============
*/
void Shim_GetStats(const shim_context_t *ctx, char *buffer, int buffer_size) {
    if (!ctx || !buffer || buffer_size <= 0) return;

    Com_sprintf(buffer, buffer_size,
               "Compatibility Shims Stats:\n"
               "  VM shims applied: %d\n"
               "  Asset shims applied: %d\n"
               "  Network shims applied: %d\n"
               "  API calls redirected: %d\n"
               "  Active mode: %s\n",
               atomic_load(&ctx->vm_shims_applied),
               atomic_load(&ctx->asset_shims_applied),
               atomic_load(&ctx->network_shims_applied),
               atomic_load(&ctx->api_calls_redirected),
               BC_LegacyModeToString(ctx->active_mode));
}

/*
===============
Shim_ResetStats

Reset shim statistics
===============
*/
void Shim_ResetStats(shim_context_t *ctx) {
    if (!ctx) return;

    atomic_store(&ctx->vm_shims_applied, 0);
    atomic_store(&ctx->asset_shims_applied, 0);
    atomic_store(&ctx->network_shims_applied, 0);
    atomic_store(&ctx->api_calls_redirected, 0);
}

/*
===============
Shim_LogShimApplication

Log shim application for debugging
===============
*/
void Shim_LogShimApplication(const char *shim_type, const char *function_name, legacy_mode_t mode) {
    Com_DPrintf("SHIM: Applied %s shim for %s (mode: %s)\n",
               shim_type, function_name, BC_LegacyModeToString(mode));
}

/*
===============
Shim_GetShimDescription

Get description of shim for a mode
===============
*/
const char *Shim_GetShimDescription(legacy_mode_t mode) {
    switch (mode) {
        case LEGACY_MODE_Q3_VANILLA:
            return "Quake 3 vanilla compatibility shims (handles original Q3 API differences)";
        case LEGACY_MODE_OA_COMPATIBLE:
            return "OpenArena compatibility shims (handles OA-specific modifications)";
        case LEGACY_MODE_MOD_GENERIC:
            return "Generic mod compatibility shims (handles common mod patterns)";
        default:
            return "No special shims required for this mode";
    }
}

/*
===============
Shim_IsShimAvailable

Check if shim is available for a mode
===============
*/
qboolean Shim_IsShimAvailable(legacy_mode_t mode, const char *shim_type) {
    // Check if specific shim types are available for each mode
    switch (mode) {
        case LEGACY_MODE_Q3_VANILLA:
            return (strcmp(shim_type, "vm") == 0 ||
                   strcmp(shim_type, "renderer") == 0 ||
                   strcmp(shim_type, "asset") == 0);
        case LEGACY_MODE_OA_COMPATIBLE:
            return (strcmp(shim_type, "asset") == 0 ||
                   strcmp(shim_type, "network") == 0);
        case LEGACY_MODE_MOD_GENERIC:
            return (strcmp(shim_type, "cvar") == 0 ||
                   strcmp(shim_type, "cmd") == 0);
        default:
            return qfalse;
    }
}