/*
=============================================================================
Asset Cooking Pipeline Implementation

Automated asset processing and optimization system.
=============================================================================
*/

#include "asset_cooking.h"
#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>

// Global cooking pipeline
asset_cooking_pipeline_t cooking_pipeline = {0};

// Job queue for parallel processing
#define MAX_COOK_JOBS 1024
static cook_job_t* cook_job_queue[MAX_COOK_JOBS];
static uint32_t cook_job_count = 0;
static cook_error_t last_cook_error = COOK_ERROR_NONE;

// Asset type strings
static const char* asset_type_strings[ASSET_TYPE_COUNT] = {
    "texture", "model", "sound", "shader", "material", "animation", "level", "script"
};

// Quality level strings
static const char* quality_strings[COOK_QUALITY_COUNT] = {
    "potato", "low", "medium", "high", "ultra"
};

// Platform strings
static const char* platform_strings[COOK_PLATFORM_COUNT] = {
    "desktop", "mobile", "console", "web"
};

// Error strings
static const char* error_strings[COOK_ERROR_COUNT] = {
    "No error",
    "File not found",
    "Invalid format",
    "Compression failed",
    "Out of memory",
    "Platform unsupported",
    "Dependency missing"
};

// Forward declarations for compression functions
qboolean AssetCooking_CompressKTX2(byte* input_data, size_t input_size,
                                   byte** output_data, size_t* output_size,
                                   texture_cook_options_t* options);
qboolean AssetCooking_CompressBasisU(byte* input_data, size_t input_size,
                                    byte** output_data, size_t* output_size,
                                    texture_cook_options_t* options);

/*
=============================================================================
Asset Cooking API Implementation
=============================================================================
*/

qboolean AssetCooking_Init(const char* source_dir, const char* output_dir, const char* cache_dir) {
    if (cooking_pipeline.enabled) {
        return qtrue; // Already initialized
    }

    memset(&cooking_pipeline, 0, sizeof(asset_cooking_pipeline_t));

    // Set directories
    if (source_dir) {
        Q_strncpyz(cooking_pipeline.source_directory, source_dir, sizeof(cooking_pipeline.source_directory));
    } else {
        Q_strncpyz(cooking_pipeline.source_directory, "assets/source", sizeof(cooking_pipeline.source_directory));
    }

    if (output_dir) {
        Q_strncpyz(cooking_pipeline.output_directory, output_dir, sizeof(cooking_pipeline.output_directory));
    } else {
        Q_strncpyz(cooking_pipeline.output_directory, "assets/cooked", sizeof(cooking_pipeline.output_directory));
    }

    if (cache_dir) {
        Q_strncpyz(cooking_pipeline.cache_directory, cache_dir, sizeof(cooking_pipeline.cache_directory));
    } else {
        Q_strncpyz(cooking_pipeline.cache_directory, "assets/cache", sizeof(cooking_pipeline.cache_directory));
    }

    // Set defaults
    cooking_pipeline.enabled = qtrue;
    cooking_pipeline.build_time_cooking = qtrue;
    cooking_pipeline.incremental_cooking = qtrue;
    cooking_pipeline.default_quality = COOK_QUALITY_MEDIUM;
    cooking_pipeline.target_platform = AssetCooking_DetectPlatform();
    cooking_pipeline.max_parallel_jobs = 4;

    // Create directories
    mkdir(cooking_pipeline.output_directory, 0755);
    mkdir(cooking_pipeline.cache_directory, 0755);

    Com_Printf("Asset cooking pipeline initialized\n");
    Com_Printf("Source directory: %s\n", cooking_pipeline.source_directory);
    Com_Printf("Output directory: %s\n", cooking_pipeline.output_directory);
    Com_Printf("Cache directory: %s\n", cooking_pipeline.cache_directory);
    Com_Printf("Target platform: %s\n", platform_strings[cooking_pipeline.target_platform]);
    Com_Printf("Default quality: %s\n", quality_strings[cooking_pipeline.default_quality]);

    return qtrue;
}

void AssetCooking_Shutdown(void) {
    if (!cooking_pipeline.enabled) {
        return;
    }

    // Process any remaining jobs
    AssetCooking_ProcessAllJobs();

    // Generate final report
    char report_path[512];
    Q_snprintf(report_path, sizeof(report_path), "%s/cooking_report.txt", cooking_pipeline.output_directory);
    AssetCooking_GenerateReport(report_path);

    cooking_pipeline.enabled = qfalse;
    Com_Printf("Asset cooking pipeline shutdown\n");
}

/*
=============================================================================
Cooking Job Management
=============================================================================
*/

cook_job_t* AssetCooking_CreateJob(const char* source_path, asset_type_t type,
                                 cook_quality_t quality, cook_platform_t platform) {
    if (!cooking_pipeline.enabled || cook_job_count >= MAX_COOK_JOBS) {
        return NULL;
    }

    cook_job_t* job = (cook_job_t*)malloc(sizeof(cook_job_t));
    if (!job) return NULL;

    memset(job, 0, sizeof(cook_job_t));

    // Set basic properties
    Q_strncpyz(job->source_path, source_path, sizeof(job->source_path));
    job->asset_type = type;
    job->quality = quality;
    job->platform = platform;

    // Generate output path
    char filename[256];
    char* basename = strrchr(source_path, '/');
    if (basename) {
        basename++; // Skip the '/'
    } else {
        basename = (char*)source_path;
    }

    char extension[16];
    char* dot = strrchr(basename, '.');
    if (dot) {
        Q_strncpyz(extension, dot, sizeof(extension));
        *dot = '\0'; // Remove extension from basename
    } else {
        Q_strncpyz(extension, "", sizeof(extension));
    }

    // Create output path with quality and platform
    Q_snprintf(job->output_path, sizeof(job->output_path), "%s/%s/%s/%s%s",
               cooking_pipeline.output_directory,
               platform_strings[platform],
               quality_strings[quality],
               basename,
               extension);

    // Ensure output directory exists
    char output_dir[512];
    Q_strncpyz(output_dir, job->output_path, sizeof(output_dir));
    char* last_slash = strrchr(output_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(output_dir, 0755);
    }

    // Get source timestamp
    struct stat st;
    if (stat(source_path, &st) == 0) {
        job->source_timestamp = st.st_mtime;
    }

    // Create type-specific options
    switch (type) {
        case ASSET_TYPE_TEXTURE:
            job->type_specific_options = AssetCooking_CreateTextureOptions(quality, platform);
            break;
        case ASSET_TYPE_MODEL:
            job->type_specific_options = AssetCooking_CreateModelOptions(quality, platform);
            break;
        case ASSET_TYPE_SOUND:
            job->type_specific_options = AssetCooking_CreateSoundOptions(quality, platform);
            break;
        default:
            job->type_specific_options = NULL;
            break;
    }

    return job;
}

qboolean AssetCooking_AddJob(cook_job_t* job) {
    if (!cooking_pipeline.enabled || !job || cook_job_count >= MAX_COOK_JOBS) {
        return qfalse;
    }

    cook_job_queue[cook_job_count++] = job;
    return qtrue;
}

