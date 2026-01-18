/*
===============================================================================

Asset Cooker - Command Line Tool

Command-line interface for the automated asset cooking pipeline.

===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// Include the asset cooking system
#include "../src/common/asset_cooking.h"
#include "../src/common/q_shared.h"

// Command line options
typedef struct {
    char source_dir[512];
    char output_dir[512];
    char cache_dir[512];
    cook_quality_t quality;
    cook_platform_t platform;
    qboolean batch_mode;
    qboolean verbose;
    qboolean force_recook;
    char specific_asset[512];
    asset_type_t asset_type_filter;
    qboolean list_only;
} cooker_options_t;

// Global options
static cooker_options_t cooker_opts;

// Forward declarations
static void print_usage(const char* program_name);
static qboolean parse_arguments(int argc, char* argv[]);
static qboolean initialize_cooking_system(void);
static qboolean scan_directory(const char* dir_path, asset_type_t filter_type);
static qboolean cook_single_asset(const char* asset_path, asset_type_t type);
static void print_statistics(void);
static asset_type_t detect_asset_type(const char* filename);

/*
==============
main
==============
*/
int main(int argc, char* argv[]) {
    printf("Asset Cooker v1.0 - idtech3 Asset Processing Tool\n");
    printf("================================================\n\n");

    // Initialize defaults
    memset(&cooker_opts, 0, sizeof(cooker_opts));
    cooker_opts.quality = COOK_QUALITY_MEDIUM;
    cooker_opts.platform = COOK_PLATFORM_DESKTOP;
    cooker_opts.asset_type_filter = ASSET_TYPE_COUNT; // No filter

    // Parse command line arguments
    if (!parse_arguments(argc, argv)) {
        return 1;
    }

    // Initialize cooking system
    if (!initialize_cooking_system()) {
        fprintf(stderr, "Failed to initialize cooking system\n");
        return 1;
    }

    // Set cooking parameters
    AssetCooking_SetQuality(cooker_opts.quality);
    AssetCooking_SetPlatform(cooker_opts.platform);

    if (cooker_opts.verbose) {
        printf("Configuration:\n");
        printf("  Source: %s\n", cooker_opts.source_dir);
        printf("  Output: %s\n", cooker_opts.output_dir);
        printf("  Cache:  %s\n", cooker_opts.cache_dir);
        printf("  Quality: %s\n", cooker_opts.quality == COOK_QUALITY_POTATO ? "potato" :
                                   cooker_opts.quality == COOK_QUALITY_LOW ? "low" :
                                   cooker_opts.quality == COOK_QUALITY_MEDIUM ? "medium" :
                                   cooker_opts.quality == COOK_QUALITY_HIGH ? "high" : "ultra");
        printf("  Platform: %s\n", cooker_opts.platform == COOK_PLATFORM_DESKTOP ? "desktop" :
                                    cooker_opts.platform == COOK_PLATFORM_MOBILE ? "mobile" :
                                    cooker_opts.platform == COOK_PLATFORM_CONSOLE ? "console" : "web");
        printf("\n");
    }

    // Process assets
    qboolean success = qtrue;

    if (cooker_opts.specific_asset[0]) {
        // Cook specific asset
        asset_type_t type = detect_asset_type(cooker_opts.specific_asset);
        if (type == ASSET_TYPE_COUNT) {
            fprintf(stderr, "Unable to detect asset type for: %s\n", cooker_opts.specific_asset);
            success = qfalse;
        } else {
            success = cook_single_asset(cooker_opts.specific_asset, type);
        }
    } else if (cooker_opts.list_only) {
        // Just list assets
        printf("Scanning directory for assets...\n");
        success = scan_directory(cooker_opts.source_dir, cooker_opts.asset_type_filter);
        printf("Scan complete.\n");
    } else {
        // Batch cook all assets
        printf("Starting batch cooking process...\n");
        success = scan_directory(cooker_opts.source_dir, cooker_opts.asset_type_filter);
        if (success) {
            success = AssetCooking_ProcessAllJobs();
        }
    }

    // Print statistics
    print_statistics();

    // Cleanup
    AssetCooking_Shutdown();

    printf("\nAsset cooking %s\n", success ? "completed successfully" : "failed");
    return success ? 0 : 1;
}

/*
==============
print_usage
==============
*/
static void print_usage(const char* program_name) {
    printf("Usage: %s [options] <source_directory>\n\n", program_name);
    printf("Options:\n");
    printf("  -o, --output <dir>      Output directory for cooked assets\n");
    printf("  -c, --cache <dir>       Cache directory for intermediate files\n");
    printf("  -q, --quality <level>   Cooking quality (potato, low, medium, high, ultra)\n");
    printf("  -p, --platform <plat>   Target platform (desktop, mobile, console, web)\n");
    printf("  -a, --asset <file>      Cook specific asset only\n");
    printf("  -t, --type <type>       Asset type filter (texture, model, sound, shader, material)\n");
    printf("  -f, --force             Force recook all assets\n");
    printf("  -l, --list              List assets without cooking\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -h, --help              Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s -o cooked_assets assets/\n", program_name);
    printf("  %s -q high -p mobile assets/\n", program_name);
    printf("  %s -a assets/textures/example.tga\n", program_name);
    printf("  %s -t texture -l assets/\n", program_name);
}

