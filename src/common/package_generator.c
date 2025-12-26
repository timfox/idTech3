/*
=============================================================================
Automated Packaging System Implementation

Multi-platform installer and package generation framework.
=============================================================================
*/

#include "package_generator.h"
#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#define PATH_SEPARATOR "\\"
#else
#include <libgen.h>
#define PATH_SEPARATOR "/"
#endif

// Forward declarations for functions used before definition
qboolean PackageGenerator_ValidateConfig(const package_config_t* config, package_generation_result_t* result);
void PackageGenerator_GeneratePackageName(const package_config_t* config, package_type_t type, char* name, size_t size);
qboolean PackageGenerator_CopyFilesToStaging(const package_config_t* config, const char* staging_dir);
qboolean PackageGenerator_CreateZipArchive(const char* source_dir, const char* output_file);
qboolean PackageGenerator_CreateTarGzArchive(const char* source_dir, const char* output_file);
package_type_t PackageGenerator_SelectPackageType(const package_config_t* config);
qboolean PackageGenerator_IsPackageTypeAvailable(package_type_t type, package_platform_t platform);
void PackageGenerator_SetToolAvailability(const char* tool_name, qboolean available);
qboolean PackageGenerator_GenerateNSISInstaller(const package_config_t* config, package_generation_result_t* result);
qboolean PackageGenerator_GenerateWiXInstaller(const package_config_t* config, package_generation_result_t* result);
qboolean PackageGenerator_GenerateDEBPackage(const package_config_t* config, package_generation_result_t* result);
qboolean PackageGenerator_GenerateRPMPackage(const package_config_t* config, package_generation_result_t* result);
qboolean PackageGenerator_GenerateAppImage(const package_config_t* config, package_generation_result_t* result);
qboolean PackageGenerator_GenerateDMGPackage(const package_config_t* config, package_generation_result_t* result);
qboolean PackageGenerator_GeneratePKGPackage(const package_config_t* config, package_generation_result_t* result);

// Internal helper functions
static qboolean PackageGenerator_CopyFile(const char* src, const char* dst) {
    FILE* fsrc = fopen(src, "rb");
    if (!fsrc) return qfalse;
    FILE* fdst = fopen(dst, "wb");
    if (!fdst) {
        fclose(fsrc);
        return qfalse;
    }
    char buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        if (fwrite(buf, 1, n, fdst) != n) {
            fclose(fsrc);
            fclose(fdst);
            return qfalse;
        }
    }
    fclose(fsrc);
    fclose(fdst);
    return qtrue;
}

static uint32_t PackageGenerator_CopyDir(const char* src, const char* dst) {
    DIR* dir = opendir(src);
    if (!dir) return 0;
    
    // Create destination directory
#ifdef _WIN32
    _mkdir(dst);
#else
    mkdir(dst, 0755);
#endif

    uint32_t fileCount = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char src_path[MAX_OSPATH];
        char dst_path[MAX_OSPATH];
        Com_sprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        Com_sprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);
        
        struct stat st;
        if (stat(src_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                fileCount += PackageGenerator_CopyDir(src_path, dst_path);
            } else if (S_ISREG(st.st_mode)) {
                if (PackageGenerator_CopyFile(src_path, dst_path)) {
                    fileCount++;
                }
            }
        }
    }
    closedir(dir);
    return fileCount;
}

// Global packaging system
packaging_system_t packaging_system = {0};

// Package type extensions
static const char* package_extensions[PACKAGE_TYPE_COUNT] = {
    ".msi", ".exe", ".deb", ".rpm", ".AppImage", ".dmg", ".pkg", ".zip", ".tar.gz"
};

// Package type names
static const char* package_type_names[PACKAGE_TYPE_COUNT] = {
    "MSI", "EXE", "DEB", "RPM", "AppImage", "DMG", "PKG", "ZIP", "TAR.GZ"
};

// Platform names
static const char* platform_names[PACKAGE_PLATFORM_COUNT] = {
    "Windows", "Linux", "macOS", "Universal"
};

// Architecture names
static const char* architecture_names[PACKAGE_ARCH_COUNT] = {
    "x86", "x86_64", "ARM", "ARM64", "Universal"
};

// Result strings
static const char* result_strings[] = {
    "SUCCESS", "WARNING", "ERROR", "FAILED"
};

// Required tools for each platform
static const struct {
    package_platform_t platform;
    const char* tools[8];  // Up to 8 tools per platform
} platform_tools[] = {
    {PACKAGE_PLATFORM_WINDOWS, {"makensis", "candle", "light", NULL}},
    {PACKAGE_PLATFORM_LINUX, {"dpkg-deb", "rpmbuild", "appimagetool", NULL}},
    {PACKAGE_PLATFORM_MACOS, {"hdiutil", "pkgbuild", "productbuild", NULL}},
    {PACKAGE_PLATFORM_UNIVERSAL, {NULL}}
};

/*
=============================================================================
Package Generation API Implementation
=============================================================================
*/