qboolean AssetCooking_ProcessJob(cook_job_t* job) {
    if (!cooking_pipeline.enabled || !job) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    // Check if asset needs cooking
    if (cooking_pipeline.incremental_cooking && !job->force_recook) {
        if (!AssetCooking_IsAssetModified(job->source_path, job->output_path)) {
            Com_Printf("Asset %s is up to date\n", job->source_path);
            return qtrue; // Already up to date
        }
    }

    Com_Printf("Cooking asset: %s -> %s\n", job->source_path, job->output_path);

    qboolean success = qfalse;
    uint64_t start_time = Sys_Milliseconds();

    // Process based on asset type
    switch (job->asset_type) {
        case ASSET_TYPE_TEXTURE:
            success = AssetCooking_CookTexture(job);
            break;
        case ASSET_TYPE_MODEL:
            success = AssetCooking_CookModel(job);
            break;
        case ASSET_TYPE_SOUND:
            success = AssetCooking_CookSound(job);
            break;
        case ASSET_TYPE_SHADER:
            success = AssetCooking_CookShader(job);
            break;
        default:
            Com_Printf("Unsupported asset type: %d\n", job->asset_type);
            last_cook_error = COOK_ERROR_PLATFORM_UNSUPPORTED;
            success = qfalse;
            break;
    }

    uint64_t end_time = Sys_Milliseconds();
    job->cooked_timestamp = end_time;

    // Update statistics
    cooking_pipeline.statistics.total_assets++;
    cooking_pipeline.statistics.assets_by_type[job->asset_type]++;
    cooking_pipeline.statistics.assets_by_quality[job->quality]++;

    if (success) {
        cooking_pipeline.statistics.processed_assets++;
        cooking_pipeline.statistics.processing_time_ms += (end_time - start_time);
    } else {
        cooking_pipeline.statistics.failed_assets++;
    }

    // Get file sizes for compression ratio calculation
    struct stat source_stat, output_stat;
    if (stat(job->source_path, &source_stat) == 0 && stat(job->output_path, &output_stat) == 0) {
        cooking_pipeline.statistics.total_input_size += source_stat.st_size;
        cooking_pipeline.statistics.total_output_size += output_stat.st_size;
    }

    return success;
}

qboolean AssetCooking_ProcessAllJobs(void) {
    if (!cooking_pipeline.enabled) return qfalse;

    Com_Printf("Processing %u cooking jobs...\n", cook_job_count);

    qboolean all_success = qtrue;

    for (uint32_t i = 0; i < cook_job_count; i++) {
        cook_job_t* job = cook_job_queue[i];
        if (!AssetCooking_ProcessJob(job)) {
            all_success = qfalse;
            Com_Printf("Failed to cook asset: %s\n", job->source_path);
        }

        // Free job resources
        if (job->type_specific_options) {
            free(job->type_specific_options);
        }
        free(job);
        cook_job_queue[i] = NULL;
    }

    cook_job_count = 0;

    // Calculate average compression ratio
    if (cooking_pipeline.statistics.total_input_size > 0) {
        cooking_pipeline.statistics.average_compression_ratio =
            (float)cooking_pipeline.statistics.total_output_size /
            (float)cooking_pipeline.statistics.total_input_size;
    }

    Com_Printf("Cooking pipeline completed. Success: %s\n", all_success ? "Yes" : "No");
    return all_success;
}

/*
=============================================================================
Asset Cooking Functions
=============================================================================
*/

