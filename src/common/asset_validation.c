/*
=============================================================================
Asset Validation System Implementation

Automated asset correctness and optimization checking framework.
=============================================================================
*/

#include "asset_validation.h"
#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <math.h>

// Global asset validation system
asset_validation_system_t asset_validation = {0};

// Supported asset types
static const char* supported_asset_types[] = {
    "texture", "model", "sound", "shader", "material", "level", "animation", "script"
};

// Issue severity strings
static const char* severity_strings[] = {
    "INFO", "WARNING", "ERROR", "CRITICAL"
};

// Validation result strings
static const char* result_strings[] = {
    "PASS", "WARNING", "ERROR", "CRITICAL", "SKIP"
};

// Check category strings
static const char* check_strings[] = {
    "Correctness", "Optimization", "Compatibility", "Quality", "Metadata"
};

// Texture format signatures
static const struct {
    const char* extension;
    const char* description;
    qboolean requires_mipmaps;
    qboolean supports_compression;
} texture_formats[] = {
    {".png", "PNG (Portable Network Graphics)", qtrue, qfalse},
    {".jpg", "JPEG", qfalse, qtrue},
    {".jpeg", "JPEG", qfalse, qtrue},
    {".tga", "TGA (Targa)", qtrue, qfalse},
    {".dds", "DDS (DirectDraw Surface)", qtrue, qtrue},
    {".ktx", "KTX (Khronos Texture)", qtrue, qtrue},
    {".ktx2", "KTX2 (Khronos Texture v2)", qtrue, qtrue},
    {".basis", "BasisU Universal Texture", qtrue, qtrue},
    {NULL, NULL, qfalse, qfalse}
};

// Audio format specifications
static const struct {
    const char* extension;
    const char* description;
    int min_sample_rate;
    int max_channels;
    qboolean supports_compression;
} audio_formats[] = {
    {".wav", "WAV (Waveform Audio)", 8000, 8, qfalse},
    {".mp3", "MP3 (MPEG Audio Layer III)", 8000, 2, qtrue},
    {".ogg", "OGG Vorbis", 8000, 8, qtrue},
    {".flac", "FLAC (Free Lossless Audio Codec)", 8000, 8, qfalse},
    {".aac", "AAC (Advanced Audio Coding)", 8000, 8, qtrue},
    {NULL, NULL, 0, 0, qfalse}
};

/*
=============================================================================
Asset Validation API Implementation
=============================================================================
*/

qboolean AssetValidation_Init(void) {
    if (asset_validation.initialized) {
        return qtrue;
    }

    memset(&asset_validation, 0, sizeof(asset_validation_system_t));

    // Allocate results storage
    asset_validation.max_results = 1000;
    asset_validation.results = (asset_validation_result_t*)malloc(
        sizeof(asset_validation_result_t) * asset_validation.max_results);

    if (!asset_validation.results) {
        Com_Printf("Failed to allocate memory for validation results\n");
        return qfalse;
    }

    memset(asset_validation.results, 0,
           sizeof(asset_validation_result_t) * asset_validation.max_results);

    // Initialize each result structure
    for (uint32_t i = 0; i < asset_validation.max_results; i++) {
        asset_validation.results[i].max_issues = 50;
        asset_validation.results[i].issues = (validation_issue_t*)malloc(
            sizeof(validation_issue_t) * asset_validation.results[i].max_issues);

        if (!asset_validation.results[i].issues) {
            Com_Printf("Failed to allocate memory for validation issues\n");
            AssetValidation_Shutdown();
            return qfalse;
        }

        memset(asset_validation.results[i].issues, 0,
               sizeof(validation_issue_t) * asset_validation.results[i].max_issues);
    }

    // Set default configuration
    asset_validation.config.enable_detailed_logging = qtrue;
    asset_validation.config.enable_auto_fix = qfalse;
    asset_validation.config.enable_strict_mode = qfalse;
    asset_validation.config.enable_performance_checks = qtrue;
    asset_validation.config.enable_quality_checks = qtrue;

    // Set default thresholds
    asset_validation.config.max_texture_size = 4096;
    asset_validation.config.min_texture_size = 8;
    asset_validation.config.max_compression_ratio = 0.1f; // 10:1 compression max
    asset_validation.config.max_vertices_per_mesh = 100000;
    asset_validation.config.max_polygons_per_model = 50000;

    // Quality thresholds
    asset_validation.config.min_texture_quality = 0.7f;
    asset_validation.config.min_audio_quality = 0.8f;
    asset_validation.config.min_samples_per_second = 22050;

    // Platform settings
    Q_strncpyz(asset_validation.config.target_platform, "desktop", sizeof(asset_validation.config.target_platform));
    asset_validation.config.check_platform_specific = qtrue;

    asset_validation.initialized = qtrue;

    Com_Printf("Asset validation system initialized\n");
    Com_Printf("Supports validation for: textures, models, sounds, shaders, materials\n");

    return qtrue;
}

void AssetValidation_Shutdown(void) {
    if (!asset_validation.initialized) {
        return;
    }

    // Free issue arrays
    for (uint32_t i = 0; i < asset_validation.max_results; i++) {
        if (asset_validation.results[i].issues) {
            free(asset_validation.results[i].issues);
        }
    }

    // Free results array
    if (asset_validation.results) {
        free(asset_validation.results);
    }

    asset_validation.initialized = qfalse;
    Com_Printf("Asset validation system shutdown\n");
}

/*
=============================================================================
Asset Validation Core
=============================================================================
*/

