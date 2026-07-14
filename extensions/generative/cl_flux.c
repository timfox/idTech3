/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_flux.h"
#include "cl_ml_worker.h"
#include "cl_pipeline.h"

#include <stdio.h>
#include <string.h>

#ifdef USE_FLUX
#include "flux.h"
#if USE_SDL
#include <SDL3/SDL.h>
#else
typedef struct SDL_Thread SDL_Thread;
#endif

// FLUX image generation cvars
cvar_t	*cl_flux_enable;
cvar_t	*cl_flux_async;        // 0 = synchronous (blocking), 1 = asynchronous (background)
cvar_t	*cl_flux_external;     // 0 = in-process (unstable), 1 = external CLI (recommended)
cvar_t	*cl_flux_model;        // Model variant: flux1-schnell, flux1-dev, flux2-dev
cvar_t	*cl_flux_device;       // Compute device: auto, cpu, gpu, gpu:N (specific GPU)
cvar_t	*cl_flux_width;
cvar_t	*cl_flux_height;
cvar_t	*cl_flux_steps;
cvar_t	*cl_flux_seed;
/* FonTS (ICCV 2025): external Python DiT pipeline; see docs/FONTS.md */
cvar_t	*cl_fonts_enable;
cvar_t	*cl_fonts_repo;
cvar_t	*cl_fonts_python;
cvar_t	*cl_fonts_cmd;

// FLUX job system for background generation
typedef enum {
	FLUX_JOB_IDLE,
	FLUX_JOB_RUNNING,
	FLUX_JOB_COMPLETED,
	FLUX_JOB_FAILED
} flux_job_status_t;

static qboolean CL_FluxFindCliPath(char *out, size_t out_size) {
	const char *base_path = Sys_DefaultBasePath();
	if (!base_path || !out || out_size == 0) {
		return qfalse;
	}

	Com_sprintf(out, out_size, "%s/flux_cli", base_path);
	if (CL_PipelineFileExists(out)) {
		return qtrue;
	}

	Com_sprintf(out, out_size, "%s/flux_cli.x86_64", base_path);
	if (CL_PipelineFileExists(out)) {
		return qtrue;
	}

	out[0] = '\0';
	return qfalse;
}

static qboolean CL_FluxGenerateExternal(const char *model_path, const char *prompt,
										const char *output_path, int width, int height,
										int steps, int seed,
										char *error_msg, size_t error_msg_size) {
	char cli_path[MAX_OSPATH];
	char model_full[MAX_OSPATH];
	char output_full[MAX_OSPATH];
	char prompt_escaped[2048];
	char cmd[4096];
	const char *base_path = Sys_DefaultBasePath();

	if (!base_path) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "Failed to get base path", error_msg_size);
		}
		return qfalse;
	}

	if (!CL_FluxFindCliPath(cli_path, sizeof(cli_path))) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "flux_cli not found in release directory", error_msg_size);
		}
		return qfalse;
	}

	if (!CL_ShellEscapeArg(prompt, prompt_escaped, sizeof(prompt_escaped))) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "Failed to escape prompt for shell", error_msg_size);
		}
		return qfalse;
	}

	Com_sprintf(model_full, sizeof(model_full), "%s/%s", base_path, model_path);
	Com_sprintf(output_full, sizeof(output_full), "%s/%s", base_path, output_path);

	Com_sprintf(cmd, sizeof(cmd),
		"cd \"%s\" && \"%s\" -d \"%s\" -p \"%s\" -o \"%s\" -W %d -H %d -s %d -S %d -q",
		base_path, cli_path, model_full, prompt_escaped, output_full,
		width, height, steps, seed);

	Com_Printf("FLUX: External generation command: %s\n", cmd);
	if (system(cmd) != 0) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "External FLUX generation failed", error_msg_size);
		}
		return qfalse;
	}

	if (!CL_PipelineFileExists(output_full)) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "External FLUX did not produce output image", error_msg_size);
		}
		return qfalse;
	}

	return qtrue;
}

// Get model directory path based on selected model variant
static const char *CL_FluxGetModelPath(const char *model_variant) {
	// Model variants can have their own directories for better organization
	if (!model_variant || !*model_variant) {
		return "flux"; // Default fallback
	}

	// Map model variants to directory names
	if (Q_stricmp(model_variant, "flux1-schnell") == 0) {
		return "flux1-schnell";
	} else if (Q_stricmp(model_variant, "flux1-dev") == 0) {
		return "flux1-dev";
	} else if (Q_stricmp(model_variant, "flux2-dev") == 0) {
		// FLUX.2 models can be in "flux2-dev" or fallback to "flux"
		// Check if flux2-dev exists, otherwise use flux
		char test_path[MAX_OSPATH];
		Com_sprintf(test_path, sizeof(test_path), "%s/flux2-dev/vae/diffusion_pytorch_model.safetensors", Sys_DefaultBasePath());
		if (CL_PipelineFileExists(test_path)) {
			return "flux2-dev";
		} else {
			return "flux"; // Fallback to default flux directory
		}
	} else {
		Com_Printf(S_COLOR_YELLOW "FLUX: Unknown model variant '%s', using default flux directory\n", model_variant);
		return "flux";
	}
}

