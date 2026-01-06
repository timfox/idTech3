/*
=============================================================================
Backwards Compatibility Header

Enhanced legacy mode system with automatic detection for maintaining
compatibility with existing Quake 3 mods and content.
=============================================================================
*/

#ifndef __BACKWARDS_COMPATIBILITY_H__
#define __BACKWARDS_COMPATIBILITY_H__

#include "q_shared.h"

// Legacy mode types
typedef enum {
    LEGACY_MODE_NONE = 0,        // Modern mode, no compatibility needed
    LEGACY_MODE_Q3_VANILLA,      // Original Quake 3 Arena (1.32)
    LEGACY_MODE_Q3_POINT_RELEASE, // Quake 3 point releases
    LEGACY_MODE_MOD_GENERIC,     // Generic mod compatibility
    LEGACY_MODE_OA_COMPATIBLE,   // OpenArena compatible
    LEGACY_MODE_CUSTOM           // Custom legacy configuration
} legacy_mode_t;

// Compatibility detection results
typedef struct {
    legacy_mode_t detected_mode;
    qboolean requires_legacy_mode;
    char detected_mod_name[MAX_QPATH];
    char compatibility_notes[1024];
    float compatibility_score; // 0.0 = no compatibility, 1.0 = full compatibility
} compatibility_result_t;

// Legacy mode configuration
typedef struct {
    // General settings
    qboolean enable_legacy_detection;
    qboolean auto_switch_modes;
    qboolean strict_compatibility;
    legacy_mode_t forced_mode; // LEGACY_MODE_NONE = auto-detect

    // VM compatibility
    qboolean allow_legacy_vm_calls;
    qboolean enable_vm_shims;
    qboolean vm_strict_mode;

    // Asset compatibility
    qboolean convert_legacy_shaders;
    qboolean fix_legacy_textures;
    qboolean enable_asset_fallbacks;

    // Network compatibility
    qboolean allow_legacy_protocols;
    qboolean enable_protocol_shims;
    int max_legacy_clients;

    // Renderer compatibility
    qboolean enable_legacy_renderer_features;
    qboolean allow_deprecated_renderer_calls;
    qboolean force_legacy_render_path;
} legacy_config_t;

// Compatibility detection context
typedef struct {
    legacy_config_t config;

    // Detection state
    qboolean detection_active;
    int detection_start_time;
    compatibility_result_t current_result;

    // Statistics
    atomic_int_t legacy_modes_detected;
    atomic_int_t compatibility_issues;
    atomic_int_t shims_applied;
} legacy_context_t;

// Core compatibility functions
qboolean BC_Init(legacy_context_t *ctx);
void BC_Shutdown(legacy_context_t *ctx);
void BC_UpdateDetection(legacy_context_t *ctx);

// Detection functions
compatibility_result_t BC_DetectContentCompatibility(const char *content_path);
compatibility_result_t BC_DetectModCompatibility(const char *mod_name);
compatibility_result_t BC_DetectNetworkCompatibility(int protocol_version);
compatibility_result_t BC_DetectVMCompatibility(int vm_version);

// Mode management
legacy_mode_t BC_GetCurrentMode(const legacy_context_t *ctx);
qboolean BC_SetLegacyMode(legacy_context_t *ctx, legacy_mode_t mode);
qboolean BC_IsLegacyModeActive(const legacy_context_t *ctx);

// Compatibility shims and wrappers
void *BC_ApplyVMShim(const char *function_name, void *original_function);
qboolean BC_ApplyAssetShim(const char *asset_path, char *output_path, int output_size);
qboolean BC_ApplyNetworkShim(byte *data, int *length, int max_length);

// Configuration management
void BC_SetConfig(legacy_context_t *ctx, const legacy_config_t *config);
void BC_GetConfig(const legacy_context_t *ctx, legacy_config_t *config);
qboolean BC_LoadConfigFromFile(legacy_context_t *ctx, const char *filename);
qboolean BC_SaveConfigToFile(const legacy_context_t *ctx, const char *filename);

// Content analysis
qboolean BC_AnalyzePK3Compatibility(const char *pk3_path, compatibility_result_t *result);
qboolean BC_AnalyzeModCompatibility(const char *mod_path, compatibility_result_t *result);
qboolean BC_AnalyzeVMCompatibility(const char *vm_path, compatibility_result_t *result);

// Statistics and reporting
void BC_GetStats(const legacy_context_t *ctx, char *buffer, int buffer_size);
void BC_ResetStats(legacy_context_t *ctx);
void BC_LogCompatibilityIssue(const char *issue_description, legacy_mode_t mode);

// Utility functions
const char *BC_LegacyModeToString(legacy_mode_t mode);
legacy_mode_t BC_StringToLegacyMode(const char *mode_str);
qboolean BC_IsCompatibleMode(legacy_mode_t mode);
float BC_CalculateCompatibilityScore(const compatibility_result_t *result);

// CVars for runtime configuration
extern cvar_t *bc_enable_detection;
extern cvar_t *bc_auto_switch;
extern cvar_t *bc_strict_mode;
extern cvar_t *bc_forced_mode;

#endif // __BACKWARDS_COMPATIBILITY_H__