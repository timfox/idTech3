/*
=============================================================================
Automated Packaging System

Multi-platform installer and package generation framework.
=============================================================================
*/

#ifndef __PACKAGE_GENERATOR_H__
#define __PACKAGE_GENERATOR_H__

#include "q_shared.h"

// Package types
typedef enum {
    PACKAGE_TYPE_MSI,           // Windows MSI installer
    PACKAGE_TYPE_EXE,           // Windows executable installer
    PACKAGE_TYPE_DEB,           // Debian/Ubuntu DEB package
    PACKAGE_TYPE_RPM,           // Red Hat/CentOS RPM package
    PACKAGE_TYPE_APPIMAGE,      // Linux AppImage portable package
    PACKAGE_TYPE_DMG,           // macOS DMG disk image
    PACKAGE_TYPE_PKG,           // macOS PKG installer
    PACKAGE_TYPE_ZIP,           // Generic ZIP archive
    PACKAGE_TYPE_TAR_GZ,        // Generic tar.gz archive
    PACKAGE_TYPE_COUNT
} package_type_t;

// Package platforms
typedef enum {
    PACKAGE_PLATFORM_WINDOWS,
    PACKAGE_PLATFORM_LINUX,
    PACKAGE_PLATFORM_MACOS,
    PACKAGE_PLATFORM_UNIVERSAL,
    PACKAGE_PLATFORM_COUNT
} package_platform_t;

// Package architectures
typedef enum {
    PACKAGE_ARCH_X86,
    PACKAGE_ARCH_X86_64,
    PACKAGE_ARCH_ARM,
    PACKAGE_ARCH_ARM64,
    PACKAGE_ARCH_UNIVERSAL,
    PACKAGE_ARCH_COUNT
} package_architecture_t;

// Package compression types
typedef enum {
    COMPRESSION_NONE,
    COMPRESSION_ZIP,
    COMPRESSION_GZIP,
    COMPRESSION_BZIP2,
    COMPRESSION_XZ,
    COMPRESSION_LZMA,
    COMPRESSION_COUNT
} compression_type_t;

// Package result
typedef enum {
    PACKAGE_RESULT_SUCCESS = 0,
    PACKAGE_RESULT_WARNING,
    PACKAGE_RESULT_ERROR,
    PACKAGE_RESULT_FAILED
} package_result_t;

// Package configuration
typedef struct {
    char package_name[128];         // Package name (e.g., "ioquake3")
    char package_version[32];       // Version string (e.g., "1.36")
    char package_description[256];  // Package description
    char package_vendor[128];       // Vendor/company name
    char package_maintainer[128];   // Package maintainer
    char package_url[256];          // Project website URL
    char package_license[64];       // License type (e.g., "GPL-2.0")

    // Platform and architecture
    package_platform_t target_platform;
    package_architecture_t target_architecture;

    // Package type preferences (by priority)
    package_type_t preferred_types[4];
    int num_preferred_types;

    // Compression settings
    compression_type_t compression_type;
    int compression_level;          // 0-9 compression level

    // File locations
    char source_directory[512];     // Directory containing files to package
    char output_directory[512];     // Directory to write packages to
    char build_directory[512];      // Temporary build directory

    // Package metadata
    char changelog_file[256];       // Changelog file path
    char readme_file[256];          // README file path
    char license_file[256];         // License file path

    // Signing options
    qboolean enable_signing;        // Enable code/package signing
    char signing_certificate[256];  // Certificate file path
    char signing_key[256];          // Private key file path
    char signing_passphrase[256];   // Key passphrase

    // Advanced options
    qboolean create_portable;       // Create portable version
    qboolean include_debug_symbols; // Include debug symbols
    qboolean strip_binaries;        // Strip binaries for size
    qboolean compress_resources;    // Compress resource files
    qboolean create_checksum;       // Generate checksum files

    // Size limits
    uint64_t max_package_size;      // Maximum package size in bytes
    uint32_t max_file_count;        // Maximum number of files

    // Dependencies
    char* runtime_dependencies;     // Runtime dependency list
    uint32_t dependency_count;
    char* build_dependencies;       // Build dependency list
    uint32_t build_dependency_count;
} package_config_t;

// Package file entry
typedef struct {
    char source_path[512];          // Path in source directory
    char install_path[512];         // Installation path
    qboolean executable;            // Is this an executable file
    qboolean compressed;            // Should this file be compressed
    uint64_t file_size;             // File size in bytes
    char permissions[16];           // Unix-style permissions (e.g., "755")
} package_file_t;

// Package generation result
typedef struct {
    char package_name[128];         // Generated package name
    char package_path[512];         // Full path to generated package
    package_type_t package_type;    // Type of package generated
    uint64_t package_size;          // Size of generated package
    uint32_t file_count;            // Number of files in package
    char checksum[128];             // Package checksum (SHA256)
    package_result_t result;        // Generation result

    // Timing information
    uint64_t generation_time_ms;    // Time spent generating package

    // Error information
    char error_message[512];        // Error message if generation failed
    uint32_t warning_count;         // Number of warnings generated
    uint32_t error_count;           // Number of errors encountered
} package_generation_result_t;