/*
==============
parse_arguments
==============
*/
static qboolean parse_arguments(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"cache", required_argument, 0, 'c'},
        {"quality", required_argument, 0, 'q'},
        {"platform", required_argument, 0, 'p'},
        {"asset", required_argument, 0, 'a'},
        {"type", required_argument, 0, 't'},
        {"force", no_argument, 0, 'f'},
        {"list", no_argument, 0, 'l'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "o:c:q:p:a:t:flvh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'o':
                strncpy(cooker_opts.output_dir, optarg, sizeof(cooker_opts.output_dir) - 1);
                break;
            case 'c':
                strncpy(cooker_opts.cache_dir, optarg, sizeof(cooker_opts.cache_dir) - 1);
                break;
            case 'q':
                if (strcmp(optarg, "potato") == 0) cooker_opts.quality = COOK_QUALITY_POTATO;
                else if (strcmp(optarg, "low") == 0) cooker_opts.quality = COOK_QUALITY_LOW;
                else if (strcmp(optarg, "medium") == 0) cooker_opts.quality = COOK_QUALITY_MEDIUM;
                else if (strcmp(optarg, "high") == 0) cooker_opts.quality = COOK_QUALITY_HIGH;
                else if (strcmp(optarg, "ultra") == 0) cooker_opts.quality = COOK_QUALITY_ULTRA;
                else {
                    fprintf(stderr, "Invalid quality level: %s\n", optarg);
                    return qfalse;
                }
                break;
            case 'p':
                if (strcmp(optarg, "desktop") == 0) cooker_opts.platform = COOK_PLATFORM_DESKTOP;
                else if (strcmp(optarg, "mobile") == 0) cooker_opts.platform = COOK_PLATFORM_MOBILE;
                else if (strcmp(optarg, "console") == 0) cooker_opts.platform = COOK_PLATFORM_CONSOLE;
                else if (strcmp(optarg, "web") == 0) cooker_opts.platform = COOK_PLATFORM_WEB;
                else {
                    fprintf(stderr, "Invalid platform: %s\n", optarg);
                    return qfalse;
                }
                break;
            case 'a':
                strncpy(cooker_opts.specific_asset, optarg, sizeof(cooker_opts.specific_asset) - 1);
                break;
            case 't':
                if (strcmp(optarg, "texture") == 0) cooker_opts.asset_type_filter = ASSET_TYPE_TEXTURE;
                else if (strcmp(optarg, "model") == 0) cooker_opts.asset_type_filter = ASSET_TYPE_MODEL;
                else if (strcmp(optarg, "sound") == 0) cooker_opts.asset_type_filter = ASSET_TYPE_SOUND;
                else if (strcmp(optarg, "shader") == 0) cooker_opts.asset_type_filter = ASSET_TYPE_SHADER;
                else if (strcmp(optarg, "material") == 0) cooker_opts.asset_type_filter = ASSET_TYPE_MATERIAL;
                else {
                    fprintf(stderr, "Invalid asset type: %s\n", optarg);
                    return qfalse;
                }
                break;
            case 'f':
                cooker_opts.force_recook = qtrue;
                break;
            case 'l':
                cooker_opts.list_only = qtrue;
                break;
            case 'v':
                cooker_opts.verbose = qtrue;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                return qfalse;
        }
    }

    // Source directory is required
    if (optind >= argc) {
        fprintf(stderr, "Source directory is required\n");
        return qfalse;
    }

    strncpy(cooker_opts.source_dir, argv[optind], sizeof(cooker_opts.source_dir) - 1);

    // Set default output/cache directories if not specified
    if (!cooker_opts.output_dir[0]) {
        snprintf(cooker_opts.output_dir, sizeof(cooker_opts.output_dir),
                "%s_cooked", cooker_opts.source_dir);
    }
    if (!cooker_opts.cache_dir[0]) {
        snprintf(cooker_opts.cache_dir, sizeof(cooker_opts.cache_dir),
                "%s/.cook_cache", cooker_opts.source_dir);
    }

    return qtrue;
}

/*
==============
initialize_cooking_system
==============
*/
static qboolean initialize_cooking_system(void) {
    // Create output and cache directories
    mkdir(cooker_opts.output_dir, 0755);
    mkdir(cooker_opts.cache_dir, 0755);

    if (!AssetCooking_Init(cooker_opts.source_dir, cooker_opts.output_dir, cooker_opts.cache_dir)) {
        return qfalse;
    }

    AssetCooking_SetParallelJobs(4); // Use 4 parallel jobs by default
    return qtrue;
}