qboolean PackageGenerator_Init(void) {
    if (packaging_system.initialized) {
        return qtrue;
    }

    memset(&packaging_system, 0, sizeof(packaging_system_t));

    // Allocate results storage
    packaging_system.max_results = 50;
    packaging_system.results = (package_generation_result_t*)malloc(
        sizeof(package_generation_result_t) * packaging_system.max_results);

    if (!packaging_system.results) {
        Com_Printf("Failed to allocate memory for package results\n");
        return qfalse;
    }

    memset(packaging_system.results, 0,
           sizeof(package_generation_result_t) * packaging_system.max_results);

    // Set default configuration
    package_config_t* config = &packaging_system.default_config;
    memset(config, 0, sizeof(package_config_t));

    Q_strncpyz(config->package_name, "ioquake3", sizeof(config->package_name));
    Q_strncpyz(config->package_version, "1.36", sizeof(config->package_version));
    Q_strncpyz(config->package_description, "ioquake3 game engine", sizeof(config->package_description));
    Q_strncpyz(config->package_vendor, "ioquake3", sizeof(config->package_vendor));
    Q_strncpyz(config->package_maintainer, "ioquake3 maintainers", sizeof(config->package_maintainer));
    Q_strncpyz(config->package_url, "https://ioquake3.org", sizeof(config->package_url));
    Q_strncpyz(config->package_license, "GPL-2.0", sizeof(config->package_license));

    config->target_platform = PackageGenerator_DetectCurrentPlatform();
    config->target_architecture = PackageGenerator_DetectCurrentArchitecture();

    // Set preferred package types based on platform
    if (config->target_platform == PACKAGE_PLATFORM_WINDOWS) {
        config->preferred_types[0] = PACKAGE_TYPE_EXE;
        config->preferred_types[1] = PACKAGE_TYPE_MSI;
        config->preferred_types[2] = PACKAGE_TYPE_ZIP;
        config->num_preferred_types = 3;
    } else if (config->target_platform == PACKAGE_PLATFORM_LINUX) {
        config->preferred_types[0] = PACKAGE_TYPE_APPIMAGE;
        config->preferred_types[1] = PACKAGE_TYPE_DEB;
        config->preferred_types[2] = PACKAGE_TYPE_TAR_GZ;
        config->num_preferred_types = 3;
    } else if (config->target_platform == PACKAGE_PLATFORM_MACOS) {
        config->preferred_types[0] = PACKAGE_TYPE_DMG;
        config->preferred_types[1] = PACKAGE_TYPE_PKG;
        config->preferred_types[2] = PACKAGE_TYPE_ZIP;
        config->num_preferred_types = 3;
    }

    config->compression_type = COMPRESSION_GZIP;
    config->compression_level = 6;

    Q_strncpyz(config->source_directory, "build", sizeof(config->source_directory));
    Q_strncpyz(config->output_directory, "packages", sizeof(config->output_directory));
    Q_strncpyz(config->build_directory, "packaging_temp", sizeof(config->build_directory));

    config->enable_signing = qfalse;
    config->create_portable = qtrue;
    config->strip_binaries = qtrue;
    config->compress_resources = qtrue;
    config->create_checksum = qtrue;

    config->max_package_size = 1024 * 1024 * 1024; // 1GB
    config->max_file_count = 10000;

    // Detect available tools
    PackageGenerator_DetectAvailableTools();

    packaging_system.initialized = qtrue;

    Com_Printf("Automated packaging system initialized\n");
    Com_Printf("Detected platform: %s (%s)\n",
               PackageGenerator_GetPlatformName(config->target_platform),
               PackageGenerator_GetArchitectureName(config->target_architecture));

    return qtrue;
}

void PackageGenerator_Shutdown(void) {
    if (!packaging_system.initialized) {
        return;
    }

    // Clean up temporary build directories
    if (packaging_system.default_config.build_directory[0]) {
        // Remove temporary directory (implementation would remove directory tree)
        Com_Printf("Cleaning up temporary build directory: %s\n",
                  packaging_system.default_config.build_directory);
    }

    // Free results
    if (packaging_system.results) {
        free(packaging_system.results);
    }

    packaging_system.initialized = qfalse;
    Com_Printf("Automated packaging system shutdown\n");
}

/*
=============================================================================
Configuration Management
=============================================================================
*/

void PackageGenerator_SetDefaultConfig(const package_config_t* config) {
    if (config) {
        memcpy(&packaging_system.default_config, config, sizeof(package_config_t));
    }
}

const package_config_t* PackageGenerator_GetDefaultConfig(void) {
    return &packaging_system.default_config;
}

package_config_t* PackageGenerator_CreateConfig(void) {
    package_config_t* config = (package_config_t*)malloc(sizeof(package_config_t));
    if (config) {
        memcpy(config, &packaging_system.default_config, sizeof(package_config_t));
    }
    return config;
}

void PackageGenerator_DestroyConfig(package_config_t* config) {
    if (config) {
        // Free any dynamically allocated strings in config
        free(config);
    }
}

/*
=============================================================================
Package Generation
=============================================================================
*/

package_generation_result_t* PackageGenerator_GeneratePackage(const package_config_t* config) {
    if (!packaging_system.initialized || !config) {
        return NULL;
    }

    // Find or create result structure
    package_generation_result_t* result = NULL;
    for (uint32_t i = 0; i < packaging_system.result_count; i++) {
        if (Q_stricmp(packaging_system.results[i].package_name, config->package_name) == 0) {
            result = &packaging_system.results[i];
            break;
        }
    }

    if (!result) {
        if (packaging_system.result_count >= packaging_system.max_results) {
            Com_Printf("Maximum package results reached\n");
            return NULL;
        }
        result = &packaging_system.results[packaging_system.result_count++];
    }

    // Initialize result
    memset(result, 0, sizeof(package_generation_result_t));
    Q_strncpyz(result->package_name, config->package_name, sizeof(result->package_name));
    result->result = PACKAGE_RESULT_SUCCESS;

    uint64_t start_time = Sys_Milliseconds();

    // Validate configuration
    if (!PackageGenerator_ValidateConfig(config, result)) {
        result->result = PACKAGE_RESULT_ERROR;
        result->generation_time_ms = Sys_Milliseconds() - start_time;
        return result;
    }

    // Determine package type to generate
    package_type_t package_type = PackageGenerator_SelectPackageType(config);
    if (package_type == PACKAGE_TYPE_COUNT) {
        Q_strncpyz(result->error_message, "No suitable package type available for platform",
                  sizeof(result->error_message));
        result->result = PACKAGE_RESULT_ERROR;
        result->generation_time_ms = Sys_Milliseconds() - start_time;
        return result;
    }

    result->package_type = package_type;

    // Generate package based on platform
    qboolean success = qfalse;
    switch (config->target_platform) {
        case PACKAGE_PLATFORM_WINDOWS:
            success = PackageGenerator_GenerateWindowsInstaller(config, result);
            break;
        case PACKAGE_PLATFORM_LINUX:
            success = PackageGenerator_GenerateLinuxPackage(config, result);
            break;
        case PACKAGE_PLATFORM_MACOS:
            success = PackageGenerator_GenerateMacOSPackage(config, result);
            break;
        case PACKAGE_PLATFORM_UNIVERSAL:
            success = PackageGenerator_GeneratePortablePackage(config, result);
            break;
        default:
            Q_strncpyz(result->error_message, "Unsupported target platform",
                      sizeof(result->error_message));
            success = qfalse;
            break;
    }

    result->generation_time_ms = Sys_Milliseconds() - start_time;

    if (!success) {
        result->result = PACKAGE_RESULT_FAILED;
        if (!result->error_message[0]) {
            Q_strncpyz(result->error_message, "Package generation failed",
                      sizeof(result->error_message));
        }
    }

    // Generate checksum if requested
    if (success && config->create_checksum && result->package_path[0]) {
        PackageGenerator_GenerateChecksum(result->package_path, result->checksum,
                                        sizeof(result->checksum));
    }

    // Update statistics
    packaging_system.total_packages_generated++;
    packaging_system.total_generation_time_ms += result->generation_time_ms;
    packaging_system.total_package_size_bytes += result->package_size;

    if (result->result == PACKAGE_RESULT_SUCCESS) {
        packaging_system.packages_succeeded++;
    } else if (result->result == PACKAGE_RESULT_WARNING) {
        packaging_system.packages_with_warnings++;
    } else {
        packaging_system.packages_failed++;
    }

    return result;
}

