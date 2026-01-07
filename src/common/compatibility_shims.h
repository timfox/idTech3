/*
=============================================================================
Compatibility Shims Header

Wrapper functions and compatibility layers for maintaining backwards
compatibility with legacy Quake 3 mods and content.
=============================================================================
*/

#ifndef __COMPATIBILITY_SHIMS_H__
#define __COMPATIBILITY_SHIMS_H__

#include "q_shared.h"
#include "backwards_compatibility.h"

// Forward declaration for renderer interface (avoid including tr_public.h here to prevent circular dependencies)
struct refexport_s;

// Shim function types
typedef void *(*shim_func_t)(const char *name, void *original_func);
typedef qboolean (*asset_shim_func_t)(const char *path, char *output_path, int output_size);
typedef qboolean (*network_shim_func_t)(byte *data, int *length, int max_length);

// Legacy API signatures (examples of what might need shimming)

// VM API shims
typedef struct {
    // Original Q3 VM API functions that might need shimming
    int (*syscall)(int, ...);
    void (*error)(const char *, ...);
    void (*print)(const char *, ...);
} vm_api_shims_t;

// Renderer API shims
typedef struct {
    // Legacy renderer functions that might need compatibility layers
    qhandle_t (*registerShader)(const char *name);
    qhandle_t (*registerShaderNoMip)(const char *name);
    void (*drawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
} renderer_api_shims_t;

// Sound API shims
typedef struct {
    // Legacy sound functions
    sfxHandle_t (*registerSound)(const char *sample);
    void (*startSound)(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
} sound_api_shims_t;

// Compatibility shim context
typedef struct {
    legacy_mode_t active_mode;

    // Function pointer tables for different APIs
    vm_api_shims_t vm_shims;
    renderer_api_shims_t renderer_shims;
    sound_api_shims_t sound_shims;

    // Generic shim registry
    struct {
        shim_func_t vm_shim;
        asset_shim_func_t asset_shim;
        network_shim_func_t network_shim;
    } generic_shims;

    // Statistics
    atomic_int_t vm_shims_applied;
    atomic_int_t asset_shims_applied;
    atomic_int_t network_shims_applied;
    atomic_int_t api_calls_redirected;
} shim_context_t;

// Core shim functions
qboolean Shim_Init(shim_context_t *ctx, legacy_mode_t mode);
void Shim_Shutdown(shim_context_t *ctx);
void Shim_SetMode(shim_context_t *ctx, legacy_mode_t mode);

// VM API shims
void *Shim_VMFunction(const char *function_name, void *original_function);
qboolean Shim_VMCall(int syscall_num, void *args);

// Renderer API shims
qhandle_t Shim_RegisterShader(const char *name);
qhandle_t Shim_RegisterShaderNoMip(const char *name);
void Shim_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);

// Asset loading shims
qboolean Shim_LoadShader(const char *shader_name, char *output_path, int output_size);
qboolean Shim_LoadModel(const char *model_name, char *output_path, int output_size);
qboolean Shim_LoadSound(const char *sound_name, char *output_path, int output_size);

// Network protocol shims
qboolean Shim_NetworkMessage(byte *data, int *length, int max_length);
qboolean Shim_GameStateMessage(byte *data, int *length, int max_length);
qboolean Shim_SnapshotMessage(byte *data, int *length, int max_length);

// CVar compatibility shims
cvar_t *Shim_Cvar_Get(const char *var_name, const char *value, int flags);
void Shim_Cvar_Set(const char *var_name, const char *value);

// Command compatibility shims
void Shim_Cmd_AddCommand(const char *cmd_name, void(*function)(void));
void Shim_Cmd_RemoveCommand(const char *cmd_name);

// Filesystem compatibility shims
int Shim_FS_FOpenFile(const char *qpath, fileHandle_t *f, fsMode_t mode);
void Shim_FS_FCloseFile(fileHandle_t f);
int Shim_FS_Read(void *buffer, int len, fileHandle_t f);

// Statistics and monitoring
void Shim_GetStats(const shim_context_t *ctx, char *buffer, int buffer_size);
void Shim_ResetStats(shim_context_t *ctx);

// Mode-specific shim implementations

// Quake 3 Vanilla shims
void Shim_Init_Q3Vanilla(shim_context_t *ctx);
qhandle_t Shim_RegisterShader_Q3Vanilla(const char *name);
void Shim_DrawStretchPic_Q3Vanilla(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);

// OpenArena shims
void Shim_Init_OpenArena(shim_context_t *ctx);
qboolean Shim_LoadShader_OpenArena(const char *shader_name, char *output_path, int output_size);
qboolean Shim_NetworkMessage_OpenArena(byte *data, int *length, int max_length);

// Generic mod shims
void Shim_Init_ModGeneric(shim_context_t *ctx);
cvar_t *Shim_Cvar_Get_ModGeneric(const char *var_name, const char *value, int flags);
void Shim_Cmd_AddCommand_ModGeneric(const char *cmd_name, void(*function)(void));

// Utility functions
const char *Shim_GetShimDescription(legacy_mode_t mode);
qboolean Shim_IsShimAvailable(legacy_mode_t mode, const char *shim_type);
void Shim_LogShimApplication(const char *shim_type, const char *function_name, legacy_mode_t mode);

// Renderer interface management
void Shim_SetRenderer(struct refexport_s *renderer);

// CVars for shim control
extern cvar_t *shim_enable_vm_shims;
extern cvar_t *shim_enable_asset_shims;
extern cvar_t *shim_enable_network_shims;
extern cvar_t *shim_strict_compatibility;

#endif // __COMPATIBILITY_SHIMS_H__