asset_validation_result_t* AssetValidation_ValidateAsset(const char* asset_path,
                                                       const char* asset_type) {
    if (!asset_validation.initialized || !asset_path) {
        return NULL;
    }

    // Find or create result structure
    asset_validation_result_t* result = NULL;
    for (uint32_t i = 0; i < asset_validation.result_count; i++) {
        if (Q_stricmp(asset_validation.results[i].asset_path, asset_path) == 0) {
            result = &asset_validation.results[i];
            // Reset result for re-validation
            result->issue_count = 0;
            result->checks_passed = 0;
            result->checks_failed = 0;
            result->checks_skipped = 0;
            result->total_checks = 0;
            break;
        }
    }

    if (!result) {
        if (asset_validation.result_count >= asset_validation.max_results) {
            Com_Printf("Maximum validation results reached\n");
            return NULL;
        }
        result = &asset_validation.results[asset_validation.result_count++];
    }

    // Initialize result
    memset(result, 0, sizeof(asset_validation_result_t));
    Q_strncpyz(result->asset_path, asset_path, sizeof(result->asset_path));
    Q_strncpyz(result->asset_type, asset_type, sizeof(result->asset_type));
    Q_strncpyz(result->validation_version, "1.0", sizeof(result->validation_version));
    result->timestamp = Sys_Milliseconds();
    result->overall_result = VALIDATION_PASS;

    uint64_t start_time = Sys_Milliseconds();

    // Validate based on asset type
    qboolean validation_success = qfalse;
    if (Q_stricmp(asset_type, "texture") == 0) {
        validation_success = AssetValidation_ValidateTexture(asset_path, result);
    } else if (Q_stricmp(asset_type, "model") == 0) {
        validation_success = AssetValidation_ValidateModel(asset_path, result);
    } else if (Q_stricmp(asset_type, "sound") == 0) {
        validation_success = AssetValidation_ValidateSound(asset_path, result);
    } else if (Q_stricmp(asset_type, "shader") == 0) {
        validation_success = AssetValidation_ValidateShader(asset_path, result);
    } else if (Q_stricmp(asset_type, "material") == 0) {
        validation_success = AssetValidation_ValidateMaterial(asset_path, result);
    } else {
        AssetValidation_AddIssue(result,
                               "Unknown asset type",
                               "Specify a valid asset type (texture, model, sound, shader, material)",
                               ISSUE_ERROR, CHECK_CORRECTNESS, asset_path, 0, qfalse);
        validation_success = qfalse;
    }

    result->validation_time_ms = Sys_Milliseconds() - start_time;

    // Determine overall result based on issues
    validation_result_t final_result = VALIDATION_PASS;
    for (uint32_t i = 0; i < result->issue_count; i++) {
        issue_severity_t severity = result->issues[i].severity;
        if (severity == ISSUE_CRITICAL) {
            final_result = VALIDATION_CRITICAL;
            break;
        } else if (severity == ISSUE_ERROR && final_result < VALIDATION_ERROR) {
            final_result = VALIDATION_ERROR;
        } else if (severity == ISSUE_WARNING && final_result < VALIDATION_WARNING) {
            final_result = VALIDATION_WARNING;
        }
    }
    result->overall_result = final_result;

    // Update global statistics
    asset_validation.total_assets_validated++;
    asset_validation.total_validation_time_ms += result->validation_time_ms;
    asset_validation.average_validation_time_ms = (float)asset_validation.total_validation_time_ms /
                                                asset_validation.total_assets_validated;

    if (final_result == VALIDATION_PASS) {
        asset_validation.assets_passed++;
    } else if (final_result == VALIDATION_WARNING) {
        asset_validation.assets_with_warnings++;
    } else if (final_result == VALIDATION_ERROR) {
        asset_validation.assets_with_errors++;
    } else if (final_result == VALIDATION_CRITICAL) {
        asset_validation.assets_critical++;
    }

    // Update issue statistics
    for (uint32_t i = 0; i < result->issue_count; i++) {
        asset_validation.total_issues_found++;
        asset_validation.issues_by_severity[result->issues[i].severity]++;
        asset_validation.issues_by_category[result->issues[i].category]++;
    }

    return result;
}

/*
=============================================================================
Texture Validation
=============================================================================
*/