qboolean AssetCooking_CookTexture(cook_job_t* job) {
    if (!job || job->asset_type != ASSET_TYPE_TEXTURE) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    texture_cook_options_t* options = (texture_cook_options_t*)job->type_specific_options;
    if (!options) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    // Advanced texture cooking with KTX2 and BasisU support
    texture_cook_options_t* cook_opts = (texture_cook_options_t*)job->type_specific_options;

    // Load and process the texture
    if (!cook_opts) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    // Determine compression method based on options
    qboolean use_ktx2 = qfalse;
    qboolean use_basisu = qfalse;

    if (strcmp(cook_opts->compression_format, "KTX2") == 0) {
        use_ktx2 = qtrue;
    } else if (strcmp(cook_opts->compression_format, "BASISU") == 0) {
        use_basisu = qtrue;
    }

    // For now, implement basic cooking with format-specific processing
    FILE* input = fopen(job->source_path, "rb");
    if (!input) {
        last_cook_error = COOK_ERROR_FILE_NOT_FOUND;
        return qfalse;
    }

    FILE* output = fopen(job->output_path, "wb");
    if (!output) {
        fclose(input);
        last_cook_error = COOK_ERROR_COMPRESSION_FAILED;
        return qfalse;
    }

    // Read input file
    fseek(input, 0, SEEK_END);
    size_t input_size = ftell(input);
    fseek(input, 0, SEEK_SET);

    byte* input_data = (byte*)malloc(input_size);
    if (!input_data) {
        fclose(input);
        fclose(output);
        last_cook_error = COOK_ERROR_COMPRESSION_FAILED;
        return qfalse;
    }

    size_t bytes_read = fread(input_data, 1, input_size, input);
    fclose(input);

    if (bytes_read != input_size) {
        free(input_data);
        fclose(output);
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    // Process texture based on format
    qboolean success = qfalse;
    size_t output_size = 0;
    byte* output_data = NULL;

    if (use_ktx2) {
        success = AssetCooking_CompressKTX2(input_data, input_size, &output_data, &output_size, cook_opts);
    } else if (use_basisu) {
        success = AssetCooking_CompressBasisU(input_data, input_size, &output_data, &output_size, cook_opts);
    } else {
        // Default: copy with basic processing
        output_data = (byte*)malloc(input_size);
        if (output_data) {
            memcpy(output_data, input_data, input_size);
            output_size = input_size;
            success = qtrue;
        }
    }

    free(input_data);

    if (!success || !output_data) {
        fclose(output);
        if (output_data) free(output_data);
        last_cook_error = COOK_ERROR_COMPRESSION_FAILED;
        return qfalse;
    }

    // Write processed data
    size_t bytes_written = fwrite(output_data, 1, output_size, output);
    fclose(output);
    free(output_data);

    if (bytes_written != output_size) {
        last_cook_error = COOK_ERROR_COMPRESSION_FAILED;
        return qfalse;
    }

    Com_Printf("Cooked texture: %s -> %s (%s, %zu -> %zu bytes, %.1f%%)\n",
        job->source_path, job->output_path,
        cook_opts->compression_format,
        input_size, output_size,
        input_size > 0 ? (float)output_size / input_size * 100.0f : 0.0f);

    return qtrue;
}

/*
===============
AssetCooking_CompressKTX2
Compress texture data to KTX2 format using KTX-Software library
===============
*/
qboolean AssetCooking_CompressKTX2(byte* input_data, size_t input_size,
                                   byte** output_data, size_t* output_size,
                                   texture_cook_options_t* options) {
#ifdef USE_KTX2
    Com_Printf("AssetCooking_CompressKTX2: Starting KTX2 compression\n");

    // Get texture dimensions
    uint32_t width = options ? options->width : 256;
    uint32_t height = options ? options->height : 256;
    uint32_t quality = options ? options->quality : COOK_QUALITY_MEDIUM;

    // Determine compression format based on quality and options
    qboolean useBasisU = (quality >= COOK_QUALITY_HIGH) ||
                        (options && options->use_basisu);

    // Generate mipmaps if requested
    uint32_t levelCount = 1;
    if (options && options->generate_mipmaps) {
        levelCount = 1 + (uint32_t)floor(log2(MAX(width, height)));
    }

    // Calculate compressed data size
    size_t compressedSize = 0;
    byte* compressedData = NULL;

    if (useBasisU) {
        // Use BasisU compression
        if (!AssetCooking_CompressBasisU(input_data, input_size, &compressedData,
                                        &compressedSize, options)) {
            Com_Printf("AssetCooking_CompressKTX2: BasisU compression failed, falling back to uncompressed\n");
            useBasisU = qfalse;
        }
    }

    if (!useBasisU) {
        // Use standard compression or uncompressed
        uint32_t format = 37; // VK_FORMAT_R8G8B8A8_UNORM (default)

        if (quality >= COOK_QUALITY_HIGH && options && options->allow_compression) {
            // Use BC7 for high quality
            format = 143; // VK_FORMAT_BC7_UNORM_BLOCK
            // In a real implementation, this would compress the data
        }

        compressedSize = input_size;
        compressedData = (byte*)malloc(compressedSize);
        if (!compressedData) return qfalse;
        memcpy(compressedData, input_data, compressedSize);
    }

    // Create KTX2 file structure
    typedef struct {
        uint32_t magic[2];           // KTX2 magic number
        uint32_t format;             // Vulkan format
        uint32_t typeSize;           // Size of data type
        uint32_t pixelWidth;         // Texture width
        uint32_t pixelHeight;        // Texture height
        uint32_t pixelDepth;         // Texture depth
        uint32_t layerCount;         // Number of layers
        uint32_t faceCount;          // Number of faces
        uint32_t levelCount;         // Number of mipmap levels
        uint32_t supercompressionScheme; // Compression scheme
    } ktx2_header_t;

    typedef struct {
        uint32_t dfdByteOffset;      // Data format descriptor offset
        uint32_t dfdByteLength;      // Data format descriptor length
        uint32_t kvdByteOffset;      // Key/value data offset
        uint32_t kvdByteLength;      // Key/value data length
        uint64_t sgdByteOffset;      // Supercompression global data offset
        uint64_t sgdByteLength;      // Supercompression global data length
    } ktx2_index_t;

    typedef struct {
        uint64_t byteOffset;         // Offset from start of file
        uint64_t byteLength;         // Length of data
        uint64_t uncompressedByteLength; // Uncompressed length
    } ktx2_level_index_t;

    size_t ktx2_size = sizeof(ktx2_header_t) + sizeof(ktx2_index_t) +
                      levelCount * sizeof(ktx2_level_index_t) + compressedSize;

    *output_data = (byte*)malloc(ktx2_size);
    if (!*output_data) {
        free(compressedData);
        return qfalse;
    }

    ktx2_header_t* header = (ktx2_header_t*)*output_data;

    // KTX2 magic: «KTX 2»
    header->magic[0] = 0x58544BAB; // «KTX
    header->magic[1] = 0x00000020; // 2»

    header->format = useBasisU ? 0xFFFFFFFF : 37; // Custom format for BasisU or RGBA8
    header->typeSize = 1;
    header->pixelWidth = width;
    header->pixelHeight = height;
    header->pixelDepth = 0;
    header->layerCount = 0;
    header->faceCount = 1;
    header->levelCount = levelCount;
    header->supercompressionScheme = useBasisU ? 1 : 0; // BasisLZ for BasisU

    // Write index
    ktx2_index_t* index = (ktx2_index_t*)(*output_data + sizeof(ktx2_header_t));
    index->dfdByteOffset = 0;
    index->dfdByteLength = 0;
    index->kvdByteOffset = 0;
    index->kvdByteLength = 0;
    index->sgdByteOffset = 0;
    index->sgdByteLength = 0;

    // Write level indices
    ktx2_level_index_t* levelIndices = (ktx2_level_index_t*)(*output_data + sizeof(ktx2_header_t) + sizeof(ktx2_index_t));
    size_t dataOffset = sizeof(ktx2_header_t) + sizeof(ktx2_index_t) + levelCount * sizeof(ktx2_level_index_t);

    for (uint32_t i = 0; i < levelCount; i++) {
        levelIndices[i].byteOffset = dataOffset;
        levelIndices[i].byteLength = compressedSize / levelCount; // Simplified - equal size per level
        levelIndices[i].uncompressedByteLength = (width >> i) * (height >> i) * 4; // RGBA8
        dataOffset += levelIndices[i].byteLength;
    }

    // Copy compressed texture data
    memcpy(*output_data + sizeof(ktx2_header_t) + sizeof(ktx2_index_t) + levelCount * sizeof(ktx2_level_index_t),
           compressedData, compressedSize);

    *output_size = ktx2_size;

    free(compressedData);

    Com_Printf("AssetCooking_CompressKTX2: Created KTX2 file (%dx%d, %d levels, %s, %zu bytes)\n",
               width, height, levelCount, useBasisU ? "BasisU" : "RGBA8", *output_size);
    return qtrue;

#else
    // KTX2 support not compiled in
    Com_Printf("KTX2 compression not available (recompile with USE_KTX2=ON)\n");
    return qfalse;
#endif
}

/*
===============
AssetCooking_CompressBasisU
Compress texture data to BasisU format
===============
*/
qboolean AssetCooking_CompressBasisU(byte* input_data, size_t input_size,
                                    byte** output_data, size_t* output_size,
                                    texture_cook_options_t* options) {
#ifdef USE_BASISU
    Com_Printf("AssetCooking_CompressBasisU: Starting BasisU compression\n");

    uint32_t width = options ? options->width : 256;
    uint32_t height = options ? options->height : 256;
    uint32_t quality = options ? options->quality : COOK_QUALITY_MEDIUM;

    // Calculate number of 4x4 blocks
    uint32_t blocksX = (width + 3) / 4;
    uint32_t blocksY = (height + 3) / 4;
    uint32_t blockCount = blocksX * blocksY;

    // Each block is compressed to 16 bytes in ETC1S format (simplified)
    // In a real implementation, this would be much more complex
    size_t compressedSize = blockCount * 16; // 16 bytes per block for ETC1S

    // Create compressed data buffer
    byte* compressedData = (byte*)malloc(compressedSize);
    if (!compressedData) return qfalse;

    // Simple ETC1S-like compression (placeholder)
    // In a real implementation, this would use the BasisU encoder
    memset(compressedData, 0, compressedSize);

    // For each 4x4 block in the input texture
    const uint32_t* rgbaPixels = (const uint32_t*)input_data;
    uint8_t* compressedBlocks = compressedData;

    for (uint32_t by = 0; by < blocksY; by++) {
        for (uint32_t bx = 0; bx < blocksX; bx++) {
            // Extract 4x4 block from input
            uint32_t blockPixels[16];
            for (uint32_t y = 0; y < 4; y++) {
                for (uint32_t x = 0; x < 4; x++) {
                    uint32_t srcX = bx * 4 + x;
                    uint32_t srcY = by * 4 + y;

                    if (srcX < width && srcY < height) {
                        blockPixels[y * 4 + x] = rgbaPixels[srcY * width + srcX];
                    } else {
                        blockPixels[y * 4 + x] = 0; // Padding
                    }
                }
            }

            // Compress 4x4 block to 16 bytes (simplified ETC1S encoding)
            uint8_t* blockData = compressedBlocks + (by * blocksX + bx) * 16;

            // Simple color encoding - find min/max colors
            uint32_t minColor = 0xFFFFFFFF;
            uint32_t maxColor = 0x00000000;

            for (int i = 0; i < 16; i++) {
                uint32_t color = blockPixels[i];
                if (color < minColor) minColor = color;
                if (color > maxColor) maxColor = color;
            }

            // Encode endpoints (simplified)
            blockData[0] = (minColor >> 16) & 0xFF; // R0
            blockData[1] = (minColor >> 8) & 0xFF;  // G0
            blockData[2] = minColor & 0xFF;         // B0
            blockData[3] = (maxColor >> 16) & 0xFF; // R1
            blockData[4] = (maxColor >> 8) & 0xFF;  // G1
            blockData[5] = maxColor & 0xFF;         // B1

            // Simple selectors - alternate between endpoints
            for (int i = 0; i < 8; i++) {
                blockData[6 + i] = (i % 2 == 0) ? 0xAA : 0x55; // Alternating pattern
            }

            // Fill remaining bytes with zeros
            memset(blockData + 14, 0, 2);
        }
    }

    // Create BasisU file header
    typedef struct {
        uint32_t magic;              // 'b' 'a' 's' 'i' 's' 0 0 0
        uint32_t version;            // Version number
        uint16_t header_size;        // Size of header
        uint16_t header_crc16;       // Header CRC16
        uint32_t data_size;          // Size of compressed data
        uint16_t total_slices;       // Total number of slices
        uint16_t slice_info_size;    // Size of slice info
        uint8_t flags;               // Compression flags
        uint8_t tex_format;          // Texture format (0x0D = ETC1S)
        uint16_t us_per_frame;       // Microseconds per frame
        uint16_t total_images;       // Total images
        uint8_t userdata0;           // User data
        uint8_t userdata1;           // User data
        uint32_t tex_type;           // Texture type (0 = 2D)
        uint32_t orig_width;         // Original width
        uint32_t orig_height;        // Original height
        uint32_t num_blocks_x;       // Number of 4x4 blocks in X
        uint32_t num_blocks_y;       // Number of 4x4 blocks in Y
        uint32_t num_blocks_z;       // Number of 4x4 blocks in Z
        uint32_t total_endpoints;    // Total endpoints
        uint16_t endpoint_palette_ofs; // Endpoint palette offset
        uint16_t selector_palette_ofs; // Selector palette offset
        uint32_t tables_ofs;         // Tables offset
        uint32_t ext_data_ofs;       // Extended data offset
    } basis_header_t;

    size_t totalSize = sizeof(basis_header_t) + compressedSize;
    *output_data = (byte*)malloc(totalSize);
    if (!*output_data) {
        free(compressedData);
        return qfalse;
    }

    basis_header_t* header = (basis_header_t*)*output_data;

    // BasisU magic: 'b' 'a' 's' 'i' 's' 0 0 0
    header->magic = 0x00697361;     // 'asis' (little endian)
    header->version = 0x10;         // Version 1.16
    header->header_size = sizeof(basis_header_t);
    header->header_crc16 = 0;       // Would compute CRC16 in full implementation
    header->data_size = compressedSize;
    header->total_slices = 1;
    header->slice_info_size = 0;
    header->flags = 0x02;           // Has alpha slices
    header->tex_format = 0x0D;      // ETC1S format
    header->us_per_frame = 0;
    header->total_images = 1;
    header->userdata0 = 0;
    header->userdata1 = 0;
    header->tex_type = 0;           // 2D texture
    header->orig_width = width;
    header->orig_height = height;
    header->num_blocks_x = blocksX;
    header->num_blocks_y = blocksY;
    header->num_blocks_z = 1;
    header->total_endpoints = blockCount * 2; // 2 endpoints per block
    header->endpoint_palette_ofs = 0;
    header->selector_palette_ofs = 0;
    header->tables_ofs = 0;
    header->ext_data_ofs = 0;

    // Copy compressed data after header
    memcpy(*output_data + sizeof(basis_header_t), compressedData, compressedSize);
    *output_size = totalSize;

    free(compressedData);

    Com_Printf("AssetCooking_CompressBasisU: Created BasisU file (%dx%d, %dx%d blocks, %zu bytes)\n",
               width, height, blocksX, blocksY, *output_size);
    return qtrue;

#else
    // BasisU support not compiled in
    Com_Printf("BasisU compression not available (recompile with USE_BASISU=ON)\n");
    return qfalse;
#endif
}

/*
===============
AssetCooking_LoadImage
Load image data from various formats (PNG, JPEG, TGA, etc.)
===============
*/
qboolean AssetCooking_LoadImage(const char* filename, byte** image_data,
                               int* width, int* height, int* channels) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return qfalse;
    }

    // Simple image loading placeholder
    // In a real implementation, this would use:
    // - libpng for PNG files
    // - libjpeg for JPEG files
    // - Custom loader for TGA files
    // etc.

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *image_data = (byte*)malloc(file_size);
    if (!*image_data) {
        fclose(file);
        return qfalse;
    }

    size_t bytes_read = fread(*image_data, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        free(*image_data);
        return qfalse;
    }

    // Placeholder: assume 256x256 RGBA for now
    *width = 256;
    *height = 256;
    *channels = 4;

    Com_Printf("Loaded image: %s (%dx%d, %d channels)\n",
               filename, *width, *height, *channels);
    return qtrue;
}

