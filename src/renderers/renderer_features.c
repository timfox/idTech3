/*
==============================================================================

RENDERER FEATURES CONFIGURATION - IMPLEMENTATION

Centralized feature toggles and configuration for all renderers.

==============================================================================
*/

#include "renderer_features.h"
#include "../common/qcommon.h"
#include "renderercommon/tr_public.h"

// Renderer import interface - defined in renderer main file
extern refimport_t ri;

//==============================================================================
// GLOBAL STATE
//==============================================================================

safe_mode_config_t g_safe_mode = {
    .enabled = qfalse,
    .reason = NULL,
    .disable_experimental = qtrue,
    .disable_debug = qtrue,
    .prefer_compatibility = qtrue
};

//==============================================================================
// SAFE MODE DETECTION
//==============================================================================

/*
===============
R_DetectSafeMode
===============
*/
qboolean R_DetectSafeMode(void) {
    // Check for safe mode flag files
    const char *safe_mode_paths[] = {
        "safe_mode.flag",                    // Current directory
        "logs/safe_mode.flag",               // logs subdirectory
        NULL
    };

    for (int i = 0; safe_mode_paths[i]; i++) {
        FILE *f = fopen(safe_mode_paths[i], "r");
        if (f) {
            fclose(f);
            Com_Printf("Safe mode detected: flag file '%s' found\n", safe_mode_paths[i]);
            return qtrue;
        }
    }

    // Note: Command line argument detection is handled in client code
    // This function focuses on file-based detection for renderer-level safe mode

    return qfalse;
}

/*
===============
R_SetSafeMode
===============
*/
void R_SetSafeMode(qboolean enable, const char *reason) {
    g_safe_mode.enabled = enable;
    g_safe_mode.reason = reason;

    if (enable) {
        Com_Printf(S_COLOR_YELLOW "SAFE MODE ACTIVATED: %s\n", reason);
        Com_Printf(S_COLOR_YELLOW "Experimental and debug features will be disabled\n");
    } else {
        Com_Printf("Safe mode deactivated\n");
    }
}

//==============================================================================
// FEATURE REGISTRATION
//==============================================================================

/*
===============
R_RegisterFeatureCvars
===============
*/
void R_RegisterFeatureCvars(const renderer_feature_t *features, int count) {
    for (int i = 0; i < count; i++) {
        const renderer_feature_t *feature = &features[i];

        // Get or create the cvar
        cvar_t *cvar = Cvar_Get(feature->name, feature->default_value,
                               CVAR_ARCHIVE_ND | (feature->requires_restart ? CVAR_LATCH : 0));

        // Set description if supported
        if (cvar && feature->description) {
            ri.Cvar_SetDescription(cvar, feature->description);
        }

        // Apply safe mode restrictions (only if cvar was successfully created)
        if (cvar && g_safe_mode.enabled) {
            const char *safe_value = NULL;

            if (feature->category == FEATURE_EXPERIMENTAL && g_safe_mode.disable_experimental) {
                safe_value = (feature->safe_mode_state == FEATURE_ENABLED) ? "1" : "0";
            } else if (feature->category == FEATURE_DEBUG && g_safe_mode.disable_debug) {
                safe_value = (feature->safe_mode_state == FEATURE_ENABLED) ? "1" : "0";
            }

            if (safe_value && Q_stricmp(cvar->string, safe_value) != 0) {
                Com_Printf(S_COLOR_CYAN "Safe mode: %s forced to %s (%s)\n",
                          feature->name, safe_value, feature->description);
                ri.Cvar_Set(feature->name, safe_value);
            }
        }
    }
}

/*
===============
R_InitFeatures
===============
*/
void R_InitFeatures(void) {
    // Detect safe mode first
    if (R_DetectSafeMode()) {
        R_SetSafeMode(qtrue, "safe mode flag or command line detected");
    }

    Com_Printf("\n" S_COLOR_CYAN "=== RENDERER FEATURE INITIALIZATION ===\n" S_COLOR_WHITE);

    // Register common features
    R_RegisterFeatureCvars(common_features, ARRAY_LEN(common_features));

    Com_Printf(S_COLOR_CYAN "Registered %d common renderer features\n" S_COLOR_WHITE, (int)ARRAY_LEN(common_features));
}