qboolean AssetValidation_ValidateTexture(const char* texture_path,
                                       asset_validation_result_t* result) {
    if (!texture_path || !result) return qfalse;

    // Check 1: File exists
    FILE* file = fopen(texture_path, "rb");
    if (!file) {
        AssetValidation_AddIssue(result,
                               "Texture file not found",
                               "Ensure the texture file exists and path is correct",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, texture_path, 0, qfalse);
        return qfalse;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    result->file_size_bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Check 2: File format validation
    char extension[16] = "";
    const char* dot = strrchr(texture_path, '.');
    if (dot) {
        Q_strncpyz(extension, dot, sizeof(extension));
    }

    qboolean format_supported = qfalse;
    const char* format_desc = "Unknown";
    qboolean requires_mipmaps = qfalse;
    qboolean supports_compression = qfalse;

    for (int i = 0; texture_formats[i].extension; i++) {
        if (Q_stricmp(extension, texture_formats[i].extension) == 0) {
            format_supported = qtrue;
            format_desc = texture_formats[i].description;
            requires_mipmaps = texture_formats[i].requires_mipmaps;
            supports_compression = texture_formats[i].supports_compression;
            break;
        }
    }

    if (!format_supported) {
        AssetValidation_AddIssue(result,
                               "Unsupported texture format",
                               "Use PNG, JPEG, TGA, DDS, or KTX formats",
                               ISSUE_ERROR, CHECK_COMPATIBILITY, texture_path, 0, qfalse);
    }

    // Check 3: Basic file header validation
    unsigned char header[16];
    size_t header_read = fread(header, 1, sizeof(header), file);
    fclose(file);

    if (header_read >= 8) {
        // PNG signature: 89 50 4E 47 0D 0A 1A 0A
        if (Q_stricmp(extension, ".png") == 0) {
            if (!(header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47)) {
                AssetValidation_AddIssue(result,
                                       "Invalid PNG file signature",
                                       "File may be corrupted or not a valid PNG",
                                       ISSUE_ERROR, CHECK_CORRECTNESS, texture_path, 0, qfalse);
            }
        }
        // JPEG signature: FF D8 FF
        else if (Q_stricmp(extension, ".jpg") == 0 || Q_stricmp(extension, ".jpeg") == 0) {
            if (!(header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF)) {
                AssetValidation_AddIssue(result,
                                       "Invalid JPEG file signature",
                                       "File may be corrupted or not a valid JPEG",
                                       ISSUE_ERROR, CHECK_CORRECTNESS, texture_path, 0, qfalse);
            }
        }
        // KTX2 signature: AB 4B 54 58 20 32 30 BB 0D 0A 1A 0A
        else if (Q_stricmp(extension, ".ktx2") == 0) {
            if (!(header[0] == 0xAB && header[1] == 0x4B && header[2] == 0x54 && header[3] == 0x58 &&
                  header[4] == 0x20 && header[5] == 0x32 && header[6] == 0x30 && header[7] == 0xBB &&
                  header[8] == 0x0D && header[9] == 0x0A && header[10] == 0x1A && header[11] == 0x0A)) {
                AssetValidation_AddIssue(result,
                                       "Invalid KTX2 file signature",
                                       "File may be corrupted or not a valid KTX2 texture",
                                       ISSUE_ERROR, CHECK_CORRECTNESS, texture_path, 0, qfalse);
            }
        }
        // BasisU doesn't have a standard file signature, but we can check file size
        else if (Q_stricmp(extension, ".basis") == 0) {
            // BasisU files should be reasonably sized
            if (result->file_size_bytes < 64) {
                AssetValidation_AddIssue(result,
                                       "BasisU file too small",
                                       "File may be corrupted or incomplete",
                                       ISSUE_WARNING, CHECK_CORRECTNESS, texture_path, 0, qfalse);
            }
        }
    }

    // Check 4: File size validation
    if (result->file_size_bytes == 0) {
        AssetValidation_AddIssue(result,
                               "Empty texture file",
                               "Texture file contains no data",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, texture_path, 0, qfalse);
    } else if (result->file_size_bytes > 50 * 1024 * 1024) { // 50MB limit
        AssetValidation_AddIssue(result,
                               "Texture file too large",
                               "Consider compressing or reducing texture resolution",
                               ISSUE_WARNING, CHECK_OPTIMIZATION, texture_path, 0, qfalse);
    }

    // Check 5: Compression ratio (if applicable)
    if (supports_compression && result->file_size_bytes > 1024) {
        // This would need actual image dimensions to calculate properly
        // For now, just check against arbitrary thresholds
        result->compression_ratio = 1.0f; // Placeholder
    }

    // Check 6: Mipmap requirements
    if (requires_mipmaps) {
        // DDS files should have mipmaps, but we can't easily check without parsing
        AssetValidation_AddIssue(result,
                               "Mipmaps recommended",
                               "Consider generating mipmaps for better performance",
                               ISSUE_INFO, CHECK_OPTIMIZATION, texture_path, 0, qfalse);
    }

    // Check 7: Platform-specific validation
    if (asset_validation.config.check_platform_specific) {
        if (Q_stricmp(asset_validation.config.target_platform, "mobile") == 0) {
            if (result->file_size_bytes > 2 * 1024 * 1024) { // 2MB for mobile
                AssetValidation_AddIssue(result,
                                       "Texture too large for mobile platform",
                                       "Reduce texture size or use higher compression for mobile devices",
                                       ISSUE_WARNING, CHECK_COMPATIBILITY, texture_path, 0, qfalse);
            }
        }
    }

    return (result->overall_result != VALIDATION_CRITICAL);
}

/*
=============================================================================
Model Validation
=============================================================================
*/

qboolean AssetValidation_ValidateModel(const char* model_path,
                                     asset_validation_result_t* result) {
    if (!model_path || !result) return qfalse;

    // Check 1: File exists
    FILE* file = fopen(model_path, "r");
    if (!file) {
        AssetValidation_AddIssue(result,
                               "Model file not found",
                               "Ensure the model file exists and path is correct",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, model_path, 0, qfalse);
        return qfalse;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    result->file_size_bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Check 2: File format validation
    char extension[16] = "";
    const char* dot = strrchr(model_path, '.');
    if (dot) {
        Q_strncpyz(extension, dot, sizeof(extension));
    }

    qboolean format_supported = qfalse;
    if (Q_stricmp(extension, ".obj") == 0 || Q_stricmp(extension, ".fbx") == 0 ||
        Q_stricmp(extension, ".dae") == 0 || Q_stricmp(extension, ".gltf") == 0 ||
        Q_stricmp(extension, ".glb") == 0 || Q_stricmp(extension, ".3ds") == 0) {
        format_supported = qtrue;
    }

    if (!format_supported) {
        AssetValidation_AddIssue(result,
                               "Unsupported model format",
                               "Use OBJ, FBX, DAE, GLTF, GLB, or 3DS formats",
                               ISSUE_ERROR, CHECK_COMPATIBILITY, model_path, 0, qfalse);
    }

    // Check 3: Basic content validation for OBJ files
    if (Q_stricmp(extension, ".obj") == 0) {
        char line[256];
        qboolean has_vertices = qfalse;
        qboolean has_faces = qfalse;
        uint32_t vertex_count = 0;
        uint32_t face_count = 0;

        while (fgets(line, sizeof(line), file)) {
            if (line[0] == 'v' && line[1] == ' ') {
                vertex_count++;
                has_vertices = qtrue;
            } else if (line[0] == 'f' && line[1] == ' ') {
                face_count++;
                has_faces = qtrue;
            }
        }

        if (!has_vertices) {
            AssetValidation_AddIssue(result,
                                   "Model has no vertices",
                                   "OBJ file must contain vertex definitions (v lines)",
                                   ISSUE_CRITICAL, CHECK_CORRECTNESS, model_path, 0, qfalse);
        }

        if (!has_faces) {
            AssetValidation_AddIssue(result,
                                   "Model has no faces",
                                   "OBJ file must contain face definitions (f lines)",
                                   ISSUE_CRITICAL, CHECK_CORRECTNESS, model_path, 0, qfalse);
        }

        // Check vertex count
        if (vertex_count > asset_validation.config.max_vertices_per_mesh) {
            AssetValidation_AddIssue(result,
                                   "Too many vertices in model",
                                   "Consider simplifying the model or splitting into smaller meshes",
                                   ISSUE_WARNING, CHECK_OPTIMIZATION, model_path, 0, qfalse);
        }

        // Check face count (rough estimate)
        if (face_count > asset_validation.config.max_polygons_per_model) {
            AssetValidation_AddIssue(result,
                                   "Too many polygons in model",
                                   "Consider reducing polygon count or using LOD",
                                   ISSUE_WARNING, CHECK_OPTIMIZATION, model_path, 0, qfalse);
        }
    }

    fclose(file);

    // Check 4: File size validation
    if (result->file_size_bytes == 0) {
        AssetValidation_AddIssue(result,
                               "Empty model file",
                               "Model file contains no data",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, model_path, 0, qfalse);
    } else if (result->file_size_bytes > 100 * 1024 * 1024) { // 100MB limit
        AssetValidation_AddIssue(result,
                               "Model file too large",
                               "Consider optimizing the model or using compression",
                               ISSUE_WARNING, CHECK_OPTIMIZATION, model_path, 0, qfalse);
    }

    return (result->overall_result != VALIDATION_CRITICAL);
}

/*
=============================================================================
Sound Validation
=============================================================================
*/

qboolean AssetValidation_ValidateSound(const char* sound_path,
                                     asset_validation_result_t* result) {
    if (!sound_path || !result) return qfalse;

    // Check 1: File exists
    FILE* file = fopen(sound_path, "rb");
    if (!file) {
        AssetValidation_AddIssue(result,
                               "Sound file not found",
                               "Ensure the sound file exists and path is correct",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, sound_path, 0, qfalse);
        return qfalse;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    result->file_size_bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Check 2: File format validation
    char extension[16] = "";
    const char* dot = strrchr(sound_path, '.');
    if (dot) {
        Q_strncpyz(extension, dot, sizeof(extension));
    }

    qboolean format_supported = qfalse;
    int min_sample_rate = 0;
    int max_channels = 0;
    qboolean supports_compression = qfalse;

    for (int i = 0; audio_formats[i].extension; i++) {
        if (Q_stricmp(extension, audio_formats[i].extension) == 0) {
            format_supported = qtrue;
            min_sample_rate = audio_formats[i].min_sample_rate;
            max_channels = audio_formats[i].max_channels;
            supports_compression = audio_formats[i].supports_compression;
            break;
        }
    }

    if (!format_supported) {
        AssetValidation_AddIssue(result,
                               "Unsupported audio format",
                               "Use WAV, MP3, OGG, FLAC, or AAC formats",
                               ISSUE_ERROR, CHECK_COMPATIBILITY, sound_path, 0, qfalse);
    }

    // Check 3: Basic file header validation
    unsigned char header[16];
    size_t header_read = fread(header, 1, sizeof(header), file);

    if (header_read >= 12) {
        // WAV signature: RIFF....WAVE
        if (Q_stricmp(extension, ".wav") == 0) {
            if (!(header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F' &&
                  header[8] == 'W' && header[9] == 'A' && header[10] == 'V' && header[11] == 'E')) {
                AssetValidation_AddIssue(result,
                                       "Invalid WAV file signature",
                                       "File may be corrupted or not a valid WAV",
                                       ISSUE_ERROR, CHECK_CORRECTNESS, sound_path, 0, qfalse);
            }
        }
    }

    fclose(file);

    // Check 4: File size validation
    if (result->file_size_bytes == 0) {
        AssetValidation_AddIssue(result,
                               "Empty sound file",
                               "Sound file contains no audio data",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, sound_path, 0, qfalse);
    } else if (result->file_size_bytes > 50 * 1024 * 1024) { // 50MB limit
        AssetValidation_AddIssue(result,
                               "Sound file too large",
                               "Consider compressing or reducing audio quality",
                               ISSUE_WARNING, CHECK_OPTIMIZATION, sound_path, 0, qfalse);
    }

    // Check 5: Quality recommendations
    if (supports_compression) {
        AssetValidation_AddIssue(result,
                               "Consider uncompressed format for critical audio",
                               "WAV or FLAC provide better quality for important sound effects",
                               ISSUE_INFO, CHECK_QUALITY, sound_path, 0, qfalse);
    }

    return (result->overall_result != VALIDATION_CRITICAL);
}

/*
=============================================================================
Shader Validation
=============================================================================
*/

qboolean AssetValidation_ValidateShader(const char* shader_path,
                                      asset_validation_result_t* result) {
    if (!shader_path || !result) return qfalse;

    // Check 1: File exists
    FILE* file = fopen(shader_path, "r");
    if (!file) {
        AssetValidation_AddIssue(result,
                               "Shader file not found",
                               "Ensure the shader file exists and path is correct",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, shader_path, 0, qfalse);
        return qfalse;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    result->file_size_bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Check 2: File format validation
    char extension[16] = "";
    const char* dot = strrchr(shader_path, '.');
    if (dot) {
        Q_strncpyz(extension, dot, sizeof(extension));
    }

    qboolean format_supported = qfalse;
    if (Q_stricmp(extension, ".glsl") == 0 || Q_stricmp(extension, ".vert") == 0 ||
        Q_stricmp(extension, ".frag") == 0 || Q_stricmp(extension, ".comp") == 0 ||
        Q_stricmp(extension, ".geom") == 0 || Q_stricmp(extension, ".tesc") == 0 ||
        Q_stricmp(extension, ".tese") == 0) {
        format_supported = qtrue;
    }

    if (!format_supported) {
        AssetValidation_AddIssue(result,
                               "Unsupported shader format",
                               "Use GLSL (.glsl, .vert, .frag, .comp, .geom, .tesc, .tese)",
                               ISSUE_ERROR, CHECK_COMPATIBILITY, shader_path, 0, qfalse);
    }

    // Check 3: Basic syntax validation
    char line[1024];
    uint32_t line_number = 0;
    qboolean has_version = qfalse;
    qboolean has_main = qfalse;

    while (fgets(line, sizeof(line), file)) {
        line_number++;

        // Check for #version directive
        if (strstr(line, "#version") == line) {
            has_version = qtrue;

            // Check version number
            char* version_str = strstr(line, "#version");
            if (version_str) {
                int version = atoi(version_str + 8);
                if (version < 120) {
                    char issue_desc[256];
                    Q_snprintf(issue_desc, sizeof(issue_desc),
                             "Shader uses old GLSL version %d", version);
                    AssetValidation_AddIssue(result,
                                           issue_desc,
                                           "Consider updating to GLSL 330 or higher for better compatibility",
                                           ISSUE_WARNING, CHECK_COMPATIBILITY, shader_path, line_number, qfalse);
                }
            }
        }

        // Check for main function
        if (strstr(line, "void main(") || strstr(line, "void main ")) {
            has_main = qtrue;
        }

        // Check for deprecated functions
        if (strstr(line, "texture2D(") || strstr(line, "texture3D(")) {
            AssetValidation_AddIssue(result,
                                   "Using deprecated texture functions",
                                   "Use texture() instead of texture2D/texture3D for GLSL 330+",
                                   ISSUE_WARNING, CHECK_OPTIMIZATION, shader_path, line_number, qfalse);
        }

        // Check for potential performance issues
        if (strstr(line, "while(") || strstr(line, "for(")) {
            AssetValidation_AddIssue(result,
                                   "Loop detected in shader",
                                   "Ensure loops are bounded and efficient in shader code",
                                   ISSUE_INFO, CHECK_OPTIMIZATION, shader_path, line_number, qfalse);
        }
    }

    fclose(file);

    if (!has_version) {
        AssetValidation_AddIssue(result,
                               "Shader missing #version directive",
                               "Add #version directive at the top of the shader",
                               ISSUE_ERROR, CHECK_CORRECTNESS, shader_path, 0, qfalse);
    }

    if (!has_main) {
        AssetValidation_AddIssue(result,
                               "Shader missing main function",
                               "All shaders must have a main() function",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, shader_path, 0, qfalse);
    }

    // Check 4: File size validation
    if (result->file_size_bytes == 0) {
        AssetValidation_AddIssue(result,
                               "Empty shader file",
                               "Shader file contains no code",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, shader_path, 0, qfalse);
    } else if (result->file_size_bytes > 1024 * 1024) { // 1MB limit
        AssetValidation_AddIssue(result,
                               "Shader file very large",
                               "Consider optimizing or splitting the shader code",
                               ISSUE_WARNING, CHECK_OPTIMIZATION, shader_path, 0, qfalse);
    }

    return (result->overall_result != VALIDATION_CRITICAL);
}

/*
=============================================================================
Material Validation
=============================================================================
*/

qboolean AssetValidation_ValidateMaterial(const char* material_path,
                                        asset_validation_result_t* result) {
    if (!material_path || !result) return qfalse;

    // For now, provide basic file existence check
    // Material validation would depend on the specific material format used
    FILE* file = fopen(material_path, "r");
    if (!file) {
        AssetValidation_AddIssue(result,
                               "Material file not found",
                               "Ensure the material file exists and path is correct",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, material_path, 0, qfalse);
        return qfalse;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    result->file_size_bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Basic validation - check if it's not empty
    if (result->file_size_bytes == 0) {
        AssetValidation_AddIssue(result,
                               "Empty material file",
                               "Material file contains no data",
                               ISSUE_CRITICAL, CHECK_CORRECTNESS, material_path, 0, qfalse);
    }

    fclose(file);

    // Material-specific validation would go here
    AssetValidation_AddIssue(result,
                           "Material validation not fully implemented",
                           "Basic file validation only - material format specific checks needed",
                           ISSUE_INFO, CHECK_CORRECTNESS, material_path, 0, qfalse);

    return qtrue;
}

/*
=============================================================================
Batch Validation and Reporting
=============================================================================
*/

uint32_t AssetValidation_ValidateDirectory(const char* directory_path,
                                         const char* asset_type_filter) {
    if (!asset_validation.initialized || !directory_path) {
        return 0;
    }

    DIR* dir = opendir(directory_path);
    if (!dir) {
        Com_Printf("Failed to open directory: %s\n", directory_path);
        return 0;
    }

    uint32_t validated_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check if it's a regular file
        char full_path[1024];
        Q_snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
            // Determine asset type from file extension
            char asset_type[32];
            if (AssetValidation_GetAssetTypeFromPath(full_path, asset_type, sizeof(asset_type))) {
                // Check if matches filter (if provided)
                if (!asset_type_filter || Q_stricmp(asset_type, asset_type_filter) == 0) {
                    AssetValidation_ValidateAsset(full_path, asset_type);
                    validated_count++;
                }
            }
        }
    }

    closedir(dir);
    return validated_count;
}

qboolean AssetValidation_AddIssue(asset_validation_result_t* result,
                                const char* description,
                                const char* recommendation,
                                issue_severity_t severity,
                                validation_check_t category,
                                const char* file_path,
                                uint32_t line_number,
                                qboolean auto_fixable) {
    if (!result || result->issue_count >= result->max_issues) {
        return qfalse;
    }

    validation_issue_t* issue = &result->issues[result->issue_count++];
    Q_strncpyz(issue->description, description, sizeof(issue->description));
    Q_strncpyz(issue->recommendation, recommendation, sizeof(issue->recommendation));
    issue->severity = severity;
    issue->category = category;
    Q_strncpyz(issue->file_path, file_path, sizeof(issue->file_path));
    issue->line_number = line_number;
    issue->auto_fixable = auto_fixable;

    result->total_checks++;

    // Update check counters based on severity
    if (severity == ISSUE_CRITICAL || severity == ISSUE_ERROR) {
        result->checks_failed++;
    } else if (severity == ISSUE_WARNING) {
        result->checks_passed++; // Warnings are considered passed but with issues
    } else {
        result->checks_passed++;
    }

    return qtrue;
}

qboolean AssetValidation_GenerateReport(const char* output_file,
                                      const char* format) {
    FILE* file = fopen(output_file, "w");
    if (!file) return qfalse;

    if (Q_stricmp(format, "json") == 0) {
        // JSON format
        fprintf(file, "{\n");
        fprintf(file, "  \"validation_summary\": {\n");
        fprintf(file, "    \"total_assets\": %u,\n", asset_validation.total_assets_validated);
        fprintf(file, "    \"assets_passed\": %u,\n", asset_validation.assets_passed);
        fprintf(file, "    \"assets_with_warnings\": %u,\n", asset_validation.assets_with_warnings);
        fprintf(file, "    \"assets_with_errors\": %u,\n", asset_validation.assets_with_errors);
        fprintf(file, "    \"assets_critical\": %u,\n", asset_validation.assets_critical);
        fprintf(file, "    \"total_issues\": %u\n", asset_validation.total_issues_found);
        fprintf(file, "  },\n");

        fprintf(file, "  \"results\": [\n");
        for (uint32_t i = 0; i < asset_validation.result_count; i++) {
            asset_validation_result_t* result = &asset_validation.results[i];
            fprintf(file, "    {\n");
            fprintf(file, "      \"asset_path\": \"%s\",\n", result->asset_path);
            fprintf(file, "      \"asset_type\": \"%s\",\n", result->asset_type);
            fprintf(file, "      \"result\": \"%s\",\n", AssetValidation_GetResultString(result->overall_result));
            fprintf(file, "      \"issues_count\": %u,\n", result->issue_count);
            fprintf(file, "      \"validation_time_ms\": %llu\n", (unsigned long long)result->validation_time_ms);
            fprintf(file, "    }%s\n", (i < asset_validation.result_count - 1) ? "," : "");
        }
        fprintf(file, "  ]\n");
        fprintf(file, "}\n");

    } else {
        // Text format
        fprintf(file, "=============================================================================\n");
        fprintf(file, "ASSET VALIDATION REPORT\n");
        fprintf(file, "Generated: %llu\n", (unsigned long long)Sys_Milliseconds());
        fprintf(file, "=============================================================================\n\n");

        // Summary
        fprintf(file, "VALIDATION SUMMARY\n");
        fprintf(file, "------------------\n");
        fprintf(file, "Total Assets Validated: %u\n", asset_validation.total_assets_validated);
        fprintf(file, "Assets Passed: %u\n", asset_validation.assets_passed);
        fprintf(file, "Assets with Warnings: %u\n", asset_validation.assets_with_warnings);
        fprintf(file, "Assets with Errors: %u\n", asset_validation.assets_with_errors);
        fprintf(file, "Assets Critical: %u\n", asset_validation.assets_critical);
        fprintf(file, "Total Issues Found: %u\n\n", asset_validation.total_issues_found);

        // Issue breakdown
        fprintf(file, "ISSUES BY SEVERITY\n");
        fprintf(file, "------------------\n");
        fprintf(file, "Info: %u\n", asset_validation.issues_by_severity[ISSUE_INFO]);
        fprintf(file, "Warning: %u\n", asset_validation.issues_by_severity[ISSUE_WARNING]);
        fprintf(file, "Error: %u\n", asset_validation.issues_by_severity[ISSUE_ERROR]);
        fprintf(file, "Critical: %u\n\n", asset_validation.issues_by_severity[ISSUE_CRITICAL]);

        // Detailed results
        fprintf(file, "DETAILED RESULTS\n");
        fprintf(file, "----------------\n");

        for (uint32_t i = 0; i < asset_validation.result_count; i++) {
            asset_validation_result_t* result = &asset_validation.results[i];
            fprintf(file, "Asset: %s\n", result->asset_path);
            fprintf(file, "Type: %s\n", result->asset_type);
            fprintf(file, "Result: %s\n", AssetValidation_GetResultString(result->overall_result));
            fprintf(file, "Issues: %u\n", result->issue_count);
            fprintf(file, "Validation Time: %llu ms\n", (unsigned long long)result->validation_time_ms);

            if (result->issue_count > 0) {
                fprintf(file, "Issues:\n");
                for (uint32_t j = 0; j < result->issue_count; j++) {
                    validation_issue_t* issue = &result->issues[j];
                    fprintf(file, "  [%s] %s\n", AssetValidation_GetSeverityString(issue->severity),
                           issue->description);
                    if (issue->recommendation[0]) {
                        fprintf(file, "    Recommendation: %s\n", issue->recommendation);
                    }
                }
            }
            fprintf(file, "\n");
        }

        // Recommendations
        fprintf(file, "RECOMMENDATIONS\n");
        fprintf(file, "---------------\n");
        if (asset_validation.assets_critical > 0) {
            fprintf(file, "- Fix critical issues immediately\n");
        }
        if (asset_validation.assets_with_errors > 0) {
            fprintf(file, "- Address error-level issues\n");
        }
        if (asset_validation.assets_with_warnings > 0) {
            fprintf(file, "- Consider fixing warning-level issues for optimal performance\n");
        }
        if (asset_validation.assets_passed == asset_validation.total_assets_validated) {
            fprintf(file, "- All assets validated successfully!\n");
        }
    }

    fclose(file);
    return qtrue;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

qboolean AssetValidation_GetAssetTypeFromPath(const char* path, char* asset_type, size_t size) {
    const char* extension = strrchr(path, '.');
    if (!extension) return qfalse;

    // Determine asset type from extension
    if (Q_stricmp(extension, ".png") == 0 || Q_stricmp(extension, ".jpg") == 0 ||
        Q_stricmp(extension, ".jpeg") == 0 || Q_stricmp(extension, ".tga") == 0 ||
        Q_stricmp(extension, ".dds") == 0 || Q_stricmp(extension, ".ktx") == 0) {
        Q_strncpyz(asset_type, "texture", size);
    } else if (Q_stricmp(extension, ".obj") == 0 || Q_stricmp(extension, ".fbx") == 0 ||
               Q_stricmp(extension, ".dae") == 0 || Q_stricmp(extension, ".gltf") == 0 ||
               Q_stricmp(extension, ".glb") == 0 || Q_stricmp(extension, ".3ds") == 0) {
        Q_strncpyz(asset_type, "model", size);
    } else if (Q_stricmp(extension, ".wav") == 0 || Q_stricmp(extension, ".mp3") == 0 ||
               Q_stricmp(extension, ".ogg") == 0 || Q_stricmp(extension, ".flac") == 0 ||
               Q_stricmp(extension, ".aac") == 0) {
        Q_strncpyz(asset_type, "sound", size);
    } else if (Q_stricmp(extension, ".glsl") == 0 || Q_stricmp(extension, ".vert") == 0 ||
               Q_stricmp(extension, ".frag") == 0 || Q_stricmp(extension, ".comp") == 0 ||
               Q_stricmp(extension, ".geom") == 0 || Q_stricmp(extension, ".tesc") == 0 ||
               Q_stricmp(extension, ".tese") == 0) {
        Q_strncpyz(asset_type, "shader", size);
    } else if (Q_stricmp(extension, ".mat") == 0 || Q_stricmp(extension, ".material") == 0) {
        Q_strncpyz(asset_type, "material", size);
    } else {
        return qfalse;
    }

    return qtrue;
}

const char* AssetValidation_GetResultString(validation_result_t result) {
    if (result >= sizeof(result_strings)/sizeof(result_strings[0])) return "UNKNOWN";
    return result_strings[result];
}

const char* AssetValidation_GetSeverityString(issue_severity_t severity) {
    if (severity >= sizeof(severity_strings)/sizeof(severity_strings[0])) return "UNKNOWN";
    return severity_strings[severity];
}

const char* AssetValidation_GetCheckString(validation_check_t check) {
    if (check >= CHECK_COUNT) return "Unknown";
    return check_strings[check];
}

/*
=============================================================================
Console Commands
=============================================================================
*/

void AssetValidation_Status_f(void) {
    if (!asset_validation.initialized) {
        Com_Printf("Asset validation system not initialized\n");
        return;
    }

    Com_Printf("=== Asset Validation System Status ===\n");
    Com_Printf("Initialized: Yes\n");
    Com_Printf("Total Assets Validated: %u\n", asset_validation.total_assets_validated);
    Com_Printf("Assets Passed: %u\n", asset_validation.assets_passed);
    Com_Printf("Assets with Warnings: %u\n", asset_validation.assets_with_warnings);
    Com_Printf("Assets with Errors: %u\n", asset_validation.assets_with_errors);
    Com_Printf("Assets Critical: %u\n", asset_validation.assets_critical);
    Com_Printf("Total Issues Found: %u\n", asset_validation.total_issues_found);
    Com_Printf("Results Stored: %u/%u\n", asset_validation.result_count, asset_validation.max_results);
    Com_Printf("Average Validation Time: %.2f ms\n", asset_validation.average_validation_time_ms);
    Com_Printf("========================================\n");
}

void AssetValidation_Validate_f(void) {
    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: validate <asset_path> [asset_type]\n");
        Com_Printf("Asset types: texture, model, sound, shader, material\n");
        Com_Printf("If asset_type is omitted, it will be auto-detected from file extension\n");
        return;
    }

    const char* asset_path = Cmd_Argv(1);
    const char* asset_type = (Cmd_Argc() >= 3) ? Cmd_Argv(2) : NULL;

    // Auto-detect asset type if not provided
    char detected_type[32];
    if (!asset_type) {
        if (!AssetValidation_GetAssetTypeFromPath(asset_path, detected_type, sizeof(detected_type))) {
            Com_Printf("Could not determine asset type from path: %s\n", asset_path);
            Com_Printf("Please specify asset type explicitly\n");
            return;
        }
        asset_type = detected_type;
    }

    asset_validation_result_t* result = AssetValidation_ValidateAsset(asset_path, asset_type);
    if (!result) {
        Com_Printf("Failed to validate asset: %s\n", asset_path);
        return;
    }

    // Print result summary
    Com_Printf("Validation Result: %s\n", AssetValidation_GetResultString(result->overall_result));
    Com_Printf("Asset Type: %s\n", result->asset_type);
    Com_Printf("File Size: %llu bytes\n", (unsigned long long)result->file_size_bytes);
    Com_Printf("Validation Time: %llu ms\n", (unsigned long long)result->validation_time_ms);
    Com_Printf("Issues Found: %u\n", result->issue_count);

    // Print issues
    if (result->issue_count > 0) {
        Com_Printf("Issues:\n");
        for (uint32_t i = 0; i < result->issue_count; i++) {
            validation_issue_t* issue = &result->issues[i];
            Com_Printf("  [%s] %s\n", AssetValidation_GetSeverityString(issue->severity),
                      issue->description);
            if (issue->recommendation[0]) {
                Com_Printf("    -> %s\n", issue->recommendation);
            }
        }
    }
}

void AssetValidation_BatchValidate_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: batchvalidate <directory_path> [asset_type]\n");
        Com_Printf("Asset types: texture, model, sound, shader, material, or 'all'\n");
        return;
    }

    const char* directory_path = Cmd_Argv(1);
    const char* asset_type_filter = (Cmd_Argc() >= 3) ? Cmd_Argv(2) : "all";

    Com_Printf("Starting batch validation of directory: %s\n", directory_path);
    if (strcmp(asset_type_filter, "all") != 0) {
        Com_Printf("Filtering by asset type: %s\n", asset_type_filter);
    }

    uint64_t start_time = Sys_Milliseconds();
    uint32_t validated_count = AssetValidation_ValidateDirectory(directory_path,
        strcmp(asset_type_filter, "all") == 0 ? NULL : asset_type_filter);
    uint64_t duration = Sys_Milliseconds() - start_time;

    Com_Printf("Batch validation completed: %u assets validated in %llu ms\n",
              validated_count, (unsigned long long)duration);

    // Print summary
    AssetValidation_PrintStatistics();
}

void AssetValidation_Report_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: validationreport <output_file> [format]\n");
        Com_Printf("Formats: text (default), json\n");
        return;
    }

    const char* output_file = Cmd_Argv(1);
    const char* format = (Cmd_Argc() >= 3) ? Cmd_Argv(2) : "text";

    if (AssetValidation_GenerateReport(output_file, format)) {
        Com_Printf("Validation report generated: %s (format: %s)\n", output_file, format);
    } else {
        Com_Printf("Failed to generate validation report\n");
    }
}