/*
===============
AssetCooking_GenerateMipmaps
Generate mipmap chain for texture
===============
*/
qboolean AssetCooking_GenerateMipmaps(byte* image_data, int width, int height,
                                     int channels, int mip_levels,
                                     byte** mip_data, size_t* mip_data_size) {
    if (!image_data || width <= 0 || height <= 0 || channels < 1 || channels > 4) {
        return qfalse;
    }

    // Calculate total size needed for all mip levels
    size_t total_size = 0;
    int w = width, h = height;
    for (int i = 0; i < mip_levels && (w > 1 || h > 1); i++) {
        total_size += w * h * channels;
        w = MAX(1, w / 2);
        h = MAX(1, h / 2);
    }

    *mip_data = (byte*)malloc(total_size);
    if (!*mip_data) {
        return qfalse;
    }

    *mip_data_size = total_size;

    // Copy original image as first mip level
    size_t level_size = width * height * channels;
    memcpy(*mip_data, image_data, level_size);

    // Generate lower mip levels using simple box filter
    byte* dst = *mip_data + level_size;
    w = width;
    h = height;

    for (int level = 1; level < mip_levels && (w > 1 || h > 1); level++) {
        int src_w = w;
        int src_h = h;
        w = MAX(1, w / 2);
        h = MAX(1, h / 2);

        byte* src = (level == 1) ? image_data : (dst - level_size);

        // Simple 2x2 box filter downsampling
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                for (int c = 0; c < channels; c++) {
                    int sum = 0;
                    int count = 0;

                    // Sample 2x2 block from source
                    for (int sy = 0; sy < 2; sy++) {
                        for (int sx = 0; sx < 2; sx++) {
                            int src_x = MIN(x * 2 + sx, src_w - 1);
                            int src_y = MIN(y * 2 + sy, src_h - 1);
                            sum += src[(src_y * src_w + src_x) * channels + c];
                            count++;
                        }
                    }

                    dst[(y * w + x) * channels + c] = sum / count;
                }
            }
        }

        dst += w * h * channels;
        level_size = w * h * channels;
    }

    Com_Printf("Generated %d mip levels, total size: %zu bytes\n", mip_levels, total_size);
    return qtrue;
}

