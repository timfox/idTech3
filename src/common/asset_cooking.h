/*
=============================================================================
Asset Cooking Pipeline

Automated asset processing and optimization system.
=============================================================================
*/

#ifndef __ASSET_COOKING_H__
#define __ASSET_COOKING_H__

#include "q_shared.h"

// Asset types
typedef enum {
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_MODEL,
    ASSET_TYPE_SOUND,
    ASSET_TYPE_SHADER,
    ASSET_TYPE_MATERIAL,
    ASSET_TYPE_ANIMATION,
    ASSET_TYPE_LEVEL,
    ASSET_TYPE_SCRIPT,
    ASSET_TYPE_COUNT
} asset_type_t;

// Cooking quality levels
typedef enum {
    COOK_QUALITY_POTATO,     // Minimum quality, maximum compression
    COOK_QUALITY_LOW,        // Low quality for older hardware
    COOK_QUALITY_MEDIUM,     // Balanced quality/performance
    COOK_QUALITY_HIGH,       // High quality for modern hardware
    COOK_QUALITY_ULTRA,      // Maximum quality, minimal compression
    COOK_QUALITY_COUNT
} cook_quality_t;

// Cooking platforms
typedef enum {
    COOK_PLATFORM_DESKTOP,   // Windows/Linux/macOS desktop
    COOK_PLATFORM_MOBILE,    // Android/iOS mobile devices
    COOK_PLATFORM_CONSOLE,   // Game consoles
    COOK_PLATFORM_WEB,       // Web browsers
    COOK_PLATFORM_COUNT
} cook_platform_t;

// Texture cooking options
typedef struct {
    qboolean generate_mipmaps;
    qboolean premultiply_alpha;
    qboolean compress_normal_maps;
    int max_texture_size;
    float compression_quality;
    char compression_format[16]; // DXT1, DXT5, ETC2, ASTC, etc.
} texture_cook_options_t;

// Model cooking options
typedef struct {
    qboolean generate_lod;
    int lod_levels;
    qboolean optimize_meshes;
    qboolean compress_vertices;
    qboolean compress_indices;
    float simplification_ratio;
} model_cook_options_t;

// Sound cooking options
typedef struct {
    qboolean compress_audio;
    char compression_format[16]; // MP3, OGG, ADPCM, etc.
    int sample_rate;
    int bit_depth;
    qboolean mono_conversion;
    float quality;
} sound_cook_options_t;

// Cooking job definition
typedef struct {
    char source_path[512];
    char output_path[512];
    asset_type_t asset_type;
    cook_quality_t quality;
    cook_platform_t platform;
    uint64_t source_timestamp;
    uint64_t cooked_timestamp;
    qboolean force_recook;
    void* type_specific_options; // Texture/Model/Sound options
    uint32_t dependency_count;
    char dependencies[8][256]; // Max 8 dependencies
} cook_job_t;

// Cooking statistics
typedef struct {
    uint32_t total_assets;
    uint32_t processed_assets;
    uint32_t failed_assets;
    uint64_t total_input_size;
    uint64_t total_output_size;
    uint64_t processing_time_ms;
    uint32_t assets_by_type[ASSET_TYPE_COUNT];
    uint32_t assets_by_quality[COOK_QUALITY_COUNT];
    float average_compression_ratio;
} cooking_statistics_t;

// Asset cooking pipeline
typedef struct {
    qboolean enabled;
    qboolean build_time_cooking;    // Cook at build time vs runtime
    qboolean incremental_cooking;   // Only cook changed assets
    cook_quality_t default_quality;
    cook_platform_t target_platform;
    char source_directory[512];
    char output_directory[512];
    char cache_directory[512];
    cooking_statistics_t statistics;
    uint32_t max_parallel_jobs;
} asset_cooking_pipeline_t;

extern asset_cooking_pipeline_t cooking_pipeline;

// Asset cooking API
qboolean AssetCooking_Init(const char* source_dir, const char* output_dir, const char* cache_dir);
void AssetCooking_Shutdown(void);

// Cooking job management
cook_job_t* AssetCooking_CreateJob(const char* source_path, asset_type_t type,
                                 cook_quality_t quality, cook_platform_t platform);
qboolean AssetCooking_AddJob(cook_job_t* job);
qboolean AssetCooking_ProcessJob(cook_job_t* job);
qboolean AssetCooking_ProcessAllJobs(void);
void AssetCooking_CancelJob(cook_job_t* job);

// Asset cooking functions
qboolean AssetCooking_CookTexture(cook_job_t* job);
qboolean AssetCooking_CookModel(cook_job_t* job);
qboolean AssetCooking_CookSound(cook_job_t* job);
qboolean AssetCooking_CookShader(cook_job_t* job);

// Utility functions
cook_quality_t AssetCooking_GetDefaultQuality(void);
cook_platform_t AssetCooking_DetectPlatform(void);
qboolean AssetCooking_IsAssetModified(const char* source_path, const char* cooked_path);
qboolean AssetCooking_ValidateAsset(const char* asset_path, asset_type_t type);

// Configuration
void AssetCooking_SetQuality(cook_quality_t quality);
void AssetCooking_SetPlatform(cook_platform_t platform);
void AssetCooking_SetParallelJobs(uint32_t count);
void AssetCooking_EnableIncremental(qboolean enable);

// Statistics and monitoring
void AssetCooking_GetStatistics(cooking_statistics_t* stats);
void AssetCooking_ResetStatistics(void);
void AssetCooking_PrintStatistics(void);
void AssetCooking_GenerateReport(const char* report_path);

// Cooking options factories
texture_cook_options_t* AssetCooking_CreateTextureOptions(cook_quality_t quality, cook_platform_t platform);
model_cook_options_t* AssetCooking_CreateModelOptions(cook_quality_t quality, cook_platform_t platform);
sound_cook_options_t* AssetCooking_CreateSoundOptions(cook_quality_t quality, cook_platform_t platform);

// Cache management
qboolean AssetCooking_IsCached(const char* asset_path);
qboolean AssetCooking_LoadFromCache(const char* asset_path, void** data, size_t* size);
qboolean AssetCooking_SaveToCache(const char* asset_path, const void* data, size_t size);
void AssetCooking_ClearCache(void);

// Error handling
typedef enum {
    COOK_ERROR_NONE,
    COOK_ERROR_FILE_NOT_FOUND,
    COOK_ERROR_INVALID_FORMAT,
    COOK_ERROR_COMPRESSION_FAILED,
    COOK_ERROR_OUT_OF_MEMORY,
    COOK_ERROR_PLATFORM_UNSUPPORTED,
    COOK_ERROR_DEPENDENCY_MISSING,
    COOK_ERROR_COUNT
} cook_error_t;

cook_error_t AssetCooking_GetLastError(void);
const char* AssetCooking_GetErrorString(cook_error_t error);

// Console commands
void AssetCooking_Status_f(void);
void AssetCooking_Cook_f(void);
void AssetCooking_BatchCook_f(void);
void AssetCooking_Quality_f(void);
void AssetCooking_Platform_f(void);
void AssetCooking_Cache_f(void);
void AssetCooking_Report_f(void);

#endif // __ASSET_COOKING_H__
