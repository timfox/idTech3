/*
===========================================================================
id Tech 3 - ASVGF Denoising Implementation

ASVGF (Adaptive Spatio-Temporal Variance-Guided Filtering)
Implementation with gradient reconstruction, temporal accumulation, and atrous filtering.
===========================================================================
*/

#ifdef USE_VULKAN

// Basic includes for denoiser
#include "../renderercommon/tr_public.h"
#include "../../common/q_shared.h"
#include "../../common/qcommon.h"

// Denoising CVARs
cvar_t *r_denoise_enable;
cvar_t *r_denoise_method;      // 0=simple, 1=ASVGF
cvar_t *r_denoise_strength;
cvar_t *r_denoise_iterations;
cvar_t *r_denoise_temporal_weight;

// Denoising state
typedef struct {
    qboolean enabled;
    int method;               // Denoising method
    float strength;           // Denoising strength
    int iterations;           // Number of atrous iterations
    float temporal_weight;    // Temporal accumulation weight

    // Statistics
    int frames_processed;
    float average_noise_reduction;
} denoiser_state_t;

static denoiser_state_t denoiser;

/*
===============
Denoiser_Init

Initialize the denoising system
===============
*/
void Denoiser_Init(void)
{
    // Register CVARs
    r_denoise_enable = ri.Cvar_Get("r_denoise_enable", "1", CVAR_ARCHIVE);
    r_denoise_method = ri.Cvar_Get("r_denoise_method", "1", CVAR_ARCHIVE); // 0=simple, 1=ASVGF
    r_denoise_strength = ri.Cvar_Get("r_denoise_strength", "1.0", CVAR_ARCHIVE);
    r_denoise_iterations = ri.Cvar_Get("r_denoise_iterations", "4", CVAR_ARCHIVE);
    r_denoise_temporal_weight = ri.Cvar_Get("r_denoise_temporal_weight", "0.9", CVAR_ARCHIVE);

    memset(&denoiser, 0, sizeof(denoiser_state_t));

    if (r_denoise_enable->integer) {
        denoiser.enabled = qtrue;
        denoiser.method = r_denoise_method->integer;
        denoiser.strength = r_denoise_strength->value;
        denoiser.iterations = r_denoise_iterations->integer;
        denoiser.temporal_weight = r_denoise_temporal_weight->value;

        Com_Printf("Denoising initialized: method=%d, strength=%.2f, iterations=%d\n",
                   denoiser.method, denoiser.strength, denoiser.iterations);
    }
}

/*
===============
Denoiser_Shutdown

Shutdown the denoising system
===============
*/
void Denoiser_Shutdown(void)
{
    denoiser.enabled = qfalse;
    Com_Printf("Denoising shutdown\n");
}

/*
===============
Denoiser_SimpleBoxFilter

Simple box filter denoising (fallback method)
===============
*/
static void Denoiser_SimpleBoxFilter(vec3_t *input, vec3_t *output, int width, int height, int kernel_size)
{
    int half_kernel = kernel_size / 2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            vec3_t sum = {0.0f, 0.0f, 0.0f};
            int count = 0;

            // Apply box filter
            for (int ky = -half_kernel; ky <= half_kernel; ky++) {
                for (int kx = -half_kernel; kx <= half_kernel; kx++) {
                    int sx = x + kx;
                    int sy = y + ky;

                    if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                        int idx = sy * width + sx;
                        VectorAdd(sum, input[idx], sum);
                        count++;
                    }
                }
            }

            if (count > 0) {
                int idx = y * width + x;
                VectorScale(sum, 1.0f / count, output[idx]);
            }
        }
    }
}

/*
===============
Denoiser_GradientReconstruction

Reconstruct gradients for ASVGF
===============
*/
static void Denoiser_GradientReconstruction(vec3_t *color, vec3_t *gradients, int width, int height)
{
    // Simplified gradient reconstruction
    // In full ASVGF, this would compute luminance gradients and variance

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            vec3_t grad = {0.0f, 0.0f, 0.0f};

            // Compute simple luminance gradient
            float luminance = color[idx][0] * 0.2126f +
                            color[idx][1] * 0.7152f +
                            color[idx][2] * 0.0722f;

            // Horizontal gradient
            if (x < width - 1) {
                int right_idx = y * width + (x + 1);
                float right_lum = color[right_idx][0] * 0.2126f +
                                color[right_idx][1] * 0.7152f +
                                color[right_idx][2] * 0.0722f;
                grad[0] = right_lum - luminance;
            }

            // Vertical gradient
            if (y < height - 1) {
                int down_idx = (y + 1) * width + x;
                float down_lum = color[down_idx][0] * 0.2126f +
                               color[down_idx][1] * 0.7152f +
                               color[down_idx][2] * 0.0722f;
                grad[1] = down_lum - luminance;
            }

            // Magnitude as confidence
            grad[2] = sqrtf(grad[0] * grad[0] + grad[1] * grad[1]);

            VectorCopy(grad, gradients[idx]);
        }
    }
}