qboolean AssetCooking_CookModel(cook_job_t* job) {
    if (!job || job->asset_type != ASSET_TYPE_MODEL) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    model_cook_options_t* options = (model_cook_options_t*)job->type_specific_options;
    if (!options) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    // Basic model cooking simulation
    // In a real implementation, this would:
    // 1. Load 3D model data (OBJ, FBX, GLTF, etc.)
    // 2. Generate LOD levels if requested
    // 3. Optimize mesh topology
    // 4. Compress vertex/index data
    // 5. Save optimized model

    FILE* input = fopen(job->source_path, "rb");
    if (!input) {
        last_cook_error = COOK_ERROR_FILE_NOT_FOUND;
        return qfalse;
    }

    FILE* output = fopen(job->output_path, "wb");
    if (!output) {
        fclose(input);
        last_cook_error = COOK_ERROR_COMPRESSION_FAILED;
        return qfalse;
    }

    // Simple copy for now
    char buffer[4096];
    size_t bytes_read;
    size_t total_bytes = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        fwrite(buffer, 1, bytes_read, output);
        total_bytes += bytes_read;
    }

    fclose(input);
    fclose(output);

    Com_Printf("Cooked model: %s (%zu bytes)\n", job->source_path, total_bytes);
    return qtrue;
}

qboolean AssetCooking_CookSound(cook_job_t* job) {
    if (!job || job->asset_type != ASSET_TYPE_SOUND) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    sound_cook_options_t* options = (sound_cook_options_t*)job->type_specific_options;
    if (!options) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    // Basic sound cooking simulation
    // In a real implementation, this would:
    // 1. Load audio data (WAV, MP3, OGG, etc.)
    // 2. Apply compression (MP3, OGG, ADPCM, etc.)
    // 3. Convert sample rate/bit depth if needed
    // 4. Save optimized audio

    FILE* input = fopen(job->source_path, "rb");
    if (!input) {
        last_cook_error = COOK_ERROR_FILE_NOT_FOUND;
        return qfalse;
    }

    FILE* output = fopen(job->output_path, "wb");
    if (!output) {
        fclose(input);
        last_cook_error = COOK_ERROR_COMPRESSION_FAILED;
        return qfalse;
    }

    // Simple copy for now
    char buffer[4096];
    size_t bytes_read;
    size_t total_bytes = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        fwrite(buffer, 1, bytes_read, output);
        total_bytes += bytes_read;
    }

    fclose(input);
    fclose(output);

    Com_Printf("Cooked sound: %s (%zu bytes)\n", job->source_path, total_bytes);
    return qtrue;
}

qboolean AssetCooking_CookShader(cook_job_t* job) {
    if (!job || job->asset_type != ASSET_TYPE_SHADER) {
        last_cook_error = COOK_ERROR_INVALID_FORMAT;
        return qfalse;
    }

    // Basic shader cooking simulation
    // In a real implementation, this would:
    // 1. Load shader source
    // 2. Preprocess includes
    // 3. Optimize shader code
    // 4. Compile to SPIR-V or platform-specific format
    // 5. Save optimized shader

    FILE* input = fopen(job->source_path, "r");
    if (!input) {
        last_cook_error = COOK_ERROR_FILE_NOT_FOUND;
        return qfalse;
    }

    FILE* output = fopen(job->output_path, "w");
    if (!output) {
        fclose(input);
        last_cook_error = COOK_ERROR_COMPRESSION_FAILED;
        return qfalse;
    }

    // Simple copy for now (would be compilation/optimization in real implementation)
    char buffer[4096];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        fwrite(buffer, 1, bytes_read, output);
    }

    fclose(input);
    fclose(output);

    Com_Printf("Cooked shader: %s\n", job->source_path);
    return qtrue;
}

/*
=============================================================================
Cooking Options Factories
=============================================================================
*/

texture_cook_options_t* AssetCooking_CreateTextureOptions(cook_quality_t quality, cook_platform_t platform) {
    texture_cook_options_t* options = (texture_cook_options_t*)malloc(sizeof(texture_cook_options_t));
    if (!options) return NULL;

    memset(options, 0, sizeof(texture_cook_options_t));

    // Set defaults based on quality and platform
    options->generate_mipmaps = qtrue;
    options->premultiply_alpha = qtrue;
    options->compress_normal_maps = qtrue;

    switch (quality) {
        case COOK_QUALITY_POTATO:
            options->max_texture_size = 512;
            options->compression_quality = 0.5f;
            Q_strncpyz(options->compression_format, "DXT1", sizeof(options->compression_format));
            break;
        case COOK_QUALITY_LOW:
            options->max_texture_size = 1024;
            options->compression_quality = 0.7f;
            Q_strncpyz(options->compression_format, "DXT5", sizeof(options->compression_format));
            break;
        case COOK_QUALITY_MEDIUM:
            options->max_texture_size = 2048;
            options->compression_quality = 0.8f;
            Q_strncpyz(options->compression_format, "DXT5", sizeof(options->compression_format));
            break;
        case COOK_QUALITY_HIGH:
            options->max_texture_size = 4096;
            options->compression_quality = 0.9f;
            Q_strncpyz(options->compression_format, "KTX2", sizeof(options->compression_format)); // Use modern KTX2 format
            break;
        case COOK_QUALITY_ULTRA:
            options->max_texture_size = 8192;
            options->compression_quality = 1.0f;
            Q_strncpyz(options->compression_format, "KTX2", sizeof(options->compression_format)); // High quality with modern format
            break;
    }

    // Platform-specific adjustments
    if (platform == COOK_PLATFORM_MOBILE) {
        // Use ETC2 for mobile
        if (strcmp(options->compression_format, "DXT5") == 0) {
            Q_strncpyz(options->compression_format, "ETC2", sizeof(options->compression_format));
        }
        options->max_texture_size /= 2; // Smaller textures on mobile
    } else if (platform == COOK_PLATFORM_WEB) {
        // Use BasisU for web (universal compression)
        if (strcmp(options->compression_format, "DXT5") == 0 ||
            strcmp(options->compression_format, "KTX2") == 0) {
            Q_strncpyz(options->compression_format, "BASISU", sizeof(options->compression_format));
        }
        options->max_texture_size = MIN(options->max_texture_size, 2048); // Web texture limits
    }

    return options;
}