typedef struct {
	flux_job_status_t status;
	int start_time;
	int timeout_seconds;
	char model_path[1024];
	char prompt[1024];
	char output_path[1024];
	int width, height, steps, seed;
	flux_image *result;
	char error_msg[1024];
} flux_job_t;

static flux_job_t flux_job;
static clMlTask_t s_fluxTask;
static qboolean s_trellisChainArmed;

/*
==================
FLUX Generation Thread Function
==================
*/
static void CL_Flux_Worker( void *data ) {
	flux_job_t *job = (flux_job_t *)data;
	flux_ctx *ctx = NULL;
	flux_params params = FLUX_PARAMS_DEFAULT;
	flux_image *image = NULL;

	if ( !job ) {
		return;
	}

	// Validate critical job fields before proceeding
	if (!job->model_path[0]) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: model_path is empty\n");
		Q_strncpyz(job->error_msg, "Model path is empty", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return;
	}

	if (!job->prompt[0]) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: prompt is empty\n");
		Q_strncpyz(job->error_msg, "Prompt is empty", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return;
	}

	if (job->width <= 0 || job->height <= 0) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: Invalid dimensions (width:%d height:%d)\n", job->width, job->height);
		Q_strncpyz(job->error_msg, "Invalid image dimensions", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return;
	}

	// External generation mode to avoid in-process crashes
	if (cl_flux_external && cl_flux_external->integer) {
		if (!CL_FluxGenerateExternal(job->model_path, job->prompt, job->output_path,
									 job->width, job->height, job->steps, job->seed,
									 job->error_msg, sizeof(job->error_msg))) {
			job->status = FLUX_JOB_FAILED;
			return;
		}
		job->status = FLUX_JOB_COMPLETED;
		return;
	}

	// Load FLUX model - construct absolute path from base path
	char full_model_path[1024];
	const char *base_path = Sys_DefaultBasePath();
	if (!base_path) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: Sys_DefaultBasePath() returned NULL\n");
		Q_strncpyz(job->error_msg, "Failed to get base path", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return;
	}
	Com_sprintf(full_model_path, sizeof(full_model_path), "%s/%s", base_path, job->model_path);

	// Add safety check for model path
	if (strlen(full_model_path) >= sizeof(full_model_path) - 1) {
		Q_strncpyz(job->error_msg, "Model path too long", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return;
	}

	Com_Printf("FLUX: About to call flux_load_dir()...\n");
	ctx = flux_load_dir(full_model_path);
	Com_Printf("FLUX: flux_load_dir() returned: %p\n", (void*)ctx);
	if (!ctx) {
		const char *error = flux_get_error();
		Com_Printf("FLUX: flux_load_dir() failed with error: %s\n", error ? error : "NULL");
		Q_strncpyz(job->error_msg, error ? error : "Model loading failed", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return;
	}
	Com_Printf("FLUX: Model loaded successfully\n");

	// Set up generation parameters
	Com_Printf("FLUX: Setting up parameters - width:%d height:%d steps:%d seed:%d\n",
		job->width, job->height, job->steps, job->seed);
	params.width = job->width;
	params.height = job->height;
	params.num_steps = job->steps;
	params.seed = job->seed;

	// Validate parameters before generation
	Com_Printf("FLUX: Validating parameters...\n");
	if (strlen(job->prompt) == 0) {
		Com_Printf("FLUX: Empty prompt detected\n");
		Q_strncpyz(job->error_msg, "Empty prompt", sizeof(job->error_msg));
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return;
	}

	if (job->width <= 0 || job->height <= 0 || job->width > 2048 || job->height > 2048) {
		Com_Printf("FLUX: Invalid dimensions - width:%d height:%d\n", job->width, job->height);
		Q_strncpyz(job->error_msg, "Invalid image dimensions", sizeof(job->error_msg));
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return;
	}
	Com_Printf("FLUX: Parameters validated successfully\n");

	// Generate image with error handling
	Com_Printf("FLUX: About to call flux_generate() with prompt: '%s'\n", job->prompt);
        Com_Printf("FLUX: Parameters: width=%d, height=%d, steps=%d, seed=%lld\n",
                params.width, params.height, params.num_steps, (long long)params.seed);
	
	// Validate context and prompt before generation
	if (!ctx) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: ctx is NULL before flux_generate()\n");
		Q_strncpyz(job->error_msg, "FLUX context is NULL", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return;
	}
	if (strlen(job->prompt) == 0) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: prompt is empty before flux_generate()\n");
		Q_strncpyz(job->error_msg, "Prompt is empty", sizeof(job->error_msg));
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return;
	}

	// CRITICAL: This is where the segmentation fault likely occurs
	// Known stability issue: flux_generate() can crash the engine
	// This is documented in README_idtech3.md as a critical stability issue
	Com_Printf("FLUX: Calling flux_generate() - this may take 30-120+ seconds...\n");
	image = flux_generate(ctx, job->prompt, &params);

	Com_Printf("FLUX: flux_generate() returned: %p\n", (void*)image);
	if (!image) {
		const char *error = flux_get_error();
		Com_Printf("FLUX: flux_generate() returned NULL, error: %s\n", error ? error : "NULL");
		if (error) {
			Q_strncpyz(job->error_msg, error, sizeof(job->error_msg));
		} else {
			Q_strncpyz(job->error_msg, "flux_generate returned NULL with no error message", sizeof(job->error_msg));
		}
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return;
	}
	Com_Printf("FLUX: flux_generate() completed successfully - image: %dx%d\n", image->width, image->height);

	// Validate generated image
	Com_Printf("FLUX: Validating generated image...\n");
	if (!image) {
		Com_Printf("FLUX: Image pointer is NULL!\n");
		Q_strncpyz(job->error_msg, "Generated image pointer is NULL", sizeof(job->error_msg));
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return;
	}

	if (!image->data || image->width <= 0 || image->height <= 0) {
		Com_Printf("FLUX: Invalid image data - data:%p width:%d height:%d\n",
			(void *)image->data, image->width, image->height);
		Q_strncpyz(job->error_msg, "Generated image is invalid", sizeof(job->error_msg));
		flux_image_free(image);
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return;
	}
	Com_Printf("FLUX: Image validation passed\n");

	// Save image with error checking
	Com_Printf("FLUX: Saving image to: %s\n", job->output_path);
	int result = flux_image_save(image, job->output_path);
	Com_Printf("FLUX: flux_image_save() returned: %d\n", result);
	if (result != 0) {
		Com_sprintf(job->error_msg, sizeof(job->error_msg), "Failed to save image to %s (error: %d)", job->output_path, result);
		flux_image_free(image);
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return;
	}

	// Verify file was actually created
	Com_Printf("FLUX: Verifying saved file...\n");
	FILE *test_file = fopen(job->output_path, "rb");
	if (!test_file) {
		Com_sprintf(job->error_msg, sizeof(job->error_msg), "Image file not found after save: %s", job->output_path);
		flux_image_free(image);
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return;
	}
	fclose(test_file);
	Com_Printf("FLUX: File saved and verified successfully\n");

	// Store result
	Com_Printf("FLUX: Storing result and marking as completed\n");
	job->result = image;
	job->status = FLUX_JOB_COMPLETED;

	// Clean up (keep image for main thread to handle)
	Com_Printf("FLUX: Cleaning up FLUX context\n");
	flux_free(ctx);
	Com_Printf("FLUX: Thread completed successfully\n");
}

static void CL_Flux_DeferFinalize( void *data )
{
	flux_job_t *job = (flux_job_t *)data;

	if ( !job ) {
		CL_MlWorker_Release( "flux" );
		return;
	}

	if ( job->status == FLUX_JOB_COMPLETED ) {
		Com_Printf( S_COLOR_GREEN "FLUX: background generation finished — %s\n", job->output_path );
	} else if ( job->status == FLUX_JOB_FAILED && job->error_msg[0] ) {
		Com_Printf( S_COLOR_RED "FLUX: background generation failed: %s\n", job->error_msg );
	}

	CL_MlWorker_Release( "flux" );
}

/*
==================
CL_FluxGenerate_f
==================
*/
static void CL_FluxGenerate_f( void ) {
	const char *prompt;

	// Check if FLUX is enabled
	if (!cl_flux_enable || !cl_flux_enable->integer) {
		Com_Printf(S_COLOR_YELLOW "FLUX image generation is disabled. Set cl_flux_enable 1 to enable.\n");
		return;
	}

	// Check if asynchronous job is already running
	if (cl_flux_async->integer && flux_job.status == FLUX_JOB_RUNNING) {
		Com_Printf(S_COLOR_YELLOW "FLUX: Generation already in progress. Wait for completion or use 'flux_cancel' to stop.\n");
		return;
	}

	// Get prompt from command arguments
	if (Cmd_Argc() < 2) {
		Com_Printf(S_COLOR_YELLOW "Usage: flux_generate <prompt>\n");
		return;
	}
	prompt = Cmd_ArgsFrom(1);

	// Check if a job is already running
	if (flux_job.status == FLUX_JOB_RUNNING) {
		Com_Printf(S_COLOR_YELLOW "FLUX: Generation already in progress. Wait for completion or use flux_status to check.\n");
		return;
	}

	// Validate parameters
	int width = cl_flux_width->integer;
	int height = cl_flux_height->integer;
	int steps = cl_flux_steps->integer;
	int seed = cl_flux_seed->integer;

	// Validate dimensions are multiples of 16 (FLUX requirement)
	if (width % 16 != 0 || height % 16 != 0) {
		Com_Printf(S_COLOR_YELLOW "FLUX: Warning: Dimensions should be multiples of 16 for best results (%dx%d)\n",
			width, height);
	}

	// Validate steps
	if (steps < 1 || steps > 50) {
		Com_Printf(S_COLOR_RED "FLUX: Invalid number of steps: %d (must be 1-50)\n", steps);
		return;
	}

	// Choose generation mode based on cvar
	if ( cl_flux_async->integer ) {
		goto async_generation;
	}
	// Synchronous (blocking) mode - revert to original working implementation
	goto sync_generation;

async_generation:
	// Asynchronous generation code

	// Initialize job with safety checks - zero everything first
	Com_Memset(&flux_job, 0, sizeof(flux_job));
	
	// Set all values BEFORE marking as RUNNING to avoid race conditions
	flux_job.start_time = Com_Milliseconds();
	flux_job.timeout_seconds = 300; // 5 minute timeout

	// Get model path based on selected variant
	const char *model_path = CL_FluxGetModelPath(cl_flux_model->string);
	if (!model_path || !*model_path) {
		Com_Printf(S_COLOR_RED "FLUX: Invalid model path\n");
		return;
	}
	
	// Check if FLUX.1 is selected but files might not be available
	if ((Q_stricmp(cl_flux_model->string, "flux1-schnell") == 0 || 
	     Q_stricmp(cl_flux_model->string, "flux1-dev") == 0) &&
	    Q_stricmp(model_path, "flux") != 0) {
		char test_path[MAX_OSPATH];
		Com_sprintf(test_path, sizeof(test_path), "%s/%s/vae/diffusion_pytorch_model.safetensors", 
		            Sys_DefaultBasePath(), model_path);
		if (!CL_PipelineFileExists(test_path)) {
			Com_Printf(S_COLOR_YELLOW "FLUX: FLUX.1 model files not found in %s/\n", model_path);
			Com_Printf(S_COLOR_YELLOW "FLUX: To use FLUX.1, download model files manually (see MANUAL_DOWNLOAD.md)\n");
			Com_Printf(S_COLOR_YELLOW "FLUX: Or switch to FLUX.2: /cl_flux_model flux2-dev\n");
			return;
		}
	}
	
	Q_strncpyz(flux_job.model_path, model_path, sizeof(flux_job.model_path));

	// Validate and copy prompt
	if (!prompt || strlen(prompt) == 0) {
		Com_Printf(S_COLOR_RED "FLUX: Empty prompt\n");
		return;
	}
	Q_strncpyz(flux_job.prompt, prompt, sizeof(flux_job.prompt));

	flux_job.width = width;
	flux_job.height = height;
	flux_job.steps = steps;
	flux_job.seed = seed;

	// Create output filename with timestamp
	Com_sprintf(flux_job.output_path, sizeof(flux_job.output_path), "flux_%d_%dx%d.png",
		Com_Milliseconds(), width, height);
	
	// Verify all critical fields are set before marking as RUNNING
	if (!flux_job.model_path[0] || !flux_job.prompt[0] || flux_job.width <= 0 || flux_job.height <= 0) {
		Com_Printf(S_COLOR_RED "FLUX: Internal error - job structure not properly initialized\n");
		Com_Printf(S_COLOR_RED "FLUX: model_path='%s', prompt='%s', width=%d, height=%d\n",
			flux_job.model_path, flux_job.prompt, flux_job.width, flux_job.height);
		return;
	}
	
	// Mark as RUNNING only after all values are set (memory barrier for thread safety)
	flux_job.status = FLUX_JOB_RUNNING;

	if ( CL_MlWorker_IsBusy() ) {
		flux_job.status = FLUX_JOB_IDLE;
		Com_Printf( S_COLOR_YELLOW "FLUX: ML worker busy (%s); wait or flux_cancel\n",
			CL_MlWorker_Owner() );
		return;
	}

	CL_MlWorker_InitTask( &s_fluxTask, "flux", CL_Flux_Worker, CL_Flux_DeferFinalize, &flux_job );

	if ( !CL_MlWorker_Submit( &s_fluxTask ) ) {
		flux_job.status = FLUX_JOB_IDLE;
		Com_Printf( S_COLOR_RED "FLUX: Failed to start generation worker\n" );
		return;
	}

	Com_Printf("FLUX: Started background generation for prompt: %s\n", prompt);
	Com_Printf("FLUX: Model: %s, Dimensions: %dx%d, Steps: %d, Seed: %d\n",
		flux_job.model_path, flux_job.width, flux_job.height, flux_job.steps, flux_job.seed);
	Com_Printf("FLUX: Use 'flux_status' to check progress, 'flux_view <filename>' when complete\n");
	return;

sync_generation:
	// Synchronous (blocking) mode - original working implementation
	{
		char outputPath[MAX_OSPATH];
		flux_ctx *ctx = NULL;
		flux_params params = FLUX_PARAMS_DEFAULT;
		flux_image *image = NULL;
		int result;

		Com_Printf("FLUX: Generating image with prompt: %s\n", prompt);

		if (cl_flux_external && cl_flux_external->integer) {
			const char *sync_model_path = CL_FluxGetModelPath(cl_flux_model->string);
			Com_sprintf(outputPath, sizeof(outputPath), "flux_%d_%dx%d.png",
				Com_Milliseconds(), width, height);
			if (!CL_FluxGenerateExternal(sync_model_path, prompt, outputPath,
										 width, height, steps, seed,
										 NULL, 0)) {
				Com_Printf(S_COLOR_RED "FLUX: External generation failed (see console for details)\n");
				return;
			}

			Com_Printf(S_COLOR_GREEN "FLUX: Image saved to %s (seed: %d)\n", outputPath, seed);
			if (re.ReloadTexture) {
				if (re.ReloadTexture(outputPath)) {
					Com_Printf(S_COLOR_GREEN "FLUX: Texture hot-reloaded successfully!\n");
				} else {
					Com_Printf(S_COLOR_YELLOW "FLUX: Hot-reload failed, use 'vid_restart' to reload all textures\n");
				}
			} else {
				Com_Printf(S_COLOR_YELLOW "FLUX: Renderer doesn't support hot-reload, use 'vid_restart'\n");
			}
			return;
		}

		// Load FLUX model - construct absolute path from base path
		char full_model_path[1024];
		const char *sync_model_path = CL_FluxGetModelPath(cl_flux_model->string);
		Com_sprintf(full_model_path, sizeof(full_model_path), "%s/%s", Sys_DefaultBasePath(), sync_model_path);
		ctx = flux_load_dir(full_model_path);
		if (!ctx) {
			const char *error = flux_get_error();
			Com_Printf(S_COLOR_RED "FLUX: Failed to load model from %s: %s\n",
				full_model_path, error ? error : "Unknown error");
			
			// Suggest FLUX.2 if FLUX.1 fails
			if (Q_stricmp(cl_flux_model->string, "flux1-schnell") == 0 || 
			    Q_stricmp(cl_flux_model->string, "flux1-dev") == 0) {
				Com_Printf(S_COLOR_YELLOW "FLUX: FLUX.1 model files may be missing or corrupted.\n");
				Com_Printf(S_COLOR_YELLOW "FLUX: Try switching to FLUX.2: /cl_flux_model flux2-dev\n");
			}
			return;
		}

		// Set up generation parameters
		params.width = width;
		params.height = height;
		params.num_steps = steps;
		params.seed = seed;

		// Generate image
		image = flux_generate(ctx, prompt, &params);
		if (!image) {
			Com_Printf(S_COLOR_RED "FLUX: Generation failed: %s\n", flux_get_error());
			flux_free(ctx);
			return;
		}

		// Create output filename with timestamp
		Com_sprintf(outputPath, sizeof(outputPath), "flux_%d_%dx%d.png",
			Com_Milliseconds(), image->width, image->height);

		// Save image
		result = flux_image_save(image, outputPath);
		if (result != 0) {
			Com_Printf(S_COLOR_RED "FLUX: Failed to save image to %s\n", outputPath);
		} else {
			Com_Printf(S_COLOR_GREEN "FLUX: Image saved to %s (seed: %lld)\n",
				outputPath, (long long)params.seed);

			// Try to hot-reload the texture if renderer supports it
			if (re.ReloadTexture) {
				if (re.ReloadTexture(outputPath)) {
					Com_Printf(S_COLOR_GREEN "FLUX: Texture hot-reloaded successfully!\n");
				} else {
					Com_Printf(S_COLOR_YELLOW "FLUX: Hot-reload failed, use 'vid_restart' to reload all textures\n");
				}
			} else {
				Com_Printf(S_COLOR_YELLOW "FLUX: Renderer doesn't support hot-reload, use 'vid_restart'\n");
			}
		}

		// Clean up
		flux_image_free(image);
		flux_free(ctx);
	}
}

/*
==================
CL_FluxDevices_f
==================
*/
static void CL_FluxDevices_f( void ) {
	Com_Printf("FLUX Device & Backend Information:\n");
	Com_Printf("===================================\n");
	Com_Printf("Current device setting: %s\n", cl_flux_device->string);
	Com_Printf("Model variant: %s\n", cl_flux_model->string);
	Com_Printf("\n");

	// Show available FLUX backends (compile-time)
	Com_Printf("Compiled FLUX backends:\n");
#if defined(USE_METAL)
	Com_Printf("  ✅ Metal Performance Shaders (Apple Silicon)\n");
#endif
#if defined(USE_BLAS)
	Com_Printf("  ✅ BLAS accelerated (Intel/AMD CPUs)\n");
#endif
#if defined(USE_CUDA)
	Com_Printf("  ✅ CUDA (NVIDIA GPUs)\n");
#endif
#if defined(USE_VULKAN)
	Com_Printf("  ✅ Vulkan (Cross-platform GPU)\n");
#endif
#if !defined(USE_METAL) && !defined(USE_BLAS) && !defined(USE_CUDA) && !defined(USE_VULKAN)
	Com_Printf("  ⚠️  Generic C fallback (slow CPU-only)\n");
#endif

	Com_Printf("\nDevice selection options:\n");
	Com_Printf("  auto     - Use best available backend\n");
	Com_Printf("  cpu      - Force CPU-only processing\n");
	Com_Printf("  gpu      - Use GPU acceleration\n");
	Com_Printf("  gpu:N    - Use specific GPU (gpu:0, gpu:1, etc.)\n");

	Com_Printf("\nNote: Device selection requires FLUX library support.\n");
	Com_Printf("Currently using compile-time backend selection.\n");
}

/*
==================
CL_FluxCancel_f
==================
*/
static void CL_FluxCancel_f( void ) {
	if (flux_job.status == FLUX_JOB_RUNNING) {
		Com_Printf("FLUX: Cancelling generation...\n");

		// Set status to failed first to prevent race conditions
		flux_job.status = FLUX_JOB_FAILED;
		Q_strncpyz(flux_job.error_msg, "Generation cancelled by user", sizeof(flux_job.error_msg));

		CL_MlWorker_Cancel( &s_fluxTask );

		// Clean up any allocated resources
		if (flux_job.result) {
			flux_image_free(flux_job.result);
			flux_job.result = NULL;
		}

		// Remove any partial output file
		if (flux_job.output_path[0]) {
			remove(flux_job.output_path);
		}

		flux_job.status = FLUX_JOB_IDLE;
		Com_Printf("FLUX: Generation cancelled successfully\n");
	} else {
		Com_Printf("FLUX: No active generation to cancel\n");
	}
}

/*
==================
CL_FluxStatus_f
==================
*/
static void CL_FluxStatus_f( void ) {
	switch (flux_job.status) {
		case FLUX_JOB_IDLE:
			Com_Printf("FLUX: No generation in progress\n");
			Com_Printf("FLUX: Device: %s, Model: %s\n", cl_flux_device->string, cl_flux_model->string);
			break;
		case FLUX_JOB_RUNNING:
			{
				int runtime_seconds = (Com_Milliseconds() - flux_job.start_time) / 1000;
				int timeout_seconds = 300; // 5 minute timeout

				Com_Printf("FLUX: Generation in progress (%d seconds)...\n", runtime_seconds);
				Com_Printf("FLUX: Device: %s, Model: %s\n", cl_flux_device->string, cl_flux_model->string);
				Com_Printf("FLUX: Prompt: %s\n", flux_job.prompt);
				Com_Printf("FLUX: Output: %s\n", flux_job.output_path);
				Com_Printf("FLUX: Settings: %dx%d, %d steps, seed %d\n",
					flux_job.width, flux_job.height, flux_job.steps, flux_job.seed);

				if (runtime_seconds > timeout_seconds) {
					Com_Printf(S_COLOR_YELLOW "FLUX: Generation timeout after %d seconds, use 'flux_cancel' to stop\n", timeout_seconds);
				} else {
					int remaining = timeout_seconds - runtime_seconds;
					Com_Printf("FLUX: Image generation can take 30-120+ seconds depending on hardware (%d seconds until timeout)\n", remaining);
				}
			}
			break;
		case FLUX_JOB_COMPLETED:
			Com_Printf(S_COLOR_GREEN "FLUX: Generation completed!\n");
			Com_Printf("FLUX: Image saved to %s\n", flux_job.output_path);
			Com_Printf("FLUX: Use 'flux_view %s' to display the image\n", flux_job.output_path);
			Com_Printf(S_COLOR_CYAN "FLUX: You can also use 'flux_view' without arguments to view this image\n");
			break;
		case FLUX_JOB_FAILED:
			Com_Printf(S_COLOR_RED "FLUX: Generation failed: %s\n", flux_job.error_msg);
			break;
	}
}

/*
==================
CL_FluxReload_f
==================
*/
static void CL_FluxReload_f( void ) {
	const char *filename;

	if (Cmd_Argc() < 2) {
		Com_Printf(S_COLOR_YELLOW "Usage: flux_reload <filename>\n");
		Com_Printf(S_COLOR_YELLOW "Example: flux_reload flux_123456_256x256.png\n");
		return;
	}

	filename = Cmd_Argv(1);

	// Try to reload the texture
	if (re.ReloadTexture && re.ReloadTexture(filename)) {
		Com_Printf(S_COLOR_GREEN "FLUX: Successfully reloaded texture %s\n", filename);
	} else {
		Com_Printf(S_COLOR_YELLOW "FLUX: Failed to reload texture %s. Use 'vid_restart' if needed.\n", filename);
	}
}

/*
==================
CL_FluxShow_f
==================
*/
static void CL_FluxShow_f( void ) {
	const char *filename;
	qhandle_t shader;

	if (Cmd_Argc() < 2) {
		Com_Printf(S_COLOR_YELLOW "Usage: flux_show <filename>\n");
		Com_Printf(S_COLOR_YELLOW "Example: flux_show flux_123456_256x256.png\n");
		Com_Printf(S_COLOR_CYAN "This will register the image as a shader for use in menus/materials\n");
		return;
	}

	filename = Cmd_Argv(1);

	// Register the image as a shader (this forces loading/reloading)
	shader = re.RegisterShader(filename);
	if (shader) {
		Com_Printf(S_COLOR_GREEN "FLUX: Registered/updated shader '%s' (handle: %d)\n", filename, shader);
		Com_Printf(S_COLOR_CYAN "FLUX: Image is now available for use in the engine\n");
		Com_Printf(S_COLOR_CYAN "FLUX: You can now see the image in menus or use it in materials\n");
	} else {
		Com_Printf(S_COLOR_RED "FLUX: Failed to register shader for %s\n", filename);
	}
}

/*
==================
CL_FluxView_f
==================
*/
static void CL_FluxView_f( void ) {
	const char *filename;
	qboolean reloadSuccess = qfalse;
	qhandle_t shader;

	// If no filename provided, check for completed job
	if (Cmd_Argc() < 2) {
		if (flux_job.status == FLUX_JOB_COMPLETED && flux_job.output_path[0]) {
			filename = flux_job.output_path;
			Com_Printf("FLUX: Using completed job output: %s\n", filename);
		} else {
			Com_Printf(S_COLOR_YELLOW "Usage: flux_view <filename>\n");
			Com_Printf(S_COLOR_YELLOW "Example: flux_view flux_123456_256x256.png\n");
			Com_Printf(S_COLOR_CYAN "This will reload the texture and register it as a shader for viewing\n");
			if (flux_job.status == FLUX_JOB_COMPLETED) {
				Com_Printf(S_COLOR_CYAN "Or use 'flux_view' with no arguments to view the last completed generation\n");
			}
			return;
		}
	} else {
		filename = Cmd_Argv(1);
	}

	// First try to reload the texture
	if (re.ReloadTexture) {
		reloadSuccess = re.ReloadTexture(filename);
		if (reloadSuccess) {
			Com_Printf(S_COLOR_GREEN "FLUX: Texture reloaded: %s\n", filename);
		} else {
			Com_Printf(S_COLOR_YELLOW "FLUX: Texture reload failed, but continuing...\n");
		}
	}

	// Then register/update the shader
	shader = re.RegisterShader(filename);
	if (shader) {
		Com_Printf(S_COLOR_GREEN "FLUX: Shader registered: '%s' (handle: %d)\n", filename, shader);
		Com_Printf(S_COLOR_CYAN "FLUX: Image is now viewable in the engine!\n");
		if (reloadSuccess) {
			Com_Printf(S_COLOR_CYAN "FLUX: Hot-reload successful - no vid_restart needed!\n");
		}
	} else {
		Com_Printf(S_COLOR_RED "FLUX: Failed to register shader for %s\n", filename);
	}

	// If this was a completed job, clean it up
	if ( flux_job.status == FLUX_JOB_COMPLETED && strcmp( filename, flux_job.output_path ) == 0 ) {
		if ( flux_job.result ) {
			flux_image_free( flux_job.result );
			flux_job.result = NULL;
		}
		flux_job.status = FLUX_JOB_IDLE;
		Com_Printf( S_COLOR_GREEN "FLUX: Job completed and cleaned up\n" );
	}
}

/*
==================
CL_FontsPipeline_f

Run a user-configured shell command for the FonTS (ICCV 2025) typography pipeline.
Upstream FonTS is Python + PyTorch + diffusers; the engine only orchestrates via system().
==================
*/
static void CL_FontsPipeline_f( void ) {
	char cmd[8192];
	const char *repo;
	const char *base;
	const char *py;
	const char *args;
	const char *tmpl;

	if ( !cl_fonts_enable || !cl_fonts_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "fonts_pipeline: set cl_fonts_enable 1 (see docs/FONTS.md)\n" );
		return;
	}
	repo = cl_fonts_repo ? cl_fonts_repo->string : "";
	if ( !repo || !repo[0] ) {
		Com_Printf( S_COLOR_YELLOW "fonts_pipeline: set cl_fonts_repo to your FonTS checkout path\n" );
		return;
	}
	base = Sys_DefaultBasePath();
	if ( !base ) {
		Com_Printf( S_COLOR_RED "fonts_pipeline: no engine base path\n" );
		return;
	}
	py = ( cl_fonts_python && cl_fonts_python->string[0] ) ? cl_fonts_python->string : "python3";
	tmpl = cl_fonts_cmd ? cl_fonts_cmd->string : "";
	if ( !tmpl || !tmpl[0] ) {
		Com_Printf( S_COLOR_YELLOW "fonts_pipeline: set cl_fonts_cmd (template with %%R %%B %%P %%A — see docs/FONTS.md)\n" );
		return;
	}
	args = ( Cmd_Argc() >= 2 ) ? Cmd_ArgsFrom( 1 ) : "";
	{
		cl_pipeline_expand_t ex;
		Com_Memset( &ex, 0, sizeof( ex ) );
		ex.repo = repo;
		ex.base = base;
		ex.py = py;
		ex.args = args;
		if ( !CL_PipelineExpandTemplate( cmd, sizeof( cmd ), tmpl, &ex ) ) {
			Com_Printf( S_COLOR_RED "fonts_pipeline: expanded command too long or bad path characters\n" );
			return;
		}
	}
	Com_Printf( "FonTS: executing (blocking): %s\n", cmd );
	if ( system( cmd ) != 0 ) {
		Com_Printf( S_COLOR_RED "fonts_pipeline: shell returned non-zero\n" );
		return;
	}
	Com_Printf( S_COLOR_GREEN "fonts_pipeline: finished\n" );
}