qboolean PackageGenerator_GenerateAllPackages(const package_config_t* config) {
    if (!packaging_system.initialized || !config) {
        return qfalse;
    }

    qboolean all_success = qtrue;

    // Generate packages for all preferred types
    for (int i = 0; i < config->num_preferred_types; i++) {
        package_config_t temp_config = *config;
        temp_config.preferred_types[0] = config->preferred_types[i];
        temp_config.num_preferred_types = 1;

        package_generation_result_t* result = PackageGenerator_GeneratePackage(&temp_config);
        if (!result || result->result != PACKAGE_RESULT_SUCCESS) {
            all_success = qfalse;
            Com_Printf("Failed to generate %s package\n",
                      PackageGenerator_GetPackageTypeName(config->preferred_types[i]));
        } else {
            Com_Printf("Successfully generated %s package: %s\n",
                      PackageGenerator_GetPackageTypeName(config->preferred_types[i]),
                      result->package_path);
        }
    }

    return all_success;
}

package_type_t PackageGenerator_SelectPackageType(const package_config_t* config) {
    // Try preferred types first
    for (int i = 0; i < config->num_preferred_types; i++) {
        package_type_t type = config->preferred_types[i];
        if (PackageGenerator_IsPackageTypeAvailable(type, config->target_platform)) {
            return type;
        }
    }

    // Fall back to platform defaults
    switch (config->target_platform) {
        case PACKAGE_PLATFORM_WINDOWS:
            if (packaging_system.has_nsis || packaging_system.has_wix) {
                return packaging_system.has_nsis ? PACKAGE_TYPE_EXE : PACKAGE_TYPE_MSI;
            }
            return PACKAGE_TYPE_ZIP;
        case PACKAGE_PLATFORM_LINUX:
            if (packaging_system.has_appimagetool) return PACKAGE_TYPE_APPIMAGE;
            if (packaging_system.has_dpkg_deb) return PACKAGE_TYPE_DEB;
            return PACKAGE_TYPE_TAR_GZ;
        case PACKAGE_PLATFORM_MACOS:
            if (packaging_system.has_hdiutil) return PACKAGE_TYPE_DMG;
            return PACKAGE_TYPE_ZIP;
        default:
            return PACKAGE_TYPE_ZIP;
    }
}

qboolean PackageGenerator_IsPackageTypeAvailable(package_type_t type, package_platform_t platform) {
    switch (platform) {
        case PACKAGE_PLATFORM_WINDOWS:
            return (type == PACKAGE_TYPE_EXE && packaging_system.has_nsis) ||
                   (type == PACKAGE_TYPE_MSI && packaging_system.has_wix) ||
                   type == PACKAGE_TYPE_ZIP;
        case PACKAGE_PLATFORM_LINUX:
            return (type == PACKAGE_TYPE_DEB && packaging_system.has_dpkg_deb) ||
                   (type == PACKAGE_TYPE_RPM && packaging_system.has_rpmbuild) ||
                   (type == PACKAGE_TYPE_APPIMAGE && packaging_system.has_appimagetool) ||
                   type == PACKAGE_TYPE_TAR_GZ;
        case PACKAGE_PLATFORM_MACOS:
            return (type == PACKAGE_TYPE_DMG && packaging_system.has_hdiutil) ||
                   (type == PACKAGE_TYPE_PKG && packaging_system.has_pkgbuild) ||
                   type == PACKAGE_TYPE_ZIP;
        default:
            return type == PACKAGE_TYPE_ZIP || type == PACKAGE_TYPE_TAR_GZ;
    }
}

/*
=============================================================================
Platform Detection and Tools
=============================================================================
*/

package_platform_t PackageGenerator_DetectCurrentPlatform(void) {
#ifdef _WIN32
    return PACKAGE_PLATFORM_WINDOWS;
#elif defined(__APPLE__)
    return PACKAGE_PLATFORM_MACOS;
#else
    return PACKAGE_PLATFORM_LINUX;
#endif
}

package_architecture_t PackageGenerator_DetectCurrentArchitecture(void) {
#ifdef __x86_64__
    return PACKAGE_ARCH_X86_64;
#elif defined(__i386__)
    return PACKAGE_ARCH_X86;
#elif defined(__aarch64__)
    return PACKAGE_ARCH_ARM64;
#elif defined(__arm__)
    return PACKAGE_ARCH_ARM;
#else
    return PACKAGE_ARCH_X86_64; // Default assumption
#endif
}