void AssetValidation_AutoFix_f(void) {
    uint32_t fixable_count = 0;
    uint32_t fixed_count = 0;

    // Count fixable issues
    for (uint32_t i = 0; i < asset_validation.result_count; i++) {
        asset_validation_result_t* result = &asset_validation.results[i];
        for (uint32_t j = 0; j < result->issue_count; j++) {
            if (result->issues[j].auto_fixable) {
                fixable_count++;
            }
        }
    }

    if (fixable_count == 0) {
        Com_Printf("No auto-fixable issues found\n");
        return;
    }

    Com_Printf("Found %u auto-fixable issues. Starting auto-fix...\n", fixable_count);

    // Note: Actual auto-fix implementation would require specific fix logic
    // for each type of issue. This is a placeholder.
    fixed_count = AssetValidation_AutoFixAll();

    Com_Printf("Auto-fix completed: %u/%u issues fixed\n", fixed_count, fixable_count);
}

void AssetValidation_Stats_f(void) {
    AssetValidation_PrintStatistics();
}

// Stub implementations for now
qboolean AssetValidation_CanAutoFix(asset_validation_result_t* result) {
    if (!result) return qfalse;
    // Check if any issues are auto-fixable
    for (uint32_t i = 0; i < result->issue_count; i++) {
        if (result->issues[i].auto_fixable) {
            return qtrue;
        }
    }
    return qfalse;
}

