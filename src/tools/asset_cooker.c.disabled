/*
===============================================================================
Asset Cooker Tool

Command-line tool for cooking assets (textures, models, etc.) for the id Tech 3 engine.
Supports KTX2 and BasisU compression formats.

Usage:
  asset_cooker [options] <input_file> <output_file>

Options:
  --format <fmt>     Output format (ktx2, basisu, dxt, etc.)
  --quality <q>      Quality level (potato, low, medium, high, ultra)
  --platform <p>     Target platform (desktop, mobile, console, web)
  --generate-mips    Generate mipmaps
  --compress-normal  Compress normal maps
  --help             Show this help

Example:
  asset_cooker --format ktx2 --quality high --generate-mips texture.png texture.ktx2
===============================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../common/asset_cooking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static void print_usage(const char* program_name) {
    fprintf(stderr,
        "Asset Cooker Tool - id Tech 3 Asset Pipeline\n\n"
        "Usage: %s [options] <input_file> <output_file>\n\n"
        "Options:\n"
        "  --format <fmt>     Output format (ktx2, basisu, dxt, etc.)\n"
        "  --quality <q>      Quality level (potato, low, medium, high, ultra)\n"
        "  --platform <p>     Target platform (desktop, mobile, console, web)\n"
        "  --generate-mips    Generate mipmaps\n"
        "  --compress-normal  Compress normal maps\n"
        "  --help             Show this help\n\n"
        "Example:\n"
        "  %s --format ktx2 --quality high --generate-mips texture.png texture.ktx2\n",
        program_name, program_name);
}

int main(int argc, char* argv[]) {
    // Initialize common systems
    Com_Init("");

    // Default options
    asset_type_t asset_type = ASSET_TYPE_TEXTURE;
    cook_quality_t quality = COOK_QUALITY_HIGH;
    cook_platform_t platform = COOK_PLATFORM_DESKTOP;
    char compression_format[32] = "KTX2";
    qboolean generate_mips = qfalse;
    qboolean compress_normals = qfalse;

    // Parse command line arguments
    struct option long_options[] = {
        {"format", required_argument, 0, 'f'},
        {"quality", required_argument, 0, 'q'},
        {"platform", required_argument, 0, 'p'},
        {"generate-mips", no_argument, 0, 'm'},
        {"compress-normal", no_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "f:q:p:mnh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'f':
                Q_strncpyz(compression_format, optarg, sizeof(compression_format));
                break;
            case 'q':
                if (strcmp(optarg, "potato") == 0) quality = COOK_QUALITY_POTATO;
                else if (strcmp(optarg, "low") == 0) quality = COOK_QUALITY_LOW;
                else if (strcmp(optarg, "medium") == 0) quality = COOK_QUALITY_MEDIUM;
                else if (strcmp(optarg, "high") == 0) quality = COOK_QUALITY_HIGH;
                else if (strcmp(optarg, "ultra") == 0) quality = COOK_QUALITY_ULTRA;
                else {
                    fprintf(stderr, "Invalid quality level: %s\n", optarg);
                    return 1;
                }
                break;
            case 'p':
                if (strcmp(optarg, "desktop") == 0) platform = COOK_PLATFORM_DESKTOP;
                else if (strcmp(optarg, "mobile") == 0) platform = COOK_PLATFORM_MOBILE;
                else if (strcmp(optarg, "console") == 0) platform = COOK_PLATFORM_CONSOLE;
                else if (strcmp(optarg, "web") == 0) platform = COOK_PLATFORM_WEB;
                else {
                    fprintf(stderr, "Invalid platform: %s\n", optarg);
                    return 1;
                }
                break;
            case 'm':
                generate_mips = qtrue;
                break;
            case 'n':
                compress_normals = qtrue;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Check for required arguments
    if (optind + 2 > argc) {
        fprintf(stderr, "Error: input and output files required\n\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* input_file = argv[optind];
    const char* output_file = argv[optind + 1];

    printf("Asset Cooker - Cooking %s to %s\n", input_file, output_file);
    printf("Format: %s, Quality: %d, Platform: %d\n",
           compression_format, quality, platform);

    // Initialize asset cooking system
    if (!AssetCooking_Init()) {
        fprintf(stderr, "Failed to initialize asset cooking system\n");
        return 1;
    }

    // Create cooking job
    cook_job_t job = {0};
    job.asset_type = asset_type;
    Q_strncpyz(job.source_path, input_file, sizeof(job.source_path));
    Q_strncpyz(job.output_path, output_file, sizeof(job.output_path));
    job.priority = COOK_PRIORITY_NORMAL;

    // Create type-specific options
    if (asset_type == ASSET_TYPE_TEXTURE) {
        texture_cook_options_t* tex_opts = AssetCooking_CreateTextureOptions(quality, platform);
        if (!tex_opts) {
            fprintf(stderr, "Failed to create texture cooking options\n");
            return 1;
        }

        // Override compression format
        Q_strncpyz(tex_opts->compression_format, compression_format, sizeof(tex_opts->compression_format));

        // Apply command-line options
        tex_opts->generate_mipmaps = generate_mips;
        tex_opts->compress_normal_maps = compress_normals;

        job.type_specific_options = tex_opts;
    }

    // Submit cooking job
    if (!AssetCooking_SubmitJob(&job)) {
        fprintf(stderr, "Failed to submit cooking job\n");
        return 1;
    }

    // Process jobs
    cook_job_t* processed_job;
    while ((processed_job = AssetCooking_GetCompletedJob()) != NULL) {
        if (processed_job->status == COOK_STATUS_SUCCESS) {
            printf("Successfully cooked asset: %s -> %s\n",
                   processed_job->source_path, processed_job->output_path);
        } else {
            fprintf(stderr, "Failed to cook asset: %s (error: %d)\n",
                    processed_job->source_path, processed_job->error_code);
            return 1;
        }
    }

    // Validate cooked asset
    asset_validation_result_t validation_result;
    if (AssetValidation_ValidateAsset(output_file, "texture")) {
        printf("Asset validation passed for: %s\n", output_file);

        // Print any issues found
        for (uint32_t i = 0; i < validation_result.issue_count; i++) {
            asset_validation_issue_t* issue = &validation_result.issues[i];
            const char* severity_str = (issue->severity == ISSUE_CRITICAL) ? "CRITICAL" :
                                     (issue->severity == ISSUE_ERROR) ? "ERROR" :
                                     (issue->severity == ISSUE_WARNING) ? "WARNING" : "INFO";
            printf("[%s] %s: %s\n", severity_str, issue->check_type, issue->message);
        }
    } else {
        fprintf(stderr, "Asset validation failed for: %s\n", output_file);
        return 1;
    }

    // Cleanup
    AssetCooking_Shutdown();
    AssetValidation_Shutdown();

    printf("Asset cooking completed successfully!\n");
    return 0;
}