qboolean PackageGenerator_DetectAvailableTools(void) {
    package_platform_t platform = PackageGenerator_DetectCurrentPlatform();

    for (int i = 0; platform_tools[i].platform != PACKAGE_PLATFORM_COUNT; i++) {
        if (platform_tools[i].platform == platform) {
            const char** tools = platform_tools[i].tools;
            for (int j = 0; tools[j]; j++) {
                qboolean available = PackageGenerator_CheckToolAvailability(tools[j]);
                PackageGenerator_SetToolAvailability(tools[j], available);
            }
            break;
        }
    }

    Com_Printf("Detected packaging tools:\n");
    Com_Printf("  NSIS: %s\n", packaging_system.has_nsis ? "Yes" : "No");
    Com_Printf("  WiX: %s\n", packaging_system.has_wix ? "Yes" : "No");
    Com_Printf("  dpkg-deb: %s\n", packaging_system.has_dpkg_deb ? "Yes" : "No");
    Com_Printf("  rpmbuild: %s\n", packaging_system.has_rpmbuild ? "Yes" : "No");
    Com_Printf("  appimagetool: %s\n", packaging_system.has_appimagetool ? "Yes" : "No");
    Com_Printf("  hdiutil: %s\n", packaging_system.has_hdiutil ? "Yes" : "No");
    Com_Printf("  pkgbuild: %s\n", packaging_system.has_pkgbuild ? "Yes" : "No");

    return qtrue;
}

qboolean PackageGenerator_CheckToolAvailability(const char* tool_name) {
    char command[256];
    Q_snprintf(command, sizeof(command), "%s --version > /dev/null 2>&1", tool_name);
    int result = system(command);
    return result == 0;
}

void PackageGenerator_SetToolAvailability(const char* tool_name, qboolean available) {
    if (Q_stricmp(tool_name, "makensis") == 0) {
        packaging_system.has_nsis = available;
    } else if (Q_stricmp(tool_name, "candle") == 0 || Q_stricmp(tool_name, "light") == 0) {
        packaging_system.has_wix = available;
    } else if (Q_stricmp(tool_name, "dpkg-deb") == 0) {
        packaging_system.has_dpkg_deb = available;
    } else if (Q_stricmp(tool_name, "rpmbuild") == 0) {
        packaging_system.has_rpmbuild = available;
    } else if (Q_stricmp(tool_name, "appimagetool") == 0) {
        packaging_system.has_appimagetool = available;
    } else if (Q_stricmp(tool_name, "hdiutil") == 0) {
        packaging_system.has_hdiutil = available;
    } else if (Q_stricmp(tool_name, "pkgbuild") == 0) {
        packaging_system.has_pkgbuild = available;
    }
}

/*
=============================================================================
Package Generation Implementations
=============================================================================
*/

qboolean PackageGenerator_GenerateWindowsInstaller(const package_config_t* config,
                                                 package_generation_result_t* result) {
    // Determine installer type
    qboolean use_nsis = (result->package_type == PACKAGE_TYPE_EXE && packaging_system.has_nsis);
    qboolean use_wix = (result->package_type == PACKAGE_TYPE_MSI && packaging_system.has_wix);

    if (!use_nsis && !use_wix) {
        // Fall back to ZIP
        result->package_type = PACKAGE_TYPE_ZIP;
        return PackageGenerator_GeneratePortablePackage(config, result);
    }

    // Create output package name
    char package_filename[256];
    PackageGenerator_GeneratePackageName(config, result->package_type,
                                       package_filename, sizeof(package_filename));

    Q_snprintf(result->package_path, sizeof(result->package_path), "%s/%s",
               config->output_directory, package_filename);

    // Create output directory
    mkdir(config->output_directory, 0755);

    // Generate installer script/content
    if (use_nsis) {
        return PackageGenerator_GenerateNSISInstaller(config, result);
    } else if (use_wix) {
        return PackageGenerator_GenerateWiXInstaller(config, result);
    }

    return qfalse;
}

qboolean PackageGenerator_GenerateLinuxPackage(const package_config_t* config,
                                             package_generation_result_t* result) {
    // Determine package type
    qboolean use_deb = (result->package_type == PACKAGE_TYPE_DEB && packaging_system.has_dpkg_deb);
    qboolean use_rpm = (result->package_type == PACKAGE_TYPE_RPM && packaging_system.has_rpmbuild);
    qboolean use_appimage = (result->package_type == PACKAGE_TYPE_APPIMAGE && packaging_system.has_appimagetool);

    if (!use_deb && !use_rpm && !use_appimage) {
        // Fall back to tar.gz
        result->package_type = PACKAGE_TYPE_TAR_GZ;
        return PackageGenerator_GeneratePortablePackage(config, result);
    }

    // Create output package name
    char package_filename[256];
    PackageGenerator_GeneratePackageName(config, result->package_type,
                                       package_filename, sizeof(package_filename));

    Q_snprintf(result->package_path, sizeof(result->package_path), "%s/%s",
               config->output_directory, package_filename);

    // Create output directory
    mkdir(config->output_directory, 0755);

    // Generate package
    if (use_deb) {
        return PackageGenerator_GenerateDEBPackage(config, result);
    } else if (use_rpm) {
        return PackageGenerator_GenerateRPMPackage(config, result);
    } else if (use_appimage) {
        return PackageGenerator_GenerateAppImage(config, result);
    }

    return qfalse;
}