/*
==============
scan_directory
==============
*/
static qboolean scan_directory(const char* dir_path, asset_type_t filter_type) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Failed to open directory: %s\n", dir_path);
        return qfalse;
    }

    struct dirent* entry;
    char full_path[1024];
    uint32_t asset_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden files and current/parent directories
        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
            asset_type_t type = detect_asset_type(entry->d_name);

            if (type != ASSET_TYPE_COUNT &&
                (filter_type == ASSET_TYPE_COUNT || type == filter_type)) {

                if (cooker_opts.list_only) {
                    printf("  %s (%s)\n", full_path,
                           type == ASSET_TYPE_TEXTURE ? "texture" :
                           type == ASSET_TYPE_MODEL ? "model" :
                           type == ASSET_TYPE_SOUND ? "sound" :
                           type == ASSET_TYPE_SHADER ? "shader" :
                           type == ASSET_TYPE_MATERIAL ? "material" : "unknown");
                } else {
                    if (!cook_single_asset(full_path, type)) {
                        fprintf(stderr, "Failed to cook: %s\n", full_path);
                    }
                }
                asset_count++;
            }
        } else if (S_ISDIR(st.st_mode) && strcmp(entry->d_name, ".") != 0 &&
                   strcmp(entry->d_name, "..") != 0) {
            // Recursively scan subdirectories
            scan_directory(full_path, filter_type);
        }
    }

    closedir(dir);

    if (cooker_opts.verbose && cooker_opts.list_only) {
        printf("Found %u assets\n", asset_count);
    }

    return qtrue;
}

/*
==============
cook_single_asset
==============
*/
static qboolean cook_single_asset(const char* asset_path, asset_type_t type) {
    cook_job_t* job = AssetCooking_CreateJob(asset_path, type,
                                           cooker_opts.quality, cooker_opts.platform);
    if (!job) {
        fprintf(stderr, "Failed to create cooking job for: %s\n", asset_path);
        return qfalse;
    }

    job->force_recook = cooker_opts.force_recook;

    if (!AssetCooking_AddJob(job)) {
        fprintf(stderr, "Failed to add cooking job for: %s\n", asset_path);
        return qfalse;
    }

    if (cooker_opts.verbose) {
        printf("Added cooking job: %s\n", asset_path);
    }

    return qtrue;
}

/*
==============
detect_asset_type
==============
*/
static asset_type_t detect_asset_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return ASSET_TYPE_COUNT;

    ext++; // Skip the dot

    if (strcasecmp(ext, "tga") == 0 || strcasecmp(ext, "png") == 0 ||
        strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "ktx2") == 0) {
        return ASSET_TYPE_TEXTURE;
    }

    if (strcasecmp(ext, "md3") == 0 || strcasecmp(ext, "md5") == 0 ||
        strcasecmp(ext, "obj") == 0 || strcasecmp(ext, "fbx") == 0 ||
        strcasecmp(ext, "dae") == 0 || strcasecmp(ext, "iqm") == 0) {
        return ASSET_TYPE_MODEL;
    }

    if (strcasecmp(ext, "wav") == 0 || strcasecmp(ext, "ogg") == 0 ||
        strcasecmp(ext, "mp3") == 0) {
        return ASSET_TYPE_SOUND;
    }

    if (strcasecmp(ext, "shader") == 0) {
        return ASSET_TYPE_SHADER;
    }

    if (strcasecmp(ext, "material") == 0) {
        return ASSET_TYPE_MATERIAL;
    }

    return ASSET_TYPE_COUNT;
}

/*
==============
print_statistics
==============
*/
static void print_statistics(void) {
    cooking_statistics_t stats;
    AssetCooking_GetStatistics(&stats);

    printf("\nCooking Statistics:\n");
    printf("==================\n");
    printf("Total assets: %u\n", stats.total_assets);
    printf("Processed: %u\n", stats.processed_assets);
    printf("Failed: %u\n", stats.failed_assets);
    printf("Input size: %.2f MB\n", stats.total_input_size / (1024.0 * 1024.0));
    printf("Output size: %.2f MB\n", stats.total_output_size / (1024.0 * 1024.0));
    printf("Compression ratio: %.2f%%\n",
           stats.total_input_size > 0 ?
           (1.0f - (float)stats.total_output_size / stats.total_input_size) * 100.0f : 0.0f);
    printf("Processing time: %.2f seconds\n", stats.processing_time_ms / 1000.0f);

    printf("\nAssets by type:\n");
    for (int i = 0; i < ASSET_TYPE_COUNT; i++) {
        if (stats.assets_by_type[i] > 0) {
            printf("  %s: %u\n",
                   i == ASSET_TYPE_TEXTURE ? "Textures" :
                   i == ASSET_TYPE_MODEL ? "Models" :
                   i == ASSET_TYPE_SOUND ? "Sounds" :
                   i == ASSET_TYPE_SHADER ? "Shaders" :
                   i == ASSET_TYPE_MATERIAL ? "Materials" : "Other",
                   stats.assets_by_type[i]);
        }
    }
}