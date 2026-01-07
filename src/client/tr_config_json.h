/*
===========================================================================
JSON-based renderer configuration system header.
Provides traditional cvars backed by JSON configuration.
===========================================================================
*/

#ifndef TR_CONFIG_JSON_H
#define TR_CONFIG_JSON_H

// Function prototypes
void R_InitConfigurationJSON(void);
void R_ShutdownConfigurationJSON(void);
void R_SyncConfigurationFromJSON(void);
void R_UpdateConfigurationJSON(void);

// Traditional cvars exposed for renderer access
extern cvar_t *r_multisample_enabled;
extern cvar_t *r_multisample_samples;
extern cvar_t *r_multisample_quality;
extern cvar_t *r_texture_anisotropy;
extern cvar_t *r_texture_filtering;
extern cvar_t *r_texture_compression;
extern cvar_t *r_texture_streaming_enabled;
extern cvar_t *r_texture_streaming_budget;
extern cvar_t *r_texture_streaming_preload;
extern cvar_t *r_bloom_enabled;
extern cvar_t *r_bloom_intensity;
extern cvar_t *r_bloom_threshold;
extern cvar_t *r_tone_mapping_operator;
extern cvar_t *r_tone_mapping_exposure;
extern cvar_t *r_color_grading_enabled;
extern cvar_t *r_color_grading_lut;

#endif // TR_CONFIG_JSON_H