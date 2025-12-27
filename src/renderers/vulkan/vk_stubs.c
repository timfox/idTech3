
#include "tr_local.h"

// Stub global GL state expected by shared renderer code paths.
glstatic_t gls = {0};
glstate_t glState = {0};

// Stub screenshot/video capture helpers expected by shared renderer code.
void RB_TakeScreenshot(int x, int y, int width, int height, const char *fileName) {
    (void)x; (void)y; (void)width; (void)height; (void)fileName;
}

void RB_TakeScreenshotJPEG(int x, int y, int width, int height, const char *fileName) {
    (void)x; (void)y; (void)width; (void)height; (void)fileName;
}

void RB_TakeScreenshotBMP(int x, int y, int width, int height, const char *fileName, int clipboard) {
    (void)x; (void)y; (void)width; (void)height; (void)fileName; (void)clipboard;
}

const void *RB_TakeVideoFrameCmd(const void *data) {
    (void)data;
    return NULL;
}

// Stubs for CVARs that are expected by the renderer but defined in the engine
cvar_t *r_texturebits;
cvar_t *r_defaultImage;
cvar_t *r_ambientScale;
cvar_t *r_lodscale;
cvar_t *r_fbo;
cvar_t *r_vk_icd;
cvar_t *r_bloom_threshold;
cvar_t *r_glint;
cvar_t *r_materialDamage;
cvar_t *r_cellLoadRadius;
cvar_t *r_layeredMaterialSimple;
cvar_t *r_hdr;
cvar_t *r_fsr_enable;
cvar_t *r_fsr_easu;
cvar_t *r_fsr_rcas;
cvar_t *r_fsr_sharpness;
cvar_t *r_postQuality;
cvar_t *r_vk_dynamicRendering;
cvar_t *r_textureLodBias;
cvar_t *r_ext_texture_filter_anisotropic;
cvar_t *r_ext_max_anisotropy;
cvar_t *r_vk_asyncShaderCompile;
cvar_t *r_virtualTextures;
cvar_t *r_vt_pageSize;
cvar_t *r_vt_cacheSize;
cvar_t *r_vk_bindlessTextures;
cvar_t *r_procDressingDensity;
cvar_t *r_gpuSceneGraph;
cvar_t *r_gpuSceneDebug;
cvar_t *r_foliageWindFrequency;
cvar_t *r_foliageWindStrength;
cvar_t *r_procDressingDebug;
cvar_t *r_particles_gpu;
cvar_t *r_particles_max;
cvar_t *r_meshShaders;
cvar_t *r_meshletSize;
cvar_t *r_layeredMaterials;
cvar_t *r_layeredMaterialProfile;
cvar_t *r_cellUnloadDistance;
cvar_t *r_cullDistance;
cvar_t *r_layeredMaterialMaxLayers;
cvar_t *r_vk_hotReload;
cvar_t *r_materialWetness;
cvar_t *r_materialMagic;
cvar_t *r_nocurves;
cvar_t *r_presentBits;
cvar_t *r_glint_intensity;
cvar_t *r_cellLoadRadius;
cvar_t *r_glint_scale;
cvar_t *r_greyscale;
cvar_t *r_bloom_intensity;
cvar_t *r_bloom_modulate;
cvar_t *r_bloom_threshold_mode;
cvar_t *r_device;
cvar_t *r_vulkan_validation;
cvar_t *r_vk_renderdoc;

// Ray tracing CVARs
cvar_t *r_rt_denoiseSpatialAlpha;
cvar_t *r_rt_denoiseVarianceAlpha;
cvar_t *r_rt_denoiseIterations;
cvar_t *r_rt_denoise;
cvar_t *r_rt_temporal;
cvar_t *r_rt_temporalAlpha;
cvar_t *r_rt_blasCompaction;
cvar_t *r_rt_blasReuse;
cvar_t *r_rt_denoiseMode;
cvar_t *r_rt_outputScale;
cvar_t *r_rt_shadowRays;
cvar_t *r_rt_adaptiveSampling;
cvar_t *r_rt_gi;
cvar_t *r_rt_giBounces;
cvar_t *r_rt_giIntensity;

// ---------------------------------------------------------------------------
// Optional subsystems are compiled from their respective translation units.
// (No stubs needed here.)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Work-in-progress subsystems (keep stubbed until integrated)
// ---------------------------------------------------------------------------

void vk_volumetric_fog_init(void) {}
void vk_volumetric_fog_shutdown(void) {}
void vk_volumetric_fog_update(void) {}
void vk_volumetric_fog_render(void) {}

void vk_decals_init(void) {}
void vk_decals_shutdown(void) {}
void vk_decals_update(void) {}
void vk_decals_render(void) {}

void vk_god_rays_init(void) {}
void vk_god_rays_shutdown(void) {}
void vk_god_rays_update(void) {}
void vk_god_rays_render(VkCommandBuffer cmd_buffer) { (void)cmd_buffer; }

void vk_terrain_init(void) {}
void vk_terrain_shutdown(void) {}
void vk_terrain_update(void) {}
void vk_terrain_render(void) {}

void vk_surface_sprites_init(void) {}
void vk_surface_sprites_shutdown(void) {}
void vk_surface_sprites_update(void) {}
void vk_surface_sprites_render(void) {}

// Global timing
vk_gpu_timing_t vk_gpu_timing;

// Utility functions
float ByteToFloat( byte b ) {
    return b / 255.0f;
}

float sRGBtoRGB( float f ) {
    if ( f <= 0.04045f ) {
        return f / 12.92f;
    } else {
        return powf( ( f + 0.055f ) / 1.055f, 2.4f );
    }
}

byte FloatToByte( float f ) {
    return (byte)( f * 255.0f );
}

int Sys_Milliseconds( void ) {
    return 0; // Stub
}

void Perf_ResetFrameCounters( void ) {
    // Stub
}

void Perf_CountDrawCall( void ) {
    // Stub
}