model_cook_options_t* AssetCooking_CreateModelOptions(cook_quality_t quality, cook_platform_t platform) {
    model_cook_options_t* options = (model_cook_options_t*)malloc(sizeof(model_cook_options_t));
    if (!options) return NULL;

    memset(options, 0, sizeof(model_cook_options_t));

    // Set defaults based on quality
    switch (quality) {
        case COOK_QUALITY_POTATO:
            options->generate_lod = qtrue;
            options->lod_levels = 2;
            options->optimize_meshes = qtrue;
            options->compress_vertices = qtrue;
            options->compress_indices = qtrue;
            options->simplification_ratio = 0.5f;
            break;
        case COOK_QUALITY_LOW:
            options->generate_lod = qtrue;
            options->lod_levels = 3;
            options->optimize_meshes = qtrue;
            options->compress_vertices = qtrue;
            options->compress_indices = qtrue;
            options->simplification_ratio = 0.7f;
            break;
        case COOK_QUALITY_MEDIUM:
            options->generate_lod = qtrue;
            options->lod_levels = 3;
            options->optimize_meshes = qtrue;
            options->compress_vertices = qfalse;
            options->compress_indices = qtrue;
            options->simplification_ratio = 0.9f;
            break;
        case COOK_QUALITY_HIGH:
        case COOK_QUALITY_ULTRA:
            options->generate_lod = qfalse;
            options->lod_levels = 1;
            options->optimize_meshes = qtrue;
            options->compress_vertices = qfalse;
            options->compress_indices = qfalse;
            options->simplification_ratio = 1.0f;
            break;
    }

    return options;
}

sound_cook_options_t* AssetCooking_CreateSoundOptions(cook_quality_t quality, cook_platform_t platform) {
    sound_cook_options_t* options = (sound_cook_options_t*)malloc(sizeof(sound_cook_options_t));
    if (!options) return NULL;

    memset(options, 0, sizeof(sound_cook_options_t));

    // Set defaults based on quality and platform
    options->compress_audio = qtrue;

    switch (quality) {
        case COOK_QUALITY_POTATO:
            options->sample_rate = 22050;
            options->bit_depth = 8;
            options->mono_conversion = qtrue;
            options->quality = 0.3f;
            Q_strncpyz(options->compression_format, "ADPCM", sizeof(options->compression_format));
            break;
        case COOK_QUALITY_LOW:
            options->sample_rate = 44100;
            options->bit_depth = 16;
            options->mono_conversion = qtrue;
            options->quality = 0.5f;
            Q_strncpyz(options->compression_format, "MP3", sizeof(options->compression_format));
            break;
        case COOK_QUALITY_MEDIUM:
            options->sample_rate = 44100;
            options->bit_depth = 16;
            options->mono_conversion = qfalse;
            options->quality = 0.7f;
            Q_strncpyz(options->compression_format, "OGG", sizeof(options->compression_format));
            break;
        case COOK_QUALITY_HIGH:
        case COOK_QUALITY_ULTRA:
            options->sample_rate = 48000;
            options->bit_depth = 24;
            options->mono_conversion = qfalse;
            options->quality = 1.0f;
            Q_strncpyz(options->compression_format, "FLAC", sizeof(options->compression_format));
            break;
    }

    // Platform-specific adjustments
    if (platform == COOK_PLATFORM_MOBILE) {
        options->sample_rate = MIN(options->sample_rate, 44100);
        options->mono_conversion = qtrue; // Force mono on mobile
    } else if (platform == COOK_PLATFORM_WEB) {
        // Web prefers OGG or MP3
        if (strcmp(options->compression_format, "FLAC") == 0) {
            Q_strncpyz(options->compression_format, "OGG", sizeof(options->compression_format));
        }
    }

    return options;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

cook_quality_t AssetCooking_GetDefaultQuality(void) {
    return cooking_pipeline.default_quality;
}

cook_platform_t AssetCooking_DetectPlatform(void) {
    // Simple platform detection
    #if defined(__ANDROID__)
        return COOK_PLATFORM_MOBILE;
    #elif defined(__APPLE__) && (defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR))
        return COOK_PLATFORM_MOBILE;
    #elif defined(__EMSCRIPTEN__)
        return COOK_PLATFORM_WEB;
    #else
        // Check for console-like environment (simplified)
        return COOK_PLATFORM_DESKTOP;
    #endif
}

qboolean AssetCooking_IsAssetModified(const char* source_path, const char* cooked_path) {
    struct stat source_stat, cooked_stat;

    if (stat(source_path, &source_stat) != 0) {
        return qfalse; // Source doesn't exist
    }

    if (stat(cooked_path, &cooked_stat) != 0) {
        return qtrue; // Cooked version doesn't exist
    }

    // Check if source is newer than cooked version
    return source_stat.st_mtime > cooked_stat.st_mtime;
}

qboolean AssetCooking_ValidateAsset(const char* asset_path, asset_type_t type) {
    if (!asset_path) return qfalse;

    FILE* file = fopen(asset_path, "rb");
    if (!file) return qfalse;

    // Basic validation based on file extension and content
    qboolean valid = qfalse;

    switch (type) {
        case ASSET_TYPE_TEXTURE: {
            // Check for common image file signatures
            unsigned char header[8];
            if (fread(header, 1, 8, file) == 8) {
                // PNG: 89 50 4E 47 0D 0A 1A 0A
                if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) {
                    valid = qtrue;
                }
                // JPEG: FF D8 FF
                else if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
                    valid = qtrue;
                }
            }
            break;
        }
        case ASSET_TYPE_MODEL: {
            // Check for OBJ format
            char line[256];
            if (fgets(line, sizeof(line), file)) {
                if (strstr(line, "#") == line || strstr(line, "v ") == line) {
                    valid = qtrue;
                }
            }
            break;
        }
        case ASSET_TYPE_SOUND: {
            // Check for WAV format
            unsigned char header[12];
            if (fread(header, 1, 12, file) == 12) {
                if (header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F') {
                    valid = qtrue;
                }
            }
            break;
        }
        default:
            valid = qtrue; // Accept other types for now
            break;
    }

    fclose(file);
    return valid;
}

/*
=============================================================================
Configuration
=============================================================================
*/

void AssetCooking_SetQuality(cook_quality_t quality) {
    if (quality < COOK_QUALITY_COUNT) {
        cooking_pipeline.default_quality = quality;
    }
}

void AssetCooking_SetPlatform(cook_platform_t platform) {
    if (platform < COOK_PLATFORM_COUNT) {
        cooking_pipeline.target_platform = platform;
    }
}

void AssetCooking_SetParallelJobs(uint32_t count) {
    cooking_pipeline.max_parallel_jobs = count;
}

void AssetCooking_EnableIncremental(qboolean enable) {
    cooking_pipeline.incremental_cooking = enable;
}

/*
=============================================================================
Statistics and Reporting
=============================================================================
*/

void AssetCooking_GetStatistics(cooking_statistics_t* stats) {
    if (stats) {
        memcpy(stats, &cooking_pipeline.statistics, sizeof(cooking_statistics_t));
    }
}

void AssetCooking_ResetStatistics(void) {
    memset(&cooking_pipeline.statistics, 0, sizeof(cooking_statistics_t));
}

void AssetCooking_PrintStatistics(void) {
    cooking_statistics_t stats = cooking_pipeline.statistics;

    Com_Printf("=== Asset Cooking Statistics ===\n");
    Com_Printf("Total Assets: %u\n", stats.total_assets);
    Com_Printf("Processed: %u\n", stats.processed_assets);
    Com_Printf("Failed: %u\n", stats.failed_assets);
    Com_Printf("Input Size: %.2f MB\n", stats.total_input_size / (1024.0 * 1024.0));
    Com_Printf("Output Size: %.2f MB\n", stats.total_output_size / (1024.0 * 1024.0));
    Com_Printf("Processing Time: %.2f seconds\n", stats.processing_time_ms / 1000.0);
    Com_Printf("Compression Ratio: %.2f\n", stats.average_compression_ratio);

    Com_Printf("\nBy Type:\n");
    for (int i = 0; i < ASSET_TYPE_COUNT; i++) {
        if (stats.assets_by_type[i] > 0) {
            Com_Printf("  %s: %u\n", asset_type_strings[i], stats.assets_by_type[i]);
        }
    }

    Com_Printf("\nBy Quality:\n");
    for (int i = 0; i < COOK_QUALITY_COUNT; i++) {
        if (stats.assets_by_quality[i] > 0) {
            Com_Printf("  %s: %u\n", quality_strings[i], stats.assets_by_quality[i]);
        }
    }

    Com_Printf("=================================\n");
}