/*
===============
R_ApplySafeMode
===============
*/
void R_ApplySafeMode(void) {
    if (!g_safe_mode.enabled) {
        return;
    }

    Com_Printf(S_COLOR_YELLOW "Applying safe mode restrictions...\n");

    // Force conservative settings for stability
    ri.Cvar_Set("r_vulkan_validation", "0");  // Disable validation layers
    ri.Cvar_Set("r_vk_bindlessTextures", "0"); // Disable experimental features
    ri.Cvar_Set("r_vrs", "0");                 // Disable VRS
    ri.Cvar_Set("r_vk_raytracing", "0");      // Disable ray tracing
    ri.Cvar_Set("r_ext_multisample", "0");    // Disable MSAA
    ri.Cvar_Set("r_ext_supersample", "0");    // Disable SSAA
    ri.Cvar_Set("developer", "0");            // Disable developer mode

    Com_Printf(S_COLOR_YELLOW "Safe mode restrictions applied\n");
}

//==============================================================================
// FEATURE QUERY FUNCTIONS
//==============================================================================

/*
===============
R_GetFeatureState
===============
*/
feature_state_t R_GetFeatureState(const char *feature_name) {
    // This function is used during initialization when cvars may not be accessible
    // Return FEATURE_AUTO as default - actual state will be determined by cvar system
    return FEATURE_AUTO;
}

/*
===============
R_IsFeatureEnabled
===============
*/
qboolean R_IsFeatureEnabled(const char *feature_name) {
    // Always check actual cvar value first when available (after initialization)
    cvar_t *cvar = ri.Cvar_Get(feature_name, "0", 0);
    if (cvar) {
        // Cvar exists, return its actual value - this reflects user settings and safe mode overrides
        return atoi(cvar->string) != 0;
    }

    // Cvar not registered yet (during early initialization), use safe mode assumptions
    if (g_safe_mode.enabled) {
        // In safe mode, assume experimental and debug features are disabled by default
        // (they will be overridden later when cvars are actually registered)
        return !R_IsExperimentalFeature(feature_name) && !R_IsDebugFeature(feature_name);
    }

    // Normal mode, assume features are enabled by default
    return qtrue;
}

/*
===============
R_IsExperimentalFeature
===============
*/
qboolean R_IsExperimentalFeature(const char *feature_name) {
    // Check all feature arrays
    const renderer_feature_t *all_features[] = { common_features, vulkan_features, opengl_features };
    int counts[] = { ARRAY_LEN(common_features), ARRAY_LEN(vulkan_features), ARRAY_LEN(opengl_features) };

    for (size_t i = 0; i < ARRAY_LEN(all_features); i++) {
        for (size_t j = 0; j < (size_t)counts[i]; j++) {
            if (Q_stricmp(all_features[i][j].name, feature_name) == 0) {
                return (all_features[i][j].category == FEATURE_EXPERIMENTAL);
            }
        }
    }

    return qfalse;
}

/*
===============
R_IsDebugFeature
===============
*/
qboolean R_IsDebugFeature(const char *feature_name) {
    // Check all feature arrays
    const renderer_feature_t *all_features[] = { common_features, vulkan_features, opengl_features };
    int counts[] = { ARRAY_LEN(common_features), ARRAY_LEN(vulkan_features), ARRAY_LEN(opengl_features) };

    for (size_t i = 0; i < ARRAY_LEN(all_features); i++) {
        for (size_t j = 0; j < (size_t)counts[i]; j++) {
            if (Q_stricmp(all_features[i][j].name, feature_name) == 0) {
                return (all_features[i][j].category == FEATURE_DEBUG);
            }
        }
    }

    return qfalse;
}

//==============================================================================
// LOGGING FUNCTIONS
//==============================================================================

