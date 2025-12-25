/*
=============================================================================
Update System

Incremental patch generation and distribution framework.
=============================================================================
*/

#ifndef __UPDATE_SYSTEM_H__
#define __UPDATE_SYSTEM_H__

#include "q_shared.h"

// Update types
typedef enum {
    UPDATE_TYPE_PATCH = 0,          // Incremental binary patch
    UPDATE_TYPE_FULL,               // Full update package
    UPDATE_TYPE_HOTFIX,             // Critical security/emergency fix
    UPDATE_TYPE_CONTENT,            // Content-only update (no binaries)
    UPDATE_TYPE_CONFIG,             // Configuration update
    UPDATE_TYPE_COUNT
} update_type_t;

// Update status
typedef enum {
    UPDATE_STATUS_AVAILABLE = 0,    // Update is available for download
    UPDATE_STATUS_DOWNLOADING,      // Update is being downloaded
    UPDATE_STATUS_DOWNLOADED,       // Update has been downloaded
    UPDATE_STATUS_APPLYING,         // Update is being applied
    UPDATE_STATUS_APPLIED,          // Update has been successfully applied
    UPDATE_STATUS_FAILED,           // Update failed to apply
    UPDATE_STATUS_ROLLBACK,         // Update is being rolled back
    UPDATE_STATUS_COUNT
} update_status_t;

// Update result
typedef enum {
    UPDATE_RESULT_SUCCESS = 0,      // Update completed successfully
    UPDATE_RESULT_FAILED,           // Update failed
    UPDATE_RESULT_CANCELLED,        // Update was cancelled by user
    UPDATE_RESULT_NETWORK_ERROR,    // Network error during download
    UPDATE_RESULT_DISK_ERROR,       // Disk error during application
    UPDATE_RESULT_VERIFICATION_FAILED, // Update verification failed
    UPDATE_RESULT_INCOMPATIBLE,     // Update is incompatible with current version
    UPDATE_RESULT_COUNT
} update_result_t;

// Update manifest entry
typedef struct {
    char filename[256];             // Original filename
    char patch_filename[256];       // Patch filename (if incremental)
    uint64_t original_size;         // Original file size
    uint64_t patch_size;            // Patch file size
    uint64_t new_size;              // New file size after patch
    char original_checksum[65];     // SHA256 of original file
    char patch_checksum[65];        // SHA256 of patch file
    char new_checksum[65];          // SHA256 of updated file
    qboolean is_binary;             // Whether this is a binary file
    qboolean is_critical;           // Whether this file is critical for operation
    qboolean requires_restart;      // Whether update requires application restart
} update_file_entry_t;

// Update manifest
typedef struct {
    char update_id[64];             // Unique update identifier
    char version_from[32];          // Source version
    char version_to[32];            // Target version
    update_type_t update_type;      // Type of update

    // Update metadata
    char description[512];          // Human-readable description
    char changelog_url[256];        // URL to full changelog
    uint64_t release_date;          // Release timestamp
    char author[128];               // Update author/maintainer

    // Size and compatibility
    uint64_t total_download_size;   // Total size of all patches
    uint64_t total_install_size;    // Total size after installation
    char min_os_version[32];        // Minimum OS version required
    char supported_platforms[128];  // Supported platforms (comma-separated)

    // File entries
    update_file_entry_t* file_entries;
    uint32_t file_count;
    uint32_t max_files;

    // Update verification
    char manifest_checksum[65];     // SHA256 of entire manifest
    char signing_key_id[128];       // GPG key ID used for signing
    char manifest_signature[512];   // GPG signature of manifest

    // Rollback information
    char rollback_version[32];      // Version to rollback to if needed
    qboolean supports_rollback;     // Whether this update supports rollback
} update_manifest_t;

// Update configuration
typedef struct {
    char update_server_url[256];   // Base URL for update server
    char update_channel[32];        // Update channel (stable, beta, dev)
    char current_version[32];       // Current installed version
    char install_directory[512];    // Game installation directory

    // Update preferences
    qboolean auto_check_updates;    // Automatically check for updates
    qboolean auto_download_updates; // Automatically download updates
    qboolean auto_apply_updates;    // Automatically apply updates
    int update_check_interval;      // Hours between update checks

    // Network settings
    int max_download_speed;         // Maximum download speed (KB/s, 0 = unlimited)
    int max_concurrent_downloads;   // Maximum concurrent downloads
    int download_timeout;           // Download timeout in seconds

    // Security settings
    qboolean verify_signatures;     // Verify GPG signatures
    char trusted_keys[512];         // Trusted GPG key fingerprints
    qboolean allow_untrusted_updates; // Allow updates from untrusted sources

    // Backup and rollback
    qboolean create_backups;        // Create backups before applying updates
    char backup_directory[512];     // Directory for update backups
    int max_backup_versions;        // Maximum backup versions to keep

    // Update restrictions
    qboolean allow_major_updates;   // Allow major version updates
    qboolean allow_prerelease_updates; // Allow pre-release updates
    qboolean require_restart_after_update; // Always restart after update

    // Logging and debugging
    qboolean enable_detailed_logging; // Enable verbose update logging
    char log_file[256];             // Update log file path
} update_config_t;

// Update download state
typedef struct {
    char update_id[64];             // Update being downloaded
    uint64_t total_size;            // Total download size
    uint64_t downloaded_size;       // Amount downloaded so far
    uint32_t file_count;            // Number of files to download
    uint32_t files_downloaded;      // Number of files downloaded
    float download_progress;        // Download progress (0.0-1.0)
    uint64_t download_speed;        // Current download speed (bytes/sec)
    uint64_t eta_seconds;           // Estimated time remaining
} update_download_state_t;