uint32_t AssetValidation_AutoFixAsset(asset_validation_result_t* result) {
    // Placeholder - actual implementation would fix specific issues
    return 0;
}

uint32_t AssetValidation_AutoFixAll(void) {
    // Placeholder - actual implementation would iterate through all results
    return 0;
}

uint32_t AssetValidation_GetResults(asset_validation_result_t** results) {
    if (results) {
        *results = asset_validation.results;
    }
    return asset_validation.result_count;
}

asset_validation_result_t* AssetValidation_GetResult(const char* asset_path) {
    for (uint32_t i = 0; i < asset_validation.result_count; i++) {
        if (Q_stricmp(asset_validation.results[i].asset_path, asset_path) == 0) {
            return &asset_validation.results[i];
        }
    }
    return NULL;
}

qboolean AssetValidation_SaveResults(const char* filename) {
    // Placeholder implementation
    return qtrue;
}

qboolean AssetValidation_LoadResults(const char* filename) {
    // Placeholder implementation
    return qtrue;
}

void AssetValidation_ClearResults(void) {
    asset_validation.result_count = 0;
    asset_validation.total_assets_validated = 0;
    asset_validation.assets_passed = 0;
    asset_validation.assets_with_warnings = 0;
    asset_validation.assets_with_errors = 0;
    asset_validation.assets_critical = 0;
    asset_validation.total_issues_found = 0;
    memset(asset_validation.issues_by_severity, 0, sizeof(asset_validation.issues_by_severity));
    memset(asset_validation.issues_by_category, 0, sizeof(asset_validation.issues_by_category));
    asset_validation.total_validation_time_ms = 0;
    asset_validation.average_validation_time_ms = 0.0f;
}