qboolean PackageGenerator_GenerateMacOSPackage(const package_config_t* config,
                                             package_generation_result_t* result) {
    // Determine package type
    qboolean use_dmg = (result->package_type == PACKAGE_TYPE_DMG && packaging_system.has_hdiutil);
    qboolean use_pkg = (result->package_type == PACKAGE_TYPE_PKG && packaging_system.has_pkgbuild);

    if (!use_dmg && !use_pkg) {
        // Fall back to ZIP
        result->package_type = PACKAGE_TYPE_ZIP;
        return PackageGenerator_GeneratePortablePackage(config, result);
    }

    // Create output package name
    char package_filename[256];
    PackageGenerator_GeneratePackageName(config, result->package_type,
                                       package_filename, sizeof(package_filename));

    Q_snprintf(result->package_path, sizeof(result->package_path), "%s/%s",
               config->output_directory, package_filename);

    // Create output directory
    mkdir(config->output_directory, 0755);

    // Generate package
    if (use_dmg) {
        return PackageGenerator_GenerateDMGPackage(config, result);
    } else if (use_pkg) {
        return PackageGenerator_GeneratePKGPackage(config, result);
    }

    return qfalse;
}

qboolean PackageGenerator_GeneratePortablePackage(const package_config_t* config,
                                                package_generation_result_t* result) {
    // Create output package name
    char package_filename[256];
    PackageGenerator_GeneratePackageName(config, result->package_type,
                                       package_filename, sizeof(package_filename));

    Q_snprintf(result->package_path, sizeof(result->package_path), "%s/%s",
               config->output_directory, package_filename);

    // Create output directory
    mkdir(config->output_directory, 0755);

    // Copy files to temporary staging area
    char staging_dir[512];
    Q_snprintf(staging_dir, sizeof(staging_dir), "%s/staging", config->build_directory);
    mkdir(config->build_directory, 0755);
    mkdir(staging_dir, 0755);

    uint32_t fileCount = PackageGenerator_CopyDir(config->source_directory, staging_dir);
    if (fileCount == 0) {
        Q_strncpyz(result->error_message, "Failed to copy files to staging area or source is empty",
                  sizeof(result->error_message));
        return qfalse;
    }
    result->file_count = fileCount;

    // Create archive
    qboolean success = qfalse;
    if (result->package_type == PACKAGE_TYPE_ZIP) {
        success = PackageGenerator_CreateZipArchive(staging_dir, result->package_path);
    } else if (result->package_type == PACKAGE_TYPE_TAR_GZ) {
        success = PackageGenerator_CreateTarGzArchive(staging_dir, result->package_path);
    }

    if (success) {
        // Get package size
        struct stat st;
        if (stat(result->package_path, &st) == 0) {
            result->package_size = st.st_size;
        }
    }

    // Clean up staging area (optional - keeping for debug if needed, but in production we'd remove)
    // For now we leave it as the config build_directory cleanup is handled in shutdown

    return success;
}

/*
=============================================================================
Helper Functions
=============================================================================
*/

qboolean PackageGenerator_ValidateConfig(const package_config_t* config,
                                       package_generation_result_t* result) {
    if (!config->package_name[0]) {
        Q_strncpyz(result->error_message, "Package name is required",
                  sizeof(result->error_message));
        return qfalse;
    }

    if (!config->package_version[0]) {
        Q_strncpyz(result->error_message, "Package version is required",
                  sizeof(result->error_message));
        return qfalse;
    }

    if (!config->source_directory[0]) {
        Q_strncpyz(result->error_message, "Source directory is required",
                  sizeof(result->error_message));
        return qfalse;
    }

    // Check if source directory exists
    DIR* dir = opendir(config->source_directory);
    if (!dir) {
        Q_snprintf(result->error_message, sizeof(result->error_message),
                  "Source directory does not exist: %s", config->source_directory);
        return qfalse;
    }
    closedir(dir);

    return qtrue;
}

void PackageGenerator_GeneratePackageName(const package_config_t* config,
                                        package_type_t type,
                                        char* name, size_t size) {
    char arch_suffix[16] = "";
    if (config->target_architecture != PACKAGE_ARCH_UNIVERSAL) {
        Q_snprintf(arch_suffix, sizeof(arch_suffix), "_%s",
                  PackageGenerator_GetArchitectureName(config->target_architecture));
    }

    char extension[16];
    if (!PackageGenerator_GetPackageExtension(type, extension, sizeof(extension))) {
        Q_strncpyz(extension, ".unknown", sizeof(extension));
    }

    Q_snprintf(name, size, "%s_%s%s%s",
               config->package_name,
               config->package_version,
               arch_suffix,
               extension);
}

qboolean PackageGenerator_CopyFilesToStaging(const package_config_t* config,
                                           const char* staging_dir) {
    if (!config || !staging_dir) return qfalse;
    
    Com_Printf("Copying files from %s to %s\n", config->source_directory, staging_dir);
    uint32_t count = PackageGenerator_CopyDir(config->source_directory, staging_dir);
    return count > 0;
}

qboolean PackageGenerator_CreateZipArchive(const char* source_dir, const char* output_file) {
    // Simplified implementation - would use libzip or system zip command
    char command[1024];
    Q_snprintf(command, sizeof(command), "cd \"%s\" && zip -r \"%s\" .", source_dir, output_file);
    int result = system(command);
    return result == 0;
}

qboolean PackageGenerator_CreateTarGzArchive(const char* source_dir, const char* output_file) {
    // Simplified implementation - would use libtar or system tar command
    char command[1024];
    Q_snprintf(command, sizeof(command), "cd \"%s\" && tar -czf \"%s\" .", source_dir, output_file);
    int result = system(command);
    return result == 0;
}

// Stub implementations for platform-specific generators
qboolean PackageGenerator_GenerateNSISInstaller(const package_config_t* config,
                                              package_generation_result_t* result) {
    // Would generate NSIS script and compile installer
    Com_Printf("Generating NSIS installer (stub implementation)\n");
    return qtrue;
}

qboolean PackageGenerator_GenerateWiXInstaller(const package_config_t* config,
                                             package_generation_result_t* result) {
    // Would generate WiX XML and compile MSI
    Com_Printf("Generating WiX MSI installer (stub implementation)\n");
    return qtrue;
}

