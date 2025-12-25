/*
=============================================================================
Asset Validation System

Automated asset correctness and optimization checking framework.
=============================================================================
*/

#ifndef __ASSET_VALIDATION_H__
#define __ASSET_VALIDATION_H__

#include "q_shared.h"

// Asset validation result types
typedef enum {
    VALIDATION_PASS = 0,           // Asset passed all checks
    VALIDATION_WARNING,            // Asset has warnings but is usable
    VALIDATION_ERROR,              // Asset has errors and needs fixing
    VALIDATION_CRITICAL,           // Asset has critical issues
    VALIDATION_SKIP                // Asset validation was skipped
} validation_result_t;

// Validation check categories
typedef enum {
    CHECK_CORRECTNESS = 0,         // Data integrity and format correctness
    CHECK_OPTIMIZATION,            // Performance and memory optimization
    CHECK_COMPATIBILITY,           // Platform and API compatibility
    CHECK_QUALITY,                 // Visual/audio quality metrics
    CHECK_METADATA,                // Asset metadata and dependencies
    CHECK_COUNT
} validation_check_t;

// Asset validation issue severity
typedef enum {
    ISSUE_INFO = 0,                // Informational only
    ISSUE_WARNING,                 // Warning, should be addressed
    ISSUE_ERROR,                   // Error, must be fixed
    ISSUE_CRITICAL                 // Critical error, asset unusable
} issue_severity_t;

// Asset validation issue
typedef struct {
    char description[256];         // Human-readable description
    char recommendation[256];      // Suggested fix or improvement
    issue_severity_t severity;     // Issue severity level
    validation_check_t category;   // Check category
    char file_path[256];           // File where issue was found
    uint32_t line_number;          // Line number (if applicable)
    qboolean auto_fixable;         // Whether issue can be auto-fixed
} validation_issue_t;

// Asset validation result
typedef struct {
    char asset_path[512];          // Path to validated asset
    char asset_type[32];           // Type of asset (texture, model, etc.)
    validation_result_t overall_result; // Overall validation result

    // Issue tracking
    validation_issue_t* issues;    // Array of validation issues
    uint32_t issue_count;          // Number of issues found
    uint32_t max_issues;           // Maximum issues that can be stored

    // Statistics
    uint32_t checks_passed;        // Number of checks that passed
    uint32_t checks_failed;        // Number of checks that failed
    uint32_t checks_skipped;       // Number of checks that were skipped
    uint32_t total_checks;         // Total number of checks performed

    // Performance metrics
    uint64_t validation_time_ms;   // Time spent validating
    uint64_t file_size_bytes;      // Asset file size
    float compression_ratio;       // Compression ratio (if applicable)

    // Metadata
    char validation_version[32];   // Version of validation system
    uint64_t timestamp;            // When validation was performed
} asset_validation_result_t;

// Validation configuration
typedef struct {
    qboolean enable_detailed_logging;     // Enable verbose logging
    qboolean enable_auto_fix;            // Enable automatic fixes
    qboolean enable_strict_mode;         // Strict validation (no warnings)
    qboolean enable_performance_checks;  // Enable performance validation
    qboolean enable_quality_checks;      // Enable quality validation

    // Thresholds
    uint32_t max_texture_size;           // Maximum texture size
    uint32_t min_texture_size;           // Minimum texture size
    float max_compression_ratio;         // Maximum allowed compression
    uint32_t max_vertices_per_mesh;      // Maximum vertices per mesh
    uint32_t max_polygons_per_model;     // Maximum polygons per model

    // Quality thresholds
    float min_texture_quality;           // Minimum texture quality score
    float min_audio_quality;             // Minimum audio quality score
    uint32_t min_samples_per_second;     // Minimum audio sample rate

    // Platform-specific settings
    char target_platform[32];            // Target platform for validation
    qboolean check_platform_specific;    // Check platform-specific issues
} validation_config_t;