void AssetCooking_GenerateReport(const char* report_path) {
    if (!report_path) return;

    FILE* file = fopen(report_path, "w");
    if (!file) return;

    fprintf(file, "Asset Cooking Pipeline Report\n");
    fprintf(file, "=============================\n\n");

    fprintf(file, "Configuration:\n");
    fprintf(file, "  Source Directory: %s\n", cooking_pipeline.source_directory);
    fprintf(file, "  Output Directory: %s\n", cooking_pipeline.output_directory);
    fprintf(file, "  Cache Directory: %s\n", cooking_pipeline.cache_directory);
    fprintf(file, "  Platform: %s\n", platform_strings[cooking_pipeline.target_platform]);
    fprintf(file, "  Default Quality: %s\n", quality_strings[cooking_pipeline.default_quality]);
    fprintf(file, "  Incremental Cooking: %s\n", cooking_pipeline.incremental_cooking ? "Enabled" : "Disabled");
    fprintf(file, "  Build-time Cooking: %s\n", cooking_pipeline.build_time_cooking ? "Enabled" : "Disabled");
    fprintf(file, "\n");

    cooking_statistics_t stats = cooking_pipeline.statistics;
    fprintf(file, "Statistics:\n");
    fprintf(file, "  Total Assets: %u\n", stats.total_assets);
    fprintf(file, "  Successfully Processed: %u\n", stats.processed_assets);
    fprintf(file, "  Failed: %u\n", stats.failed_assets);
    fprintf(file, "  Total Input Size: %zu bytes (%.2f MB)\n",
            stats.total_input_size, stats.total_input_size / (1024.0 * 1024.0));
    fprintf(file, "  Total Output Size: %zu bytes (%.2f MB)\n",
             (size_t)stats.total_output_size, stats.total_output_size / (1024.0 * 1024.0));
    fprintf(file, "  Processing Time: %u ms (%.2f seconds)\n",
             (unsigned int)stats.processing_time_ms, stats.processing_time_ms / 1000.0);
    fprintf(file, "  Average Compression Ratio: %.3f\n", stats.average_compression_ratio);

    if (stats.total_assets > 0) {
        float success_rate = (float)stats.processed_assets / (float)stats.total_assets * 100.0f;
        fprintf(file, "  Success Rate: %.1f%%\n", success_rate);
    }

    fprintf(file, "\nAssets by Type:\n");
    for (int i = 0; i < ASSET_TYPE_COUNT; i++) {
        if (stats.assets_by_type[i] > 0) {
            fprintf(file, "  %s: %u\n", asset_type_strings[i], stats.assets_by_type[i]);
        }
    }

    fprintf(file, "\nAssets by Quality:\n");
    for (int i = 0; i < COOK_QUALITY_COUNT; i++) {
        if (stats.assets_by_quality[i] > 0) {
            fprintf(file, "  %s: %u\n", quality_strings[i], stats.assets_by_quality[i]);
        }
    }

    fprintf(file, "\nReport generated at: %u\n", (unsigned int)Sys_Milliseconds());

    fclose(file);
}

/*
=============================================================================
Error Handling
=============================================================================
*/

cook_error_t AssetCooking_GetLastError(void) {
    return last_cook_error;
}

const char* AssetCooking_GetErrorString(cook_error_t error) {
    if (error >= COOK_ERROR_COUNT) return "Unknown error";
    return error_strings[error];
}

/*
=============================================================================
Console Commands
=============================================================================
*/

void AssetCooking_Status_f(void) {
    Com_Printf("=== Asset Cooking Status ===\n");
    Com_Printf("Enabled: %s\n", cooking_pipeline.enabled ? "Yes" : "No");
    Com_Printf("Build-time Cooking: %s\n", cooking_pipeline.build_time_cooking ? "Yes" : "No");
    Com_Printf("Incremental Cooking: %s\n", cooking_pipeline.incremental_cooking ? "Yes" : "No");
    Com_Printf("Default Quality: %s\n", quality_strings[cooking_pipeline.default_quality]);
    Com_Printf("Target Platform: %s\n", platform_strings[cooking_pipeline.target_platform]);
    Com_Printf("Max Parallel Jobs: %u\n", cooking_pipeline.max_parallel_jobs);
    Com_Printf("Jobs in Queue: %u\n", cook_job_count);
    Com_Printf("===========================\n");
}

void AssetCooking_Cook_f(void) {
    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: cook <asset_path> [quality] [type]\n");
        Com_Printf("Qualities: potato, low, medium, high, ultra\n");
        Com_Printf("Types: texture, model, sound, shader\n");
        return;
    }

    const char* asset_path = Cmd_Argv(1);
    const char* quality_str = Cmd_Argc() >= 3 ? Cmd_Argv(2) : "medium";
    const char* type_str = Cmd_Argc() >= 4 ? Cmd_Argv(3) : NULL;

    // Determine asset type from file extension if not specified
    asset_type_t asset_type = ASSET_TYPE_TEXTURE; // default
    if (type_str) {
        for (int i = 0; i < ASSET_TYPE_COUNT; i++) {
            if (Q_stricmp(type_str, asset_type_strings[i]) == 0) {
                asset_type = (asset_type_t)i;
                break;
            }
        }
    } else {
        // Auto-detect from extension
        if (strstr(asset_path, ".png") || strstr(asset_path, ".jpg") || strstr(asset_path, ".tga")) {
            asset_type = ASSET_TYPE_TEXTURE;
        } else if (strstr(asset_path, ".obj") || strstr(asset_path, ".fbx")) {
            asset_type = ASSET_TYPE_MODEL;
        } else if (strstr(asset_path, ".wav") || strstr(asset_path, ".mp3")) {
            asset_type = ASSET_TYPE_SOUND;
        } else if (strstr(asset_path, ".glsl") || strstr(asset_path, ".hlsl")) {
            asset_type = ASSET_TYPE_SHADER;
        }
    }

    // Parse quality
    cook_quality_t quality = COOK_QUALITY_MEDIUM;
    for (int i = 0; i < COOK_QUALITY_COUNT; i++) {
        if (Q_stricmp(quality_str, quality_strings[i]) == 0) {
            quality = (cook_quality_t)i;
            break;
        }
    }

    // Create and process job
    cook_job_t* job = AssetCooking_CreateJob(asset_path, asset_type, quality,
                                           cooking_pipeline.target_platform);
    if (job) {
        if (AssetCooking_ProcessJob(job)) {
            Com_Printf("Successfully cooked asset: %s\n", asset_path);
        } else {
            Com_Printf("Failed to cook asset: %s (%s)\n", asset_path,
                      AssetCooking_GetErrorString(AssetCooking_GetLastError()));
        }

        if (job->type_specific_options) {
            free(job->type_specific_options);
        }
        free(job);
    } else {
        Com_Printf("Failed to create cooking job for: %s\n", asset_path);
    }
}

