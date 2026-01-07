/*
===========================================================================
JSON-based audio configuration system header.
===========================================================================
*/

#ifndef SND_CONFIG_JSON_H
#define SND_CONFIG_JSON_H

// Function prototypes
void SND_InitConfigurationJSON(void);
void SND_ShutdownConfigurationJSON(void);
void SND_GetGeneralConfig(float *master_volume, float *music_volume, float *sfx_volume, float *voice_volume, qboolean *mute_minimized, qboolean *mute_unfocused);
void SND_UpdateGeneralConfig(float master_volume, float music_volume, float sfx_volume, float voice_volume, qboolean mute_minimized, qboolean mute_unfocused);
void SND_GetSpatialConfig(qboolean *enabled, const char **model, float *max_distance, float *rolloff_factor, float *reference_distance, float *doppler_factor, float *doppler_velocity);
void SND_UpdateSpatialConfig(qboolean enabled, const char *model, float max_distance, float rolloff_factor, float reference_distance);
void SND_GetSourcesConfig(int *max_sources, int *reserved_sources, qboolean *priority_enabled, float *distance_bias, float *volume_bias);
void SND_GetEffectsConfig(qboolean *reverb_enabled, const char **reverb_preset, float *reverb_wet_mix, qboolean *occlusion_enabled, float *occlusion_factor, qboolean *echo_enabled, float *echo_delay);
void SND_UpdateEffectsConfig(qboolean reverb_enabled, const char *reverb_preset, float reverb_wet_mix, qboolean occlusion_enabled, float occlusion_factor, qboolean echo_enabled, float echo_delay);
void SND_GetPerformanceConfig(const char **hrtf_mode, int *sample_rate, int *buffer_size, int *update_frequency);

// JSON-based audio configuration functions
void SND_InitConfigurationJSON(void);
void SND_ShutdownConfigurationJSON(void);

// General audio configuration
void SND_GetGeneralConfig(float *master_volume, float *music_volume, float *sfx_volume, float *voice_volume,
						 qboolean *mute_minimized, qboolean *mute_unfocused);
void SND_UpdateGeneralConfig(float master_volume, float music_volume, float sfx_volume, float voice_volume,
							qboolean mute_minimized, qboolean mute_unfocused);

// Spatial audio configuration
void SND_GetSpatialConfig(qboolean *enabled, const char **model, float *max_distance,
						 float *rolloff_factor, float *reference_distance,
						 float *doppler_factor, float *doppler_velocity);
void SND_UpdateSpatialConfig(qboolean enabled, const char *model, float max_distance,
							float rolloff_factor, float reference_distance);

// Audio sources configuration
void SND_GetSourcesConfig(int *max_sources, int *reserved_sources,
						 qboolean *priority_enabled, float *distance_bias, float *volume_bias);

// Audio effects configuration
void SND_GetEffectsConfig(qboolean *reverb_enabled, const char **reverb_preset, float *reverb_wet_mix,
						 qboolean *occlusion_enabled, float *occlusion_factor,
						 qboolean *echo_enabled, float *echo_delay);
void SND_UpdateEffectsConfig(qboolean reverb_enabled, const char *reverb_preset, float reverb_wet_mix,
							qboolean occlusion_enabled, float occlusion_factor,
							qboolean echo_enabled, float echo_delay);

// Performance configuration
void SND_GetPerformanceConfig(const char **hrtf_mode, int *sample_rate, int *buffer_size, int *update_frequency);

#endif // SND_CONFIG_JSON_H