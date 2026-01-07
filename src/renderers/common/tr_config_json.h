/*
===========================================================================
JSON-based renderer configuration system header.
===========================================================================
*/

#ifndef TR_CONFIG_JSON_H
#define TR_CONFIG_JSON_H

// JSON-based renderer configuration functions
void R_InitConfigurationJSON(void);
void R_ShutdownConfigurationJSON(void);

// Multisampling configuration
void R_GetMultisamplingConfig(qboolean *enabled, int *samples, const char **quality);
void R_UpdateMultisamplingConfig(qboolean enabled, int samples, const char *quality);

// Texture configuration
void R_GetTextureConfig(int *anisotropy, const char **filtering, const char **compression,
					   qboolean *streaming_enabled, int *streaming_budget_mb, int *preload_mips);
void R_UpdateTextureConfig(int anisotropy, const char *filtering, const char *compression,
						  qboolean streaming_enabled, int streaming_budget_mb, int preload_mips);

// Post-processing configuration
void R_GetPostProcessingConfig(qboolean *bloom_enabled, float *bloom_intensity, float *bloom_threshold,
							  const char **tone_operator, float *tone_exposure,
							  qboolean *color_grading_enabled, const char **color_grading_lut);
void R_UpdatePostProcessingConfig(qboolean bloom_enabled, float bloom_intensity, float bloom_threshold,
								 const char *tone_operator, float tone_exposure,
								 qboolean color_grading_enabled, const char *color_grading_lut);

#endif // TR_CONFIG_JSON_H