// Update application state
typedef struct {
    char update_id[64];             // Update being applied
    update_status_t status;         // Current application status
    uint32_t current_file;          // Current file being processed
    uint32_t total_files;           // Total files to process
    float application_progress;     // Application progress (0.0-1.0)
    char current_operation[128];    // Current operation description
    uint64_t start_time;            // When application started
    uint64_t eta_seconds;           // Estimated time remaining
} update_application_state_t;

// Update system
typedef struct {
    qboolean initialized;
    update_config_t config;

    // Current state
    update_status_t current_status;
    update_download_state_t download_state;
    update_application_state_t application_state;

    // Available updates
    update_manifest_t* available_updates;
    uint32_t available_update_count;
    uint32_t max_available_updates;

    // Update history
    update_manifest_t* applied_updates;
    uint32_t applied_update_count;
    uint32_t max_applied_updates;

    // Statistics
    uint32_t total_updates_downloaded;
    uint32_t total_updates_applied;
    uint32_t total_updates_failed;
    uint64_t total_bytes_downloaded;
    uint64_t total_update_time_ms;

    // Last update check
    uint64_t last_update_check;
    qboolean updates_available;

    // Background update thread
    qboolean background_update_enabled;
    qboolean update_thread_running;
} update_system_t;

extern update_system_t update_system;

// Update System API
qboolean UpdateSystem_Init(void);
void UpdateSystem_Shutdown(void);

// Configuration Management
void UpdateSystem_SetConfig(const update_config_t* config);
const update_config_t* UpdateSystem_GetConfig(void);
void UpdateSystem_LoadConfig(void);
void UpdateSystem_SaveConfig(void);

// Update Discovery
qboolean UpdateSystem_CheckForUpdates(void);
qboolean UpdateSystem_GetAvailableUpdates(update_manifest_t** updates, uint32_t* count);
qboolean UpdateSystem_IsUpdateAvailable(void);
update_manifest_t* UpdateSystem_GetLatestUpdate(void);

// Update Download
qboolean UpdateSystem_DownloadUpdate(const char* update_id);
qboolean UpdateSystem_CancelDownload(void);
qboolean UpdateSystem_GetDownloadProgress(update_download_state_t* state);
qboolean UpdateSystem_PauseDownload(void);
qboolean UpdateSystem_ResumeDownload(void);

// Update Application
qboolean UpdateSystem_ApplyUpdate(const char* update_id);
qboolean UpdateSystem_CancelUpdateApplication(void);
qboolean UpdateSystem_GetApplicationProgress(update_application_state_t* state);

// Rollback and Recovery
qboolean UpdateSystem_RollbackUpdate(const char* update_id);
qboolean UpdateSystem_CanRollbackUpdate(const char* update_id);
qboolean UpdateSystem_GetRollbackVersions(char** versions, uint32_t* count);

// Update Verification
qboolean UpdateSystem_VerifyUpdate(const char* update_path);
qboolean UpdateSystem_VerifyFileIntegrity(const char* file_path, const char* expected_checksum);
qboolean UpdateSystem_VerifyUpdateSignature(const update_manifest_t* manifest);

// Patch Generation (for developers/distributors)
qboolean UpdateSystem_GeneratePatch(const char* old_version_path,
                                   const char* new_version_path,
                                   const char* output_patch_path);
qboolean UpdateSystem_GenerateUpdateManifest(const char* version_from,
                                            const char* version_to,
                                            const char* output_manifest_path);
qboolean UpdateSystem_GenerateIncrementalUpdate(const char* base_directory,
                                               const char* update_files[],
                                               uint32_t file_count,
                                               const char* output_directory);

// Utility Functions
const char* UpdateSystem_GetStatusString(update_status_t status);
const char* UpdateSystem_GetResultString(update_result_t result);
const char* UpdateSystem_GetTypeString(update_type_t type);
qboolean UpdateSystem_ParseVersionString(const char* version_str, int* major, int* minor, int* patch);
int UpdateSystem_CompareVersions(const char* version1, const char* version2);
qboolean UpdateSystem_IsVersionCompatible(const char* current_version, const char* required_version);

// Update Manifest Management
update_manifest_t* UpdateSystem_LoadManifest(const char* manifest_path);
qboolean UpdateSystem_SaveManifest(const update_manifest_t* manifest, const char* output_path);
void UpdateSystem_FreeManifest(update_manifest_t* manifest);

// Statistics and Reporting
void UpdateSystem_PrintStatistics(void);
qboolean UpdateSystem_GenerateUpdateReport(const char* output_file, const char* format);
qboolean UpdateSystem_GetUpdateHistory(update_manifest_t** history, uint32_t* count);

// Network and Download Management
qboolean UpdateSystem_SetDownloadMirror(const char* mirror_url);
qboolean UpdateSystem_TestDownloadSpeed(const char* test_url, uint64_t* speed_bps);
qboolean UpdateSystem_GetDownloadStatistics(uint64_t* total_downloaded,
                                          uint64_t* average_speed,
                                          uint32_t* successful_downloads);

// Console Commands
void UpdateSystem_Status_f(void);
void UpdateSystem_Check_f(void);
void UpdateSystem_Download_f(void);
void UpdateSystem_Apply_f(void);
void UpdateSystem_Rollback_f(void);
void UpdateSystem_Config_f(void);
void UpdateSystem_Report_f(void);

#endif // __UPDATE_SYSTEM_H__