/*
===============
R_LogRendererInfo
===============
*/
void R_LogRendererInfo(const char *renderer_name, const char *version) {
    Com_Printf(S_COLOR_GREEN "Renderer: %s %s\n", renderer_name, version ? version : "");

    if (g_safe_mode.enabled) {
        Com_Printf(S_COLOR_YELLOW "Safe Mode: ACTIVE (%s)\n", g_safe_mode.reason);
    } else {
        Com_Printf("Safe Mode: inactive\n");
    }

    Com_Printf("\n");
}

/*
===============
R_LogFeatureStatus
===============
*/
void R_LogFeatureStatus(void) {
    Com_Printf(S_COLOR_CYAN "=== FEATURE STATUS ===\n" S_COLOR_WHITE);

    // Log experimental features
    Com_Printf("Experimental Features:\n");
    qboolean has_experimental = qfalse;

    const renderer_feature_t *all_features[] = { common_features, vulkan_features, opengl_features };
    int counts[] = { ARRAY_LEN(common_features), ARRAY_LEN(vulkan_features), ARRAY_LEN(opengl_features) };

    for (size_t i = 0; i < ARRAY_LEN(all_features); i++) {
        for (size_t j = 0; j < (size_t)counts[i]; j++) {
            const renderer_feature_t *feature = &all_features[i][j];
            if (feature->category == FEATURE_EXPERIMENTAL && R_IsFeatureEnabled(feature->name)) {
                Com_Printf(S_COLOR_YELLOW "  %s: ENABLED (%s)\n", feature->name, feature->description);
                has_experimental = qtrue;
            }
        }
    }

    if (!has_experimental) {
        Com_Printf("  None enabled\n");
    }

    // Log debug features
    Com_Printf("Debug Features:\n");
    qboolean has_debug = qfalse;

    for (size_t i = 0; i < ARRAY_LEN(all_features); i++) {
        for (size_t j = 0; j < (size_t)counts[i]; j++) {
            const renderer_feature_t *feature = &all_features[i][j];
            if (feature->category == FEATURE_DEBUG && R_IsFeatureEnabled(feature->name)) {
                Com_Printf(S_COLOR_MAGENTA "  %s: ENABLED (%s)\n", feature->name, feature->description);
                has_debug = qtrue;
            }
        }
    }

    if (!has_debug) {
        Com_Printf("  None enabled\n");
    }

    Com_Printf("\n");
}

/*
===============
R_LogFallbackInfo
===============
*/
void R_LogFallbackInfo(const char *requested, const char *fallback, const char *reason) {
    Com_Printf(S_COLOR_YELLOW "Renderer fallback: %s -> %s (%s)\n", requested, fallback, reason);
}

/*
===============
R_LogFeatureSummary
===============
*/
void R_LogFeatureSummary(void) {
    int enabled_features = 0;
    int experimental_features = 0;
    int debug_features = 0;

    const renderer_feature_t *all_features[] = { common_features, vulkan_features, opengl_features };
    int counts[] = { ARRAY_LEN(common_features), ARRAY_LEN(vulkan_features), ARRAY_LEN(opengl_features) };

    for (size_t i = 0; i < ARRAY_LEN(all_features); i++) {
        for (size_t j = 0; j < (size_t)counts[i]; j++) {
            const renderer_feature_t *feature = &all_features[i][j];
            if (R_IsFeatureEnabled(feature->name)) {
                enabled_features++;
                if (feature->category == FEATURE_EXPERIMENTAL) {
                    experimental_features++;
                } else if (feature->category == FEATURE_DEBUG) {
                    debug_features++;
                }
            }
        }
    }

    Com_Printf(S_COLOR_CYAN "Feature Summary: %d enabled", enabled_features);
    if (experimental_features > 0) {
        Com_Printf(" (%d experimental", experimental_features);
        if (debug_features > 0) {
            Com_Printf(", %d debug", debug_features);
        }
        Com_Printf(")");
    } else if (debug_features > 0) {
        Com_Printf(" (%d debug)", debug_features);
    }
    Com_Printf("\n" S_COLOR_WHITE);
}