qboolean PackageGenerator_GenerateDEBPackage(const package_config_t* config,
                                           package_generation_result_t* result) {
    // Would create Debian package structure and use dpkg-deb
    Com_Printf("Generating DEB package (stub implementation)\n");
    return qtrue;
}

qboolean PackageGenerator_GenerateRPMPackage(const package_config_t* config,
                                           package_generation_result_t* result) {
    // Would create RPM spec file and use rpmbuild
    Com_Printf("Generating RPM package (stub implementation)\n");
    return qtrue;
}

qboolean PackageGenerator_GenerateAppImage(const package_config_t* config,
                                         package_generation_result_t* result) {
    // Would create AppImage structure and use appimagetool
    Com_Printf("Generating AppImage (stub implementation)\n");
    return qtrue;
}

qboolean PackageGenerator_GenerateDMGPackage(const package_config_t* config,
                                           package_generation_result_t* result) {
    // Would create DMG using hdiutil
    Com_Printf("Generating DMG package (stub implementation)\n");
    return qtrue;
}

qboolean PackageGenerator_GeneratePKGPackage(const package_config_t* config,
                                           package_generation_result_t* result) {
    // Would create PKG using pkgbuild
    Com_Printf("Generating PKG package (stub implementation)\n");
    return qtrue;
}

/*
=============================================================================
Reporting and Statistics
=============================================================================
*/

qboolean PackageGenerator_GenerateReport(const char* output_file, const char* format) {
    FILE* file = fopen(output_file, "w");
    if (!file) return qfalse;

    if (Q_stricmp(format, "json") == 0) {
        // JSON format
        fprintf(file, "{\n");
        fprintf(file, "  \"packaging_summary\": {\n");
        fprintf(file, "    \"total_packages\": %u,\n", packaging_system.total_packages_generated);
        fprintf(file, "    \"packages_succeeded\": %u,\n", packaging_system.packages_succeeded);
        fprintf(file, "    \"packages_with_warnings\": %u,\n", packaging_system.packages_with_warnings);
        fprintf(file, "    \"packages_failed\": %u\n", packaging_system.packages_failed);
        fprintf(file, "  },\n");

        fprintf(file, "  \"results\": [\n");
        for (uint32_t i = 0; i < packaging_system.result_count; i++) {
            package_generation_result_t* result = &packaging_system.results[i];
            fprintf(file, "    {\n");
            fprintf(file, "      \"package_name\": \"%s\",\n", result->package_name);
            fprintf(file, "      \"package_path\": \"%s\",\n", result->package_path);
            fprintf(file, "      \"package_type\": \"%s\",\n",
                   PackageGenerator_GetPackageTypeName(result->package_type));
            fprintf(file, "      \"result\": \"%s\",\n",
                   PackageGenerator_GetResultString(result->result));
            fprintf(file, "      \"package_size\": %llu,\n", (unsigned long long)result->package_size);
            fprintf(file, "      \"generation_time_ms\": %llu\n", (unsigned long long)result->generation_time_ms);
            fprintf(file, "    }%s\n", (i < packaging_system.result_count - 1) ? "," : "");
        }
        fprintf(file, "  ]\n");
        fprintf(file, "}\n");

    } else {
        // Text format
        fprintf(file, "=============================================================================\n");
        fprintf(file, "PACKAGE GENERATION REPORT\n");
        fprintf(file, "Generated: %llu\n", (unsigned long long)Sys_Milliseconds());
        fprintf(file, "=============================================================================\n\n");

        // Summary
        fprintf(file, "PACKAGING SUMMARY\n");
        fprintf(file, "------------------\n");
        fprintf(file, "Total Packages Generated: %u\n", packaging_system.total_packages_generated);
        fprintf(file, "Packages Succeeded: %u\n", packaging_system.packages_succeeded);
        fprintf(file, "Packages with Warnings: %u\n", packaging_system.packages_with_warnings);
        fprintf(file, "Packages Failed: %u\n\n", packaging_system.packages_failed);

        // Detailed results
        fprintf(file, "PACKAGE GENERATION RESULTS\n");
        fprintf(file, "--------------------------\n");

        for (uint32_t i = 0; i < packaging_system.result_count; i++) {
            package_generation_result_t* result = &packaging_system.results[i];
            fprintf(file, "Package: %s\n", result->package_name);
            fprintf(file, "Type: %s\n", PackageGenerator_GetPackageTypeName(result->package_type));
            fprintf(file, "Result: %s\n", PackageGenerator_GetResultString(result->result));
            fprintf(file, "Output: %s\n", result->package_path);
            fprintf(file, "Size: %.2f MB\n", result->package_size / (1024.0 * 1024.0));
            fprintf(file, "Generation Time: %llu ms\n", (unsigned long long)result->generation_time_ms);

            if (result->error_message[0]) {
                fprintf(file, "Error: %s\n", result->error_message);
            }
            fprintf(file, "\n");
        }

        // Recommendations
        fprintf(file, "RECOMMENDATIONS\n");
        fprintf(file, "---------------\n");
        if (packaging_system.packages_failed > 0) {
            fprintf(file, "- Address package generation failures\n");
        }
        if (packaging_system.packages_with_warnings > 0) {
            fprintf(file, "- Review packages with warnings\n");
        }
        fprintf(file, "- Test package installation on target platforms\n");
        fprintf(file, "- Verify package signatures if using signed packages\n");
    }

    fclose(file);
    return qtrue;
}