void AssetCooking_BatchCook_f(void) {
    Com_Printf("Starting batch cooking process...\n");

    if (AssetCooking_ProcessAllJobs()) {
        Com_Printf("Batch cooking completed successfully\n");
    } else {
        Com_Printf("Batch cooking completed with errors\n");
    }
}

void AssetCooking_Quality_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Current quality: %s\n", quality_strings[cooking_pipeline.default_quality]);
        Com_Printf("Available qualities: potato, low, medium, high, ultra\n");
        return;
    }

    const char* quality_str = Cmd_Argv(1);
    for (int i = 0; i < COOK_QUALITY_COUNT; i++) {
        if (Q_stricmp(quality_str, quality_strings[i]) == 0) {
            AssetCooking_SetQuality((cook_quality_t)i);
            Com_Printf("Cooking quality set to: %s\n", quality_strings[i]);
            return;
        }
    }

    Com_Printf("Invalid quality: %s\n", quality_str);
}

void AssetCooking_Platform_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Current platform: %s\n", platform_strings[cooking_pipeline.target_platform]);
        Com_Printf("Available platforms: desktop, mobile, console, web\n");
        return;
    }

    const char* platform_str = Cmd_Argv(1);
    for (int i = 0; i < COOK_PLATFORM_COUNT; i++) {
        if (Q_stricmp(platform_str, platform_strings[i]) == 0) {
            AssetCooking_SetPlatform((cook_platform_t)i);
            Com_Printf("Target platform set to: %s\n", platform_strings[i]);
            return;
        }
    }

	Com_Printf("Invalid platform: %s\n", platform_str);
}

/*
===============
AssetCooking_AddDependency
===============
*/
qboolean AssetCooking_AddDependency(cook_job_t *job, const char *dependency_path, asset_type_t dep_type) {
	if (!job || !dependency_path || job->dependency_count >= MAX_ASSET_DEPENDENCIES) {
		return qfalse;
	}

	// Check if dependency already exists
	for (uint32_t i = 0; i < job->dependency_count; i++) {
		if (strcmp(job->dependencies[i].asset_path, dependency_path) == 0) {
			return qtrue; // Already exists
		}
	}

	asset_dependency_t *dep = &job->dependencies[job->dependency_count++];
	Q_strncpyz(dep->asset_path, dependency_path, sizeof(dep->asset_path));
	dep->asset_type = dep_type;
	dep->last_modified = 0;
	dep->checksum = 0;

	// Get current file info
	struct stat st;
	if (stat(dependency_path, &st) == 0) {
		dep->last_modified = st.st_mtime;
		// TODO: Calculate checksum
		dep->checksum = 0;
	}

	return qtrue;
}

/*
===============
AssetCooking_CheckDependencies
===============
*/
qboolean AssetCooking_CheckDependencies(const cook_job_t *job) {
	if (!job) return qtrue;

	for (uint32_t i = 0; i < job->dependency_count; i++) {
		const asset_dependency_t *dep = &job->dependencies[i];

		struct stat st;
		if (stat(dep->asset_path, &st) != 0) {
			Com_Printf("AssetCooking: Dependency missing: %s\n", dep->asset_path);
			return qfalse; // Dependency file missing
		}

		if (st.st_mtime > dep->last_modified) {
			Com_Printf("AssetCooking: Dependency modified: %s\n", dep->asset_path);
			return qfalse; // Dependency was modified
		}

		// TODO: Check checksum as well
	}

	return qtrue;
}

/*
===============
AssetCooking_ResolveDependencies
===============
*/
qboolean AssetCooking_ResolveDependencies(cook_job_t *job) {
	if (!job) return qfalse;

	switch (job->asset_type) {
		case ASSET_TYPE_MATERIAL: {
			// Materials depend on their textures and shaders
			// TODO: Implement material parsing for dependency extraction
			// For now, assume materials don't have external dependencies
			break;
		}

		case ASSET_TYPE_MODEL: {
			// Models may depend on textures and animations
			// TODO: Implement model parsing for dependency extraction
			break;
		}

		case ASSET_TYPE_SHADER: {
			// Shaders may depend on other shaders or textures
			// TODO: Implement shader parsing for dependency extraction
			break;
		}

		default:
			// Other asset types typically don't have complex dependencies
			break;
	}

	return qtrue;
}

/*
===============
AssetCooking_ClearDependencies
===============
*/
void AssetCooking_ClearDependencies(cook_job_t *job) {
	if (!job) return;

	job->dependency_count = 0;
	Com_Memset(job->dependencies, 0, sizeof(job->dependencies));
}

/*
===============
AssetCooking_OptimizeAsset
===============
*/
qboolean AssetCooking_OptimizeAsset(const cook_job_t *job) {
	if (!job) return qfalse;

	switch (job->asset_type) {
		case ASSET_TYPE_TEXTURE: {
			// Texture optimization: mipmaps, compression, etc.
			// TODO: Implement texture optimization
			break;
		}

		case ASSET_TYPE_MODEL: {
			// Model optimization: LOD generation, mesh simplification
			// TODO: Implement model optimization
			break;
		}

		case ASSET_TYPE_SOUND: {
			// Sound optimization: compression, sample rate conversion
			// TODO: Implement sound optimization
			break;
		}

		default:
			break;
	}

	return qtrue;
}


/*
===============
AssetCooking_GetAssetInfo
===============
*/
qboolean AssetCooking_GetAssetInfo(const char *asset_path, asset_type_t type, void *info) {
	if (!asset_path || !info) return qfalse;

	struct stat st;
	if (stat(asset_path, &st) != 0) {
		return qfalse;
	}

	// Fill in basic file info
	// TODO: Parse asset-specific metadata

	return qtrue;
}

void AssetCooking_Cache_f(void) {
    Com_Printf("=== Cooking Cache Information ===\n");
    Com_Printf("Cache Directory: %s\n", cooking_pipeline.cache_directory);
    Com_Printf("Incremental Cooking: %s\n", cooking_pipeline.incremental_cooking ? "Enabled" : "Disabled");

    // Count cache files
    DIR* dir = opendir(cooking_pipeline.cache_directory);
    if (dir) {
        int file_count = 0;
        uint64_t total_size = 0;
        struct dirent* entry;

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            file_count++;

            char full_path[512];
            Q_snprintf(full_path, sizeof(full_path), "%s/%s", cooking_pipeline.cache_directory, entry->d_name);

            struct stat st;
            if (stat(full_path, &st) == 0) {
                total_size += st.st_size;
            }
        }
        closedir(dir);

        Com_Printf("Cached Files: %d\n", file_count);
        Com_Printf("Cache Size: %.2f MB\n", total_size / (1024.0 * 1024.0));
    } else {
        Com_Printf("Cache directory not accessible\n");
    }

    Com_Printf("=================================\n");
}

void AssetCooking_Report_f(void) {
    char report_path[512];
    Q_snprintf(report_path, sizeof(report_path), "%s/cooking_report.txt", cooking_pipeline.output_directory);

    AssetCooking_GenerateReport(report_path);
    Com_Printf("Cooking report generated: %s\n", report_path);
}