clFluxJobStatus_t CL_Flux_GetJobStatus( void )
{
	return (clFluxJobStatus_t)flux_job.status;
}

const char *CL_Flux_GetOutputPath( void )
{
	return flux_job.output_path;
}

qboolean CL_Flux_IsRunning( void )
{
	return flux_job.status == FLUX_JOB_RUNNING;
}

void CL_Flux_ArmTrellisChain( void )
{
	s_trellisChainArmed = qtrue;
}

qboolean CL_Flux_IsTrellisChainArmed( void )
{
	return s_trellisChainArmed;
}

void CL_Flux_ClearTrellisChain( void )
{
	s_trellisChainArmed = qfalse;
}

void CL_Flux_Frame( void )
{
	if ( flux_job.status != FLUX_JOB_RUNNING || flux_job.timeout_seconds <= 0 ) {
		return;
	}

	{
		int runtime = ( Com_Milliseconds() - flux_job.start_time ) / 1000;
		if ( runtime > flux_job.timeout_seconds ) {
			Com_Printf( S_COLOR_YELLOW "FLUX: generation exceeded %d s (still running); flux_cancel to stop\n",
				flux_job.timeout_seconds );
			flux_job.timeout_seconds = 0;
		}
	}
}

void CL_Flux_Init( void )
{
	cl_flux_enable = Cvar_Get( "cl_flux_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_flux_enable, "Enable FLUX.2 image generation features. Requires model files to be present." );

	cl_flux_async = Cvar_Get( "cl_flux_async", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_async, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_flux_async, "FLUX generation mode: 0=synchronous (blocking), 1=asynchronous (background)." );

	cl_flux_external = Cvar_Get( "cl_flux_external", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_external, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_flux_external, "Use external flux_cli process to avoid in-process crashes (recommended)." );

	cl_flux_model = Cvar_Get( "cl_flux_model", "flux1-schnell", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_flux_model, "FLUX model variant: flux1-schnell (fast), flux1-dev (balanced), flux2-dev (high quality)." );

	cl_flux_device = Cvar_Get( "cl_flux_device", "auto", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_flux_device, "FLUX compute device: auto (default), cpu (force CPU), gpu (force GPU), gpu:0/1/etc (specific GPU)." );

	cl_flux_width = Cvar_Get( "cl_flux_width", "256", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_width, "64", "1792", CV_INTEGER );
	Cvar_SetDescription( cl_flux_width, "Width of generated images in pixels. Must be multiple of 16." );

	cl_flux_height = Cvar_Get( "cl_flux_height", "256", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_height, "64", "1792", CV_INTEGER );
	Cvar_SetDescription( cl_flux_height, "Height of generated images in pixels. Must be multiple of 16." );

	cl_flux_steps = Cvar_Get( "cl_flux_steps", "2", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_steps, "1", "50", CV_INTEGER );
	Cvar_SetDescription( cl_flux_steps, "Number of denoising steps for image generation. Higher = better quality but slower." );

	cl_flux_seed = Cvar_Get( "cl_flux_seed", "-1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_seed, "-1", "2147483647", CV_INTEGER );
	Cvar_SetDescription( cl_flux_seed, "Random seed for reproducible image generation. -1 for random seed." );

	cl_fonts_enable = Cvar_Get( "cl_fonts_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_fonts_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_fonts_enable,
		"Enable FonTS (ICCV 2025) external Python pipeline hook. Requires separate FonTS repo + GPU env; see docs/FONTS.md." );
	cl_fonts_repo = Cvar_Get( "cl_fonts_repo", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_fonts_repo, "Absolute path to a checkout of github.com/ArtmeScienceLab/FonTS (used as %R in cl_fonts_cmd)." );
	cl_fonts_python = Cvar_Get( "cl_fonts_python", "python3", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_fonts_python, "Python interpreter for fonts_pipeline (substituted as %P in cl_fonts_cmd)." );
	cl_fonts_cmd = Cvar_Get( "cl_fonts_cmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_fonts_cmd,
		"Shell template for fonts_pipeline: use %R FonTS repo, %B engine base path, %P python, %A args from console; %% for literal %. Blocking system() call." );

	Cmd_AddCommand( "flux_generate", CL_FluxGenerate_f );
	Cmd_AddCommand( "flux_status", CL_FluxStatus_f );
	Cmd_AddCommand( "flux_cancel", CL_FluxCancel_f );
	Cmd_AddCommand( "flux_devices", CL_FluxDevices_f );
	Cmd_AddCommand( "flux_reload", CL_FluxReload_f );
	Cmd_AddCommand( "flux_show", CL_FluxShow_f );
	Cmd_AddCommand( "flux_view", CL_FluxView_f );
	Cmd_AddCommand( "fonts_pipeline", CL_FontsPipeline_f );

	Com_Memset( &flux_job, 0, sizeof( flux_job ) );
	s_trellisChainArmed = qfalse;

	if ( cl_flux_enable && cl_flux_enable->integer ) {
		Com_Printf( "FLUX image generation: enabled (device: %s, model: %s)\n",
			cl_flux_device->string, cl_flux_model->string );
		Com_Printf( "FLUX external generation: %s\n",
			cl_flux_external && cl_flux_external->integer ? "enabled" : "disabled" );
	} else {
		Com_Printf( "FLUX image generation: disabled (set cl_flux_enable 1 to enable)\n" );
	}
	if ( cl_fonts_enable && cl_fonts_enable->integer ) {
		Com_Printf( "FonTS pipeline: enabled (cl_fonts_repo %s)\n",
			( cl_fonts_repo && cl_fonts_repo->string[0] ) ? cl_fonts_repo->string : "unset — set cl_fonts_repo + cl_fonts_cmd" );
	} else {
		Com_Printf( "FonTS pipeline: disabled (cl_fonts_enable 0; docs/FONTS.md)\n" );
	}
}

void CL_Flux_Shutdown( void )
{
	if ( flux_job.status == FLUX_JOB_RUNNING ) {
		CL_MlWorker_Cancel( &s_fluxTask );
	}
	if ( flux_job.result ) {
		flux_image_free( flux_job.result );
		flux_job.result = NULL;
	}
	flux_job.status = FLUX_JOB_IDLE;
	s_trellisChainArmed = qfalse;

	Cmd_RemoveCommand( "flux_generate" );
	Cmd_RemoveCommand( "flux_status" );
	Cmd_RemoveCommand( "flux_cancel" );
	Cmd_RemoveCommand( "flux_devices" );
	Cmd_RemoveCommand( "flux_reload" );
	Cmd_RemoveCommand( "flux_show" );
	Cmd_RemoveCommand( "flux_view" );
	Cmd_RemoveCommand( "fonts_pipeline" );
}

#else

void CL_Flux_Init( void ) { }
void CL_Flux_Shutdown( void ) { }

#endif /* USE_FLUX */