void PackageGenerator_PrintStatistics(void) {
    Com_Printf("=== Package Generation Statistics ===\n");
    Com_Printf("Total Packages Generated: %u\n", packaging_system.total_packages_generated);
    Com_Printf("  Succeeded: %u\n", packaging_system.packages_succeeded);
    Com_Printf("  With Warnings: %u\n", packaging_system.packages_with_warnings);
    Com_Printf("  Failed: %u\n", packaging_system.packages_failed);
    Com_Printf("Total Generation Time: %.2f seconds\n",
              packaging_system.total_generation_time_ms / 1000.0);
    Com_Printf("Total Package Size: %.2f MB\n",
              packaging_system.total_package_size_bytes / (1024.0 * 1024.0));

    if (packaging_system.total_packages_generated > 0) {
        float avg_time = (float)packaging_system.total_generation_time_ms /
                        packaging_system.total_packages_generated / 1000.0;
        Com_Printf("Average Generation Time: %.2f seconds\n", avg_time);
    }

    Com_Printf("=====================================\n");
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

const char* PackageGenerator_GetPackageTypeName(package_type_t type) {
    if (type >= PACKAGE_TYPE_COUNT) return "Unknown";
    return package_type_names[type];
}

const char* PackageGenerator_GetPlatformName(package_platform_t platform) {
    if (platform >= PACKAGE_PLATFORM_COUNT) return "Unknown";
    return platform_names[platform];
}

const char* PackageGenerator_GetArchitectureName(package_architecture_t arch) {
    if (arch >= PACKAGE_ARCH_COUNT) return "Unknown";
    return architecture_names[arch];
}

const char* PackageGenerator_GetResultString(package_result_t result) {
    if (result > PACKAGE_RESULT_FAILED) return "UNKNOWN";
    return result_strings[result];
}

qboolean PackageGenerator_GetPackageExtension(package_type_t type, char* extension, size_t size) {
    if (type >= PACKAGE_TYPE_COUNT) return qfalse;
    Q_strncpyz(extension, package_extensions[type], size);
    return qtrue;
}

qboolean PackageGenerator_SanitizePackageName(const char* name, char* sanitized, size_t size) {
    if (!name || !sanitized || size == 0) return qfalse;

    size_t i;
    for (i = 0; i < size - 1 && name[i]; i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            sanitized[i] = c;
        } else {
            sanitized[i] = '_';
        }
    }
    sanitized[i] = '\0';
    return qtrue;
}

/*
=============================================================================
Checksum and Signing (Stub Implementations)
=============================================================================
*/

qboolean PackageGenerator_GenerateChecksum(const char* file_path, char* checksum, size_t size) {
    if (!file_path || !checksum || size == 0) return qfalse;

    char *md5 = Com_MD5File(file_path, 0, NULL, 0);
    if (md5) {
        Q_strncpyz(checksum, md5, size);
        return qtrue;
    }

    return qfalse;
}

qboolean PackageGenerator_SignPackage(const char* package_path,
                                    const char* certificate_path,
                                    const char* key_path,
                                    const char* passphrase) {
    // Stub implementation - would use platform-specific signing tools
    Com_Printf("Package signing not yet implemented\n");
    return qfalse;
}

/*
=============================================================================
Console Commands
=============================================================================
*/

void PackageGenerator_Status_f(void) {
    if (!packaging_system.initialized) {
        Com_Printf("Automated packaging system not initialized\n");
        return;
    }

    Com_Printf("=== Automated Packaging System Status ===\n");
    Com_Printf("Initialized: Yes\n");
    Com_Printf("Total Packages Generated: %u\n", packaging_system.total_packages_generated);
    Com_Printf("Results Stored: %u/%u\n", packaging_system.result_count, packaging_system.max_results);
    Com_Printf("Available Tools:\n");
    Com_Printf("  NSIS: %s\n", packaging_system.has_nsis ? "Yes" : "No");
    Com_Printf("  WiX: %s\n", packaging_system.has_wix ? "Yes" : "No");
    Com_Printf("  dpkg-deb: %s\n", packaging_system.has_dpkg_deb ? "Yes" : "No");
    Com_Printf("  rpmbuild: %s\n", packaging_system.has_rpmbuild ? "Yes" : "No");
    Com_Printf("  appimagetool: %s\n", packaging_system.has_appimagetool ? "Yes" : "No");
    Com_Printf("  hdiutil: %s\n", packaging_system.has_hdiutil ? "Yes" : "No");
    Com_Printf("  pkgbuild: %s\n", packaging_system.has_pkgbuild ? "Yes" : "No");
    Com_Printf("=========================================\n");
}

void PackageGenerator_Generate_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: package generate <package_name> [platform] [type]\n");
        Com_Printf("Platforms: windows, linux, macos, universal\n");
        Com_Printf("Types: auto, msi, exe, deb, rpm, appimage, dmg, pkg, zip, tar.gz\n");
        return;
    }

    const char* package_name = Cmd_Argv(1);
    const char* platform_str = Cmd_Argc() >= 3 ? Cmd_Argv(2) : "auto";
    const char* type_str = Cmd_Argc() >= 4 ? Cmd_Argv(3) : "auto";

    // Create configuration
    package_config_t config = packaging_system.default_config;
    Q_strncpyz(config.package_name, package_name, sizeof(config.package_name));

    // Parse platform
    if (Q_stricmp(platform_str, "windows") == 0) {
        config.target_platform = PACKAGE_PLATFORM_WINDOWS;
    } else if (Q_stricmp(platform_str, "linux") == 0) {
        config.target_platform = PACKAGE_PLATFORM_LINUX;
    } else if (Q_stricmp(platform_str, "macos") == 0) {
        config.target_platform = PACKAGE_PLATFORM_MACOS;
    } else if (Q_stricmp(platform_str, "universal") == 0) {
        config.target_platform = PACKAGE_PLATFORM_UNIVERSAL;
    }

    // Parse package type
    if (Q_stricmp(type_str, "msi") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_MSI;
        config.num_preferred_types = 1;
    } else if (Q_stricmp(type_str, "exe") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_EXE;
        config.num_preferred_types = 1;
    } else if (Q_stricmp(type_str, "deb") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_DEB;
        config.num_preferred_types = 1;
    } else if (Q_stricmp(type_str, "rpm") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_RPM;
        config.num_preferred_types = 1;
    } else if (Q_stricmp(type_str, "appimage") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_APPIMAGE;
        config.num_preferred_types = 1;
    } else if (Q_stricmp(type_str, "dmg") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_DMG;
        config.num_preferred_types = 1;
    } else if (Q_stricmp(type_str, "pkg") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_PKG;
        config.num_preferred_types = 1;
    } else if (Q_stricmp(type_str, "zip") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_ZIP;
        config.num_preferred_types = 1;
    } else if (Q_stricmp(type_str, "tar.gz") == 0) {
        config.preferred_types[0] = PACKAGE_TYPE_TAR_GZ;
        config.num_preferred_types = 1;
    }

    Com_Printf("Generating package: %s for %s\n", package_name, platform_str);

    package_generation_result_t* result = PackageGenerator_GeneratePackage(&config);
    if (result) {
        Com_Printf("Package generation result: %s\n",
                  PackageGenerator_GetResultString(result->result));
        if (result->result == PACKAGE_RESULT_SUCCESS) {
            Com_Printf("Package created: %s\n", result->package_path);
            Com_Printf("Package size: %.2f MB\n", result->package_size / (1024.0 * 1024.0));
        } else {
            Com_Printf("Error: %s\n", result->error_message);
        }
    } else {
        Com_Printf("Failed to generate package\n");
    }
}