void AssetValidation_PrintStatistics(void) {
    Com_Printf("=== Asset Validation Statistics ===\n");
    Com_Printf("Total Assets Validated: %u\n", asset_validation.total_assets_validated);
    Com_Printf("  Passed: %u\n", asset_validation.assets_passed);
    Com_Printf("  Warnings: %u\n", asset_validation.assets_with_warnings);
    Com_Printf("  Errors: %u\n", asset_validation.assets_with_errors);
    Com_Printf("  Critical: %u\n", asset_validation.assets_critical);
    Com_Printf("Total Issues Found: %u\n", asset_validation.total_issues_found);
    Com_Printf("  Info: %u\n", asset_validation.issues_by_severity[ISSUE_INFO]);
    Com_Printf("  Warning: %u\n", asset_validation.issues_by_severity[ISSUE_WARNING]);
    Com_Printf("  Error: %u\n", asset_validation.issues_by_severity[ISSUE_ERROR]);
    Com_Printf("  Critical: %u\n", asset_validation.issues_by_severity[ISSUE_CRITICAL]);
    Com_Printf("Average Validation Time: %.2f ms\n", asset_validation.average_validation_time_ms);
    Com_Printf("===================================\n");
}

void AssetValidation_PrintResults(void) {
    Com_Printf("=== Asset Validation Results ===\n");

    for (uint32_t i = 0; i < asset_validation.result_count; i++) {
        asset_validation_result_t* result = &asset_validation.results[i];
        Com_Printf("%s: %s (%u issues)\n",
                  result->asset_path,
                  AssetValidation_GetResultString(result->overall_result),
                  result->issue_count);
    }

    Com_Printf("=================================\n");
}