// Packaging system
typedef struct {
    qboolean initialized;
    package_config_t default_config;

    // Package generation results
    package_generation_result_t* results;
    uint32_t result_count;
    uint32_t max_results;

    // Statistics
    uint32_t total_packages_generated;
    uint32_t packages_succeeded;
    uint32_t packages_failed;
    uint32_t packages_with_warnings;
    uint64_t total_generation_time_ms;
    uint64_t total_package_size_bytes;

    // Platform-specific tools
    qboolean has_nsis;              // Windows NSIS installer
    qboolean has_wix;               // Windows WiX toolset
    qboolean has_dpkg_deb;          // Debian packaging tools
    qboolean has_rpmbuild;          // RPM packaging tools
    qboolean has_appimagetool;      // AppImage tools
    qboolean has_hdiutil;           // macOS disk image tools
    qboolean has_pkgbuild;          // macOS package tools
} packaging_system_t;

extern packaging_system_t packaging_system;

// Package Generation API
qboolean PackageGenerator_Init(void);
void PackageGenerator_Shutdown(void);

// Configuration Management
void PackageGenerator_SetDefaultConfig(const package_config_t* config);
const package_config_t* PackageGenerator_GetDefaultConfig(void);
package_config_t* PackageGenerator_CreateConfig(void);
void PackageGenerator_DestroyConfig(package_config_t* config);

// Package Generation
package_generation_result_t* PackageGenerator_GeneratePackage(const package_config_t* config);
qboolean PackageGenerator_GenerateAllPackages(const package_config_t* config);
qboolean PackageGenerator_GeneratePlatformPackage(package_platform_t platform,
                                                const package_config_t* config);

// File Management
qboolean PackageGenerator_AddFile(package_config_t* config, const char* source_path,
                                const char* install_path, qboolean executable);
qboolean PackageGenerator_AddDirectory(package_config_t* config, const char* source_dir,
                                     const char* install_prefix);
qboolean PackageGenerator_ExcludePattern(package_config_t* config, const char* pattern);

// Platform Detection
package_platform_t PackageGenerator_DetectCurrentPlatform(void);
package_architecture_t PackageGenerator_DetectCurrentArchitecture(void);
qboolean PackageGenerator_IsPlatformSupported(package_platform_t platform);
qboolean PackageGenerator_IsArchitectureSupported(package_architecture_t arch);

// Tool Detection
qboolean PackageGenerator_DetectAvailableTools(void);
qboolean PackageGenerator_CheckToolAvailability(const char* tool_name);
const char* PackageGenerator_GetToolPath(const char* tool_name);

// Package Validation
qboolean PackageGenerator_ValidatePackage(const char* package_path);
qboolean PackageGenerator_TestPackageInstallation(const char* package_path);
qboolean PackageGenerator_VerifyPackageContents(const char* package_path,
                                              const package_config_t* config);

// Signing and Security
qboolean PackageGenerator_SignPackage(const char* package_path,
                                    const char* certificate_path,
                                    const char* key_path,
                                    const char* passphrase);
qboolean PackageGenerator_VerifyPackageSignature(const char* package_path,
                                               const char* certificate_path);

// Checksum Generation
qboolean PackageGenerator_GenerateChecksum(const char* file_path, char* checksum, size_t size);
qboolean PackageGenerator_VerifyChecksum(const char* file_path, const char* expected_checksum);

// Result Management
uint32_t PackageGenerator_GetResults(package_generation_result_t** results);
package_generation_result_t* PackageGenerator_GetResult(const char* package_name);
qboolean PackageGenerator_SaveResults(const char* filename);
qboolean PackageGenerator_LoadResults(const char* filename);
void PackageGenerator_ClearResults(void);

// Reporting and Statistics
qboolean PackageGenerator_GenerateReport(const char* output_file, const char* format);
void PackageGenerator_PrintStatistics(void);
void PackageGenerator_PrintPackageList(void);

// Utility Functions
const char* PackageGenerator_GetPackageTypeName(package_type_t type);
const char* PackageGenerator_GetPlatformName(package_platform_t platform);
const char* PackageGenerator_GetArchitectureName(package_architecture_t arch);
const char* PackageGenerator_GetResultString(package_result_t result);
qboolean PackageGenerator_GetPackageExtension(package_type_t type, char* extension, size_t size);
qboolean PackageGenerator_SanitizePackageName(const char* name, char* sanitized, size_t size);

// Platform-specific helpers
qboolean PackageGenerator_GenerateWindowsInstaller(const package_config_t* config,
                                                 package_generation_result_t* result);
qboolean PackageGenerator_GenerateLinuxPackage(const package_config_t* config,
                                             package_generation_result_t* result);
qboolean PackageGenerator_GenerateMacOSPackage(const package_config_t* config,
                                             package_generation_result_t* result);
qboolean PackageGenerator_GeneratePortablePackage(const package_config_t* config,
                                                package_generation_result_t* result);

// Console Commands
void PackageGenerator_Status_f(void);
void PackageGenerator_Generate_f(void);
void PackageGenerator_BatchGenerate_f(void);
void PackageGenerator_Validate_f(void);
void PackageGenerator_Report_f(void);
void PackageGenerator_List_f(void);

#endif // __PACKAGE_GENERATOR_H__