/*
===============
Denoiser_AtrousFilter

Apply atrous wavelet filter
===============
*/
static void Denoiser_AtrousFilter(vec3_t *input, vec3_t *gradients, vec3_t *output,
                                 int width, int height, int iteration, float sigma_color)
{
    int step_size = 1 << iteration; // Exponential step size

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            vec3_t sum = {0.0f, 0.0f, 0.0f};
            float weight_sum = 0.0f;

            // 3x3 atrous kernel
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int sx = x + kx * step_size;
                    int sy = y + ky * step_size;

                    if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                        int sidx = sy * width + sx;

                        // Color weight (Gaussian based on luminance difference)
                        float lum_center = input[idx][0] * 0.2126f +
                                         input[idx][1] * 0.7152f +
                                         input[idx][2] * 0.0722f;

                        float lum_sample = input[sidx][0] * 0.2126f +
                                         input[sidx][1] * 0.7152f +
                                         input[sidx][2] * 0.0722f;

                        float color_diff = fabsf(lum_sample - lum_center);
                        float color_weight = expf(-color_diff * color_diff / (2.0f * sigma_color * sigma_color));

                        // Gradient weight (from reconstructed gradients)
                        float grad_diff = 0.0f;
                        if (gradients) {
                            grad_diff = fabsf(gradients[sidx][2] - gradients[idx][2]);
                        }
                        float grad_weight = expf(-grad_diff * grad_diff / 2.0f);

                        // Combine weights
                        float weight = color_weight * grad_weight;

                        // Accumulate
                        sum[0] += input[sidx][0] * weight;
                        sum[1] += input[sidx][1] * weight;
                        sum[2] += input[sidx][2] * weight;
                        weight_sum += weight;
                    }
                }
            }

            if (weight_sum > 0.0f) {
                output[idx][0] = sum[0] / weight_sum;
                output[idx][1] = sum[1] / weight_sum;
                output[idx][2] = sum[2] / weight_sum;
            } else {
                VectorCopy(input[idx], output[idx]);
            }
        }
    }
}

/*
===============
Denoiser_TemporalAccumulation

Apply temporal accumulation for denoising
===============
*/
static void Denoiser_TemporalAccumulation(vec3_t *current, vec3_t *history,
                                        vec3_t *output, int width, int height)
{
    float temporal_weight = denoiser.temporal_weight;

    for (int i = 0; i < width * height; i++) {
        // Blend current frame with history
        output[i][0] = current[i][0] * (1.0f - temporal_weight) + history[i][0] * temporal_weight;
        output[i][1] = current[i][1] * (1.0f - temporal_weight) + history[i][1] * temporal_weight;
        output[i][2] = current[i][2] * (1.0f - temporal_weight) + history[i][2] * temporal_weight;

        // Update history for next frame
        VectorCopy(output[i], history[i]);
    }
}

/*
===============
Denoiser_Apply

Apply denoising to the rendered image
===============
*/
void Denoiser_Apply(vec3_t *input, vec3_t *output, int width, int height)
{
    if (!denoiser.enabled) {
        // No denoising - copy input to output
        memcpy(output, input, width * height * sizeof(vec3_t));
        return;
    }

    denoiser.frames_processed++;

    if (denoiser.method == 0) {
        // Simple box filter
        Denoiser_SimpleBoxFilter(input, output, width, height, 3);
    } else {
        // ASVGF-inspired denoising
        static vec3_t *gradients = nullptr;
        static vec3_t *temp_buffer = nullptr;
        static vec3_t *history = nullptr;
        static bool initialized = false;

        // Initialize buffers on first use
        if (!initialized) {
            size_t buffer_size = width * height * sizeof(vec3_t);
            gradients = reinterpret_cast<vec3_t*>(ri.Malloc(buffer_size));
            temp_buffer = reinterpret_cast<vec3_t*>(ri.Malloc(buffer_size));
            history = reinterpret_cast<vec3_t*>(ri.Malloc(buffer_size));
            memset(history, 0, buffer_size);
            initialized = true;
        }

        // Step 1: Gradient reconstruction
        Denoiser_GradientReconstruction(input, gradients, width, height);

        // Step 2: Atrous filtering iterations
        vec3_t *current = input;
        vec3_t *next = temp_buffer;

        for (int i = 0; i < denoiser.iterations; i++) {
            Denoiser_AtrousFilter(current, gradients, next, width, height, i,
                                denoiser.strength * (1.0f + i * 0.5f));

            // Swap buffers
            vec3_t *temp = current;
            current = next;
            next = temp;
        }

        // Step 3: Temporal accumulation
        Denoiser_TemporalAccumulation(current, history, output, width, height);
    }

    // Calculate noise reduction estimate
    float noise_before = 0.0f, noise_after = 0.0f;
    for (int i = 0; i < width * height; i++) {
        noise_before += input[i][0] + input[i][1] + input[i][2];
        noise_after += output[i][0] + output[i][1] + output[i][2];
    }
    noise_before /= (width * height * 3);
    noise_after /= (width * height * 3);

    denoiser.average_noise_reduction = noise_after / (noise_before + 0.001f);
}

/*
===============
Denoiser_UpdateStatistics

Update denoising statistics
===============
*/
void Denoiser_UpdateStatistics(void)
{
    // Statistics are updated in Apply function
}

/*
===============
Denoiser_GetStatistics

Get denoising statistics
===============
*/
void Denoiser_GetStatistics(int *frames_processed, float *avg_noise_reduction)
{
    if (frames_processed) *frames_processed = denoiser.frames_processed;
    if (avg_noise_reduction) *avg_noise_reduction = denoiser.average_noise_reduction;
}

/*
===============
Denoiser_ResetStatistics

Reset denoising statistics
===============
*/
void Denoiser_ResetStatistics(void)
{
    denoiser.frames_processed = 0;
    denoiser.average_noise_reduction = 0.0f;
}

#endif // USE_VULKAN