qboolean AssetValidation_IsAssetTypeSupported(const char* asset_type) {
    for (int i = 0; i < sizeof(supported_asset_types)/sizeof(supported_asset_types[0]); i++) {
        if (Q_stricmp(asset_type, supported_asset_types[i]) == 0) {
            return qtrue;
        }
    }
    return qfalse;
}

// Stub implementations for quality metrics and other advanced features
float AssetValidation_CalculateTextureQuality(const char* texture_path) {
    // Placeholder - would analyze texture for artifacts, compression quality, etc.
    return 0.8f;
}

float AssetValidation_CalculateAudioQuality(const char* audio_path) {
    // Placeholder - would analyze audio for artifacts, compression quality, etc.
    return 0.85f;
}

float AssetValidation_CalculateModelQuality(const char* model_path) {
    // Placeholder - would analyze model for topology, UV mapping, etc.
    return 0.75f;
}

uint64_t AssetValidation_GetAssetLoadTime(const char* asset_path) {
    // Placeholder - would measure actual load time
    return 50; // 50ms
}

uint64_t AssetValidation_GetAssetMemoryUsage(const char* asset_path) {
    // Placeholder - would measure memory usage
    return 1024 * 1024; // 1MB
}

float AssetValidation_GetAssetCompressionRatio(const char* asset_path) {
    // Placeholder - would calculate actual compression ratio
    return 0.5f; // 2:1 compression
}

qboolean AssetValidation_CheckPlatformCompatibility(const char* asset_path,
                                                  const char* platform) {
    // Placeholder - would check platform-specific requirements
    return qtrue;
}

qboolean AssetValidation_OptimizeForPlatform(const char* asset_path,
                                           const char* platform) {
    // Placeholder - would apply platform-specific optimizations
    return qtrue;
}

uint32_t AssetValidation_ValidateAllAssets(void) {
    // Placeholder - would validate all known assets
    return 0;
}

qboolean AssetValidation_GenerateSummary(const char* output_file) {
    // Placeholder - simplified summary generation
    return AssetValidation_GenerateReport(output_file, "text");
}
