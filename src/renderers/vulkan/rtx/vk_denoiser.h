/*
===========================================================================
id Tech 3 - Denoiser Header

Denoising implementation for RTX renderer
===========================================================================
*/

#ifndef __VK_DENOISER_H__
#define __VK_DENOISER_H__

// Denoiser statistics
typedef struct {
    int frames_processed;
    float avg_noise_reduction;
} denoiser_stats_t;

// Denoiser API
void Denoiser_Init(void);
void Denoiser_Shutdown(void);
void Denoiser_Apply(vec3_t *input, vec3_t *output, int width, int height);
void Denoiser_UpdateStatistics(void);
void Denoiser_GetStatistics(int *frames_processed, float *avg_noise_reduction);
void Denoiser_ResetStatistics(void);

#endif // __VK_DENOISER_H__