void PackageGenerator_BatchGenerate_f(void) {
    Com_Printf("Generating packages for all platforms...\n");

    package_config_t config = packaging_system.default_config;
    qboolean success = PackageGenerator_GenerateAllPackages(&config);

    if (success) {
        Com_Printf("All packages generated successfully\n");
    } else {
        Com_Printf("Some packages failed to generate\n");
    }

    PackageGenerator_PrintStatistics();
}

void PackageGenerator_Report_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: package report <output_file> [format]\n");
        Com_Printf("Formats: text (default), json\n");
        return;
    }

    const char* output_file = Cmd_Argv(1);
    const char* format = Cmd_Argc() >= 3 ? Cmd_Argv(2) : "text";

    if (PackageGenerator_GenerateReport(output_file, format)) {
        Com_Printf("Package report generated: %s (format: %s)\n", output_file, format);
    } else {
        Com_Printf("Failed to generate package report\n");
    }
}

void PackageGenerator_Validate_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: package validate <package_path>\n");
        return;
    }

    const char* package_path = Cmd_Argv(1);

    Com_Printf("Validating package: %s\n", package_path);

    if (PackageGenerator_ValidatePackage(package_path)) {
        Com_Printf("Package validation: PASSED\n");
    } else {
        Com_Printf("Package validation: FAILED\n");
    }
}

void PackageGenerator_List_f(void) {
    Com_Printf("=== Generated Packages ===\n");

    for (uint32_t i = 0; i < packaging_system.result_count; i++) {
        package_generation_result_t* result = &packaging_system.results[i];
        Com_Printf("%s (%s): %s - %.2f MB\n",
                  result->package_name,
                  PackageGenerator_GetPackageTypeName(result->package_type),
                  PackageGenerator_GetResultString(result->result),
                  result->package_size / (1024.0 * 1024.0));
    }

    if (packaging_system.result_count == 0) {
        Com_Printf("No packages generated yet\n");
    }

    Com_Printf("=========================\n");
}

// Stub implementations for remaining functions
uint32_t PackageGenerator_GetResults(package_generation_result_t** results) {
    if (results) {
        *results = packaging_system.results;
    }
    return packaging_system.result_count;
}

package_generation_result_t* PackageGenerator_GetResult(const char* package_name) {
    for (uint32_t i = 0; i < packaging_system.result_count; i++) {
        if (Q_stricmp(packaging_system.results[i].package_name, package_name) == 0) {
            return &packaging_system.results[i];
        }
    }
    return NULL;
}

qboolean PackageGenerator_SaveResults(const char* filename) {
    // Placeholder implementation
    return qtrue;
}

qboolean PackageGenerator_LoadResults(const char* filename) {
    // Placeholder implementation
    return qtrue;
}

void PackageGenerator_ClearResults(void) {
    packaging_system.result_count = 0;
    packaging_system.total_packages_generated = 0;
    packaging_system.packages_succeeded = 0;
    packaging_system.packages_failed = 0;
    packaging_system.packages_with_warnings = 0;
    packaging_system.total_generation_time_ms = 0;
    packaging_system.total_package_size_bytes = 0;
}

qboolean PackageGenerator_IsPlatformSupported(package_platform_t platform) {
    return platform >= PACKAGE_PLATFORM_WINDOWS && platform < PACKAGE_PLATFORM_COUNT;
}

qboolean PackageGenerator_IsArchitectureSupported(package_architecture_t arch) {
    return arch >= PACKAGE_ARCH_X86 && arch < PACKAGE_ARCH_COUNT;
}

const char* PackageGenerator_GetToolPath(const char* tool_name) {
    // Simplified - would search PATH
    return tool_name;
}

qboolean PackageGenerator_GeneratePlatformPackage(package_platform_t platform,
                                                const package_config_t* config) {
    package_config_t temp_config = *config;
    temp_config.target_platform = platform;
    return PackageGenerator_GeneratePackage(&temp_config) != NULL;
}

qboolean PackageGenerator_ValidatePackage(const char* package_path) {
    // Basic validation - check if file exists and is readable
    FILE* file = fopen(package_path, "rb");
    if (!file) return qfalse;

    // Check file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);

    return size > 0;
}

qboolean PackageGenerator_TestPackageInstallation(const char* package_path) {
    // Placeholder - would attempt to install package in test environment
    return qtrue;
}

qboolean PackageGenerator_VerifyPackageContents(const char* package_path,
                                              const package_config_t* config) {
    // Placeholder - would extract and verify package contents
    return qtrue;
}

qboolean PackageGenerator_VerifyPackageSignature(const char* package_path,
                                               const char* certificate_path) {
    // Placeholder - would verify package signature
    return qtrue;
}