// Asset validation system
typedef struct {
    qboolean initialized;
    validation_config_t config;

    // Results storage
    asset_validation_result_t* results;
    uint32_t result_count;
    uint32_t max_results;

    // Statistics
    uint32_t total_assets_validated;
    uint32_t assets_passed;
    uint32_t assets_with_warnings;
    uint32_t assets_with_errors;
    uint32_t assets_critical;

    // Issue tracking
    uint32_t total_issues_found;
    uint32_t issues_by_severity[4];      // Info, Warning, Error, Critical
    uint32_t issues_by_category[CHECK_COUNT];

    // Performance tracking
    uint64_t total_validation_time_ms;
    float average_validation_time_ms;
} asset_validation_system_t;

extern asset_validation_system_t asset_validation;

// Asset Validation API
qboolean AssetValidation_Init(void);
void AssetValidation_Shutdown(void);

// Validation Configuration
void AssetValidation_SetConfig(const validation_config_t* config);
const validation_config_t* AssetValidation_GetConfig(void);
void AssetValidation_ResetConfig(void);

// Asset Validation
asset_validation_result_t* AssetValidation_ValidateAsset(const char* asset_path,
                                                       const char* asset_type);
qboolean AssetValidation_ValidateTexture(const char* texture_path,
                                       asset_validation_result_t* result);
qboolean AssetValidation_ValidateModel(const char* model_path,
                                     asset_validation_result_t* result);
qboolean AssetValidation_ValidateSound(const char* sound_path,
                                     asset_validation_result_t* result);
qboolean AssetValidation_ValidateShader(const char* shader_path,
                                      asset_validation_result_t* result);
qboolean AssetValidation_ValidateMaterial(const char* material_path,
                                        asset_validation_result_t* result);

// Batch Validation
uint32_t AssetValidation_ValidateDirectory(const char* directory_path,
                                         const char* asset_type_filter);
uint32_t AssetValidation_ValidateAllAssets(void);

// Result Management
uint32_t AssetValidation_GetResults(asset_validation_result_t** results);
asset_validation_result_t* AssetValidation_GetResult(const char* asset_path);
qboolean AssetValidation_SaveResults(const char* filename);
qboolean AssetValidation_LoadResults(const char* filename);
void AssetValidation_ClearResults(void);

// Issue Management
qboolean AssetValidation_AddIssue(asset_validation_result_t* result,
                                const char* description,
                                const char* recommendation,
                                issue_severity_t severity,
                                validation_check_t category,
                                const char* file_path,
                                uint32_t line_number,
                                qboolean auto_fixable);

// Auto-fix functionality
qboolean AssetValidation_CanAutoFix(asset_validation_result_t* result);
uint32_t AssetValidation_AutoFixAsset(asset_validation_result_t* result);
uint32_t AssetValidation_AutoFixAll(void);

// Reporting and Statistics
qboolean AssetValidation_GenerateReport(const char* output_file,
                                      const char* format); // "text", "html", "json"
qboolean AssetValidation_GenerateSummary(const char* output_file);
void AssetValidation_PrintStatistics(void);
void AssetValidation_PrintResults(void);

// Utility Functions
const char* AssetValidation_GetResultString(validation_result_t result);
const char* AssetValidation_GetSeverityString(issue_severity_t severity);
const char* AssetValidation_GetCheckString(validation_check_t check);
qboolean AssetValidation_IsAssetTypeSupported(const char* asset_type);
qboolean AssetValidation_GetAssetTypeFromPath(const char* path, char* asset_type, size_t size);

// Quality Metrics
float AssetValidation_CalculateTextureQuality(const char* texture_path);
float AssetValidation_CalculateAudioQuality(const char* audio_path);
float AssetValidation_CalculateModelQuality(const char* model_path);

// Performance Metrics
uint64_t AssetValidation_GetAssetLoadTime(const char* asset_path);
uint64_t AssetValidation_GetAssetMemoryUsage(const char* asset_path);
float AssetValidation_GetAssetCompressionRatio(const char* asset_path);

// Platform Compatibility
qboolean AssetValidation_CheckPlatformCompatibility(const char* asset_path,
                                                  const char* platform);
qboolean AssetValidation_OptimizeForPlatform(const char* asset_path,
                                           const char* platform);

// Console Commands
void AssetValidation_Status_f(void);
void AssetValidation_Validate_f(void);
void AssetValidation_BatchValidate_f(void);
void AssetValidation_Report_f(void);
void AssetValidation_AutoFix_f(void);
void AssetValidation_Stats_f(void);

#endif // __ASSET_VALIDATION_H__
