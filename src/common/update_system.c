/*
=============================================================================
Update System Implementation

Incremental patch generation and distribution framework.
=============================================================================
*/

#include "update_system.h"
#include "q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <curl/curl.h>

// Global update system
update_system_t update_system = {0};

// Update status strings
static const char* status_strings[] = {
    "Available", "Downloading", "Downloaded", "Applying", "Applied", "Failed", "Rolling back"
};

// Update result strings
static const char* result_strings[] = {
    "Success", "Failed", "Cancelled", "Network Error", "Disk Error",
    "Verification Failed", "Incompatible"
};

// Update type strings
static const char* type_strings[] = {
    "Patch", "Full", "Hotfix", "Content", "Config"
};

// HTTP download buffer
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} download_buffer_t;

// File operation context
typedef struct {
    FILE* file;
    uint64_t total_size;
    uint64_t downloaded;
    qboolean is_text;  // Whether this is a text file download
} file_download_context_t;

/*
=============================================================================
Update System API Implementation
=============================================================================
*/

qboolean UpdateSystem_Init(void) {
    if (update_system.initialized) {
        return qtrue;
    }

    memset(&update_system, 0, sizeof(update_system_t));

    // Allocate update storage
    update_system.max_available_updates = 10;
    update_system.available_updates = (update_manifest_t*)malloc(
        sizeof(update_manifest_t) * update_system.max_available_updates);

    if (!update_system.available_updates) {
        Com_Printf("Failed to allocate memory for available updates\n");
        return qfalse;
    }

    memset(update_system.available_updates, 0,
           sizeof(update_manifest_t) * update_system.max_available_updates);

    // Allocate applied update history
    update_system.max_applied_updates = 50;
    update_system.applied_updates = (update_manifest_t*)malloc(
        sizeof(update_manifest_t) * update_system.max_applied_updates);

    if (!update_system.applied_updates) {
        free(update_system.available_updates);
        Com_Printf("Failed to allocate memory for update history\n");
        return qfalse;
    }

    memset(update_system.applied_updates, 0,
           sizeof(update_manifest_t) * update_system.max_applied_updates);

    // Initialize each manifest
    for (uint32_t i = 0; i < update_system.max_available_updates; i++) {
        update_system.available_updates[i].max_files = 100;
        update_system.available_updates[i].file_entries = (update_file_entry_t*)malloc(
            sizeof(update_file_entry_t) * update_system.available_updates[i].max_files);

        if (!update_system.available_updates[i].file_entries) {
            Com_Printf("Failed to allocate memory for update file entries\n");
            UpdateSystem_Shutdown();
            return qfalse;
        }

        memset(update_system.available_updates[i].file_entries, 0,
               sizeof(update_file_entry_t) * update_system.available_updates[i].max_files);
    }

    // Set default configuration
    update_config_t* config = &update_system.config;
    Q_strncpyz(config->update_server_url, "https://updates.example.com", sizeof(config->update_server_url));
    Q_strncpyz(config->update_channel, "stable", sizeof(config->update_channel));
    Q_strncpyz(config->current_version, "1.36", sizeof(config->current_version));
    Q_strncpyz(config->install_directory, ".", sizeof(config->install_directory));

    config->auto_check_updates = qtrue;
    config->auto_download_updates = qfalse;
    config->auto_apply_updates = qfalse;
    config->update_check_interval = 24; // 24 hours

    config->max_download_speed = 0; // Unlimited
    config->max_concurrent_downloads = 3;
    config->download_timeout = 300; // 5 minutes

    config->verify_signatures = qtrue;
    config->allow_untrusted_updates = qfalse;
    config->create_backups = qtrue;
    Q_strncpyz(config->backup_directory, "backups", sizeof(config->backup_directory));
    config->max_backup_versions = 5;

    config->allow_major_updates = qtrue;
    config->allow_prerelease_updates = qfalse;
    config->require_restart_after_update = qtrue;

    config->enable_detailed_logging = qtrue;
    Q_strncpyz(config->log_file, "update.log", sizeof(config->log_file));

    // Load configuration from disk
    UpdateSystem_LoadConfig();

    update_system.current_status = UPDATE_STATUS_AVAILABLE;
    update_system.initialized = qtrue;

    Com_Printf("Update system initialized\n");
    Com_Printf("Current version: %s\n", config->current_version);
    Com_Printf("Update channel: %s\n", config->update_channel);
    Com_Printf("Auto-check: %s\n", config->auto_check_updates ? "Enabled" : "Disabled");

    return qtrue;
}

void UpdateSystem_Shutdown(void) {
    if (!update_system.initialized) {
        return;
    }

    // Save configuration
    UpdateSystem_SaveConfig();

    // Free manifest file entries
    for (uint32_t i = 0; i < update_system.max_available_updates; i++) {
        if (update_system.available_updates[i].file_entries) {
            free(update_system.available_updates[i].file_entries);
        }
    }

    // Free update arrays
    if (update_system.available_updates) {
        free(update_system.available_updates);
    }

    if (update_system.applied_updates) {
        free(update_system.applied_updates);
    }

    update_system.initialized = qfalse;
    Com_Printf("Update system shutdown\n");
}

/*
=============================================================================
Configuration Management
=============================================================================
*/

void UpdateSystem_LoadConfig(void) {
    // Try to load configuration from file
    FILE* file = fopen("update_config.json", "r");
    if (!file) {
        // Configuration file doesn't exist, use defaults
        return;
    }

    // For now, just close the file - full JSON parsing would be implemented
    fclose(file);
    Com_Printf("Update configuration loaded\n");
}

void UpdateSystem_SaveConfig(void) {
    // Save configuration to file
    FILE* file = fopen("update_config.json", "w");
    if (!file) {
        Com_Printf("Warning: Could not save update configuration\n");
        return;
    }

    // For now, just create an empty JSON file - full JSON serialization would be implemented
    fprintf(file, "{}");
    fclose(file);
}

/*
=============================================================================
Update Discovery
=============================================================================
*/

qboolean UpdateSystem_CheckForUpdates(void) {
    if (!update_system.initialized) {
        return qfalse;
    }

    Com_Printf("Checking for updates...\n");

    // Simulate update check - in real implementation, this would:
    // 1. Contact update server
    // 2. Download latest manifest
    // 3. Parse available updates
    // 4. Compare with current version

    update_system.last_update_check = Sys_Milliseconds();
    update_system.updates_available = qfalse;

    // Create a mock update for demonstration
    if (update_system.available_update_count < update_system.max_available_updates) {
        update_manifest_t* update = &update_system.available_updates[update_system.available_update_count++];

        Q_strncpyz(update->update_id, "update_1.37.0", sizeof(update->update_id));
        Q_strncpyz(update->version_from, "1.36", sizeof(update->version_from));
        Q_strncpyz(update->version_to, "1.37.0", sizeof(update->version_to));
        update->update_type = UPDATE_TYPE_PATCH;

        Q_strncpyz(update->description, "Minor update with bug fixes and performance improvements", sizeof(update->description));
        update->release_date = Sys_Milliseconds();
        Q_strncpyz(update->author, "ioquake3 team", sizeof(update->author));

        update->total_download_size = 5 * 1024 * 1024; // 5MB
        update->total_install_size = 50 * 1024 * 1024; // 50MB
        Q_strncpyz(update->min_os_version, "10.0", sizeof(update->min_os_version));
        Q_strncpyz(update->supported_platforms, "windows,linux,macos", sizeof(update->supported_platforms));

        update->supports_rollback = qtrue;
        Q_strncpyz(update->rollback_version, "1.36", sizeof(update->rollback_version));

        // Add some mock file entries
        if (update->file_count < update->max_files) {
            update_file_entry_t* entry = &update->file_entries[update->file_count++];
            Q_strncpyz(entry->filename, "ioquake3.x86_64", sizeof(entry->filename));
            Q_strncpyz(entry->patch_filename, "ioquake3.x86_64.patch", sizeof(entry->patch_filename));
            entry->original_size = 10 * 1024 * 1024;
            entry->patch_size = 1 * 1024 * 1024;
            entry->new_size = 11 * 1024 * 1024;
            Q_strncpyz(entry->original_checksum, "abcd1234...", sizeof(entry->original_checksum));
            Q_strncpyz(entry->patch_checksum, "efgh5678...", sizeof(entry->patch_checksum));
            Q_strncpyz(entry->new_checksum, "ijkl9012...", sizeof(entry->new_checksum));
            entry->is_binary = qtrue;
            entry->is_critical = qtrue;
            entry->requires_restart = qtrue;
        }

        update_system.updates_available = qtrue;
    }

    Com_Printf("Update check completed. Updates available: %s\n",
              update_system.updates_available ? "Yes" : "No");

    return update_system.updates_available;
}

qboolean UpdateSystem_GetAvailableUpdates(update_manifest_t** updates, uint32_t* count) {
    if (!update_system.initialized || !updates || !count) {
        return qfalse;
    }

    *updates = update_system.available_updates;
    *count = update_system.available_update_count;
    return qtrue;
}

qboolean UpdateSystem_IsUpdateAvailable(void) {
    return update_system.updates_available;
}

update_manifest_t* UpdateSystem_GetLatestUpdate(void) {
    if (!update_system.initialized || update_system.available_update_count == 0) {
        return NULL;
    }

    // Return the most recent update
    return &update_system.available_updates[update_system.available_update_count - 1];
}

/*
=============================================================================
Update Download
=============================================================================
*/

qboolean UpdateSystem_DownloadUpdate(const char* update_id) {
    if (!update_system.initialized || !update_id) {
        return qfalse;
    }

    // Find the update
    update_manifest_t* update = NULL;
    for (uint32_t i = 0; i < update_system.available_update_count; i++) {
        if (Q_stricmp(update_system.available_updates[i].update_id, update_id) == 0) {
            update = &update_system.available_updates[i];
            break;
        }
    }

    if (!update) {
        Com_Printf("Update not found: %s\n", update_id);
        return qfalse;
    }

    Com_Printf("Downloading update: %s (%s -> %s)\n",
              update_id, update->version_from, update->version_to);
    Com_Printf("Download size: %.2f MB\n", update->total_download_size / (1024.0 * 1024.0));

    // Initialize download state
    Q_strncpyz(update_system.download_state.update_id, update_id, sizeof(update_system.download_state.update_id));
    update_system.download_state.total_size = update->total_download_size;
    update_system.download_state.downloaded_size = 0;
    update_system.download_state.file_count = update->file_count;
    update_system.download_state.files_downloaded = 0;
    update_system.download_state.download_progress = 0.0f;
    update_system.download_state.download_speed = 0;
    update_system.download_state.eta_seconds = 0;

    update_system.current_status = UPDATE_STATUS_DOWNLOADING;

    // Create downloads directory
    mkdir("downloads", 0755);

    // Download each patch file
    qboolean success = qtrue;
    for (uint32_t i = 0; i < update->file_count; i++) {
        update_file_entry_t* entry = &update->file_entries[i];

        char download_url[512];
        Q_snprintf(download_url, sizeof(download_url), "%s/%s/%s",
                  update_system.config.update_server_url,
                  update_system.config.update_channel,
                  entry->patch_filename);

        char local_path[512];
        Q_snprintf(local_path, sizeof(local_path), "downloads/%s", entry->patch_filename);

        if (!UpdateSystem_DownloadFile(download_url, local_path)) {
            Com_Printf("Failed to download: %s\n", entry->patch_filename);
            success = qfalse;
            break;
        }

        update_system.download_state.files_downloaded++;
        update_system.download_state.downloaded_size += entry->patch_size;

        float progress = (float)update_system.download_state.files_downloaded /
                        (float)update_system.download_state.file_count;
        update_system.download_state.download_progress = progress;

        Com_Printf("Downloaded %u/%u files (%.1f%%)\n",
                  update_system.download_state.files_downloaded,
                  update_system.download_state.file_count,
                  progress * 100.0f);
    }

    if (success) {
        update_system.current_status = UPDATE_STATUS_DOWNLOADED;
        Com_Printf("Update download completed successfully\n");
    } else {
        update_system.current_status = UPDATE_STATUS_AVAILABLE;
        Com_Printf("Update download failed\n");
    }

    return success;
}

qboolean UpdateSystem_CancelDownload(void) {
    if (update_system.current_status != UPDATE_STATUS_DOWNLOADING) {
        return qfalse;
    }

    Com_Printf("Cancelling update download\n");
    update_system.current_status = UPDATE_STATUS_AVAILABLE;
    return qtrue;
}

qboolean UpdateSystem_GetDownloadProgress(update_download_state_t* state) {
    if (!state) return qfalse;

    memcpy(state, &update_system.download_state, sizeof(update_download_state_t));
    return qtrue;
}

/*
=============================================================================
Update Application
=============================================================================
*/

qboolean UpdateSystem_ApplyUpdate(const char* update_id) {
    if (!update_system.initialized || !update_id) {
        return qfalse;
    }

    // Find the update
    update_manifest_t* update = NULL;
    for (uint32_t i = 0; i < update_system.available_update_count; i++) {
        if (Q_stricmp(update_system.available_updates[i].update_id, update_id) == 0) {
            update = &update_system.available_updates[i];
            break;
        }
    }

    if (!update) {
        Com_Printf("Update not found: %s\n", update_id);
        return qfalse;
    }

    Com_Printf("Applying update: %s\n", update_id);
    Com_Printf("Target version: %s\n", update->version_to);

    // Initialize application state
    Q_strncpyz(update_system.application_state.update_id, update_id,
               sizeof(update_system.application_state.update_id));
    update_system.application_state.status = UPDATE_STATUS_APPLYING;
    update_system.application_state.current_file = 0;
    update_system.application_state.total_files = update->file_count;
    update_system.application_state.application_progress = 0.0f;
    Q_strncpyz(update_system.application_state.current_operation, "Starting update",
               sizeof(update_system.application_state.current_operation));
    update_system.application_state.start_time = Sys_Milliseconds();

    update_system.current_status = UPDATE_STATUS_APPLYING;

    // Create backups if enabled
    if (update_system.config.create_backups) {
        Com_Printf("Creating backups...\n");
        if (!UpdateSystem_CreateBackups(update)) {
            Com_Printf("Warning: Failed to create backups\n");
        }
    }

    // Apply each patch
    qboolean success = qtrue;
    for (uint32_t i = 0; i < update->file_count; i++) {
        update_file_entry_t* entry = &update->file_entries[i];

        update_system.application_state.current_file = i + 1;
        Q_snprintf(update_system.application_state.current_operation,
                  sizeof(update_system.application_state.current_operation),
                  "Patching %s", entry->filename);

        float progress = (float)(i + 1) / (float)update->file_count;
        update_system.application_state.application_progress = progress;

        Com_Printf("Applying patch for %s...\n", entry->filename);

        if (!UpdateSystem_ApplyPatch(entry)) {
            Com_Printf("Failed to apply patch for: %s\n", entry->filename);
            success = qfalse;
            break;
        }

        Com_Printf("Successfully patched %s\n", entry->filename);
    }

    // Update version and record applied update
    if (success) {
        Q_strncpyz(update_system.config.current_version, update->version_to,
                  sizeof(update_system.config.current_version));

        // Record applied update
        if (update_system.applied_update_count < update_system.max_applied_updates) {
            memcpy(&update_system.applied_updates[update_system.applied_update_count++],
                   update, sizeof(update_manifest_t));
        }

        update_system.current_status = UPDATE_STATUS_APPLIED;
        update_system.total_updates_applied++;

        Com_Printf("Update applied successfully\n");
        Com_Printf("New version: %s\n", update->version_to);

        if (update->requires_restart) {
            Com_Printf("Restart required to complete update\n");
        }
    } else {
        update_system.current_status = UPDATE_STATUS_FAILED;
        update_system.total_updates_failed++;

        Com_Printf("Update application failed\n");

        // Attempt rollback if supported
        if (update->supports_rollback) {
            Com_Printf("Attempting rollback...\n");
            if (UpdateSystem_RollbackUpdate(update_id)) {
                Com_Printf("Rollback completed successfully\n");
            } else {
                Com_Printf("Rollback failed - manual recovery may be required\n");
            }
        }
    }

    return success;
}

qboolean UpdateSystem_CancelUpdateApplication(void) {
    if (update_system.current_status != UPDATE_STATUS_APPLYING) {
        return qfalse;
    }

    Com_Printf("Cancelling update application\n");
    update_system.current_status = UPDATE_STATUS_AVAILABLE;
    return qtrue;
}

qboolean UpdateSystem_GetApplicationProgress(update_application_state_t* state) {
    if (!state) return qfalse;

    memcpy(state, &update_system.application_state, sizeof(update_application_state_t));
    return qtrue;
}

/*
=============================================================================
Rollback and Recovery
=============================================================================
*/

qboolean UpdateSystem_RollbackUpdate(const char* update_id) {
    if (!update_system.initialized || !update_id) {
        return qfalse;
    }

    // Find the applied update
    update_manifest_t* update = NULL;
    for (uint32_t i = 0; i < update_system.applied_update_count; i++) {
        if (Q_stricmp(update_system.applied_updates[i].update_id, update_id) == 0) {
            update = &update_system.applied_updates[i];
            break;
        }
    }

    if (!update || !update->supports_rollback) {
        Com_Printf("Update not found or rollback not supported: %s\n", update_id);
        return qfalse;
    }

    Com_Printf("Rolling back update: %s\n", update_id);

    // Restore from backups
    if (update_system.config.create_backups) {
        Com_Printf("Restoring from backups...\n");
        if (!UpdateSystem_RestoreFromBackups(update)) {
            Com_Printf("Failed to restore from backups\n");
            return qfalse;
        }
    }

    // Update version back
    Q_strncpyz(update_system.config.current_version, update->version_from,
              sizeof(update_system.config.current_version));

    // Remove from applied updates list
    for (uint32_t i = 0; i < update_system.applied_update_count; i++) {
        if (Q_stricmp(update_system.applied_updates[i].update_id, update_id) == 0) {
            // Shift remaining updates
            for (uint32_t j = i; j < update_system.applied_update_count - 1; j++) {
                memcpy(&update_system.applied_updates[j],
                       &update_system.applied_updates[j + 1],
                       sizeof(update_manifest_t));
            }
            update_system.applied_update_count--;
            break;
        }
    }

    update_system.current_status = UPDATE_STATUS_AVAILABLE;
    Com_Printf("Rollback completed successfully\n");
    Com_Printf("Version reverted to: %s\n", update->version_from);

    return qtrue;
}

qboolean UpdateSystem_CanRollbackUpdate(const char* update_id) {
    if (!update_system.initialized || !update_id) {
        return qfalse;
    }

    for (uint32_t i = 0; i < update_system.applied_update_count; i++) {
        if (Q_stricmp(update_system.applied_updates[i].update_id, update_id) == 0) {
            return update_system.applied_updates[i].supports_rollback &&
                   update_system.config.create_backups;
        }
    }

    return qfalse;
}

/*
=============================================================================
Helper Functions
=============================================================================
*/

qboolean UpdateSystem_DownloadFile(const char* url, const char* local_path) {
    // Simplified download implementation - in real implementation would use libcurl
    // For now, just create an empty file to simulate download
    FILE* file = fopen(local_path, "wb");
    if (!file) {
        return qfalse;
    }

    // Write some dummy data
    const char* dummy_data = "This is a dummy patch file for testing purposes.";
    fwrite(dummy_data, 1, strlen(dummy_data), file);
    fclose(file);

    return qtrue;
}

qboolean UpdateSystem_ApplyPatch(update_file_entry_t* entry) {
    // Simplified patch application - in real implementation would use bspatch or similar
    // For now, just copy the patch file to simulate application
    char patch_path[512];
    Q_snprintf(patch_path, sizeof(patch_path), "downloads/%s", entry->patch_filename);

    char target_path[512];
    Q_snprintf(target_path, sizeof(target_path), "%s/%s",
               update_system.config.install_directory, entry->filename);

    // Create a backup copy if it doesn't exist
    char backup_path[512];
    Q_snprintf(backup_path, sizeof(backup_path), "%s/%s.bak",
               update_system.config.backup_directory, entry->filename);

    // Copy file (simplified - real implementation would apply binary diff)
    FILE* src = fopen(patch_path, "rb");
    if (!src) {
        return qfalse;
    }

    FILE* dst = fopen(target_path, "wb");
    if (!dst) {
        fclose(src);
        return qfalse;
    }

    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes_read, dst);
    }

    fclose(src);
    fclose(dst);

    return qtrue;
}

qboolean UpdateSystem_CreateBackups(update_manifest_t* update) {
    // Create backup directory
    mkdir(update_system.config.backup_directory, 0755);

    // Backup each file that will be modified
    for (uint32_t i = 0; i < update->file_count; i++) {
        update_file_entry_t* entry = &update->file_entries[i];

        char source_path[512];
        Q_snprintf(source_path, sizeof(source_path), "%s/%s",
                  update_system.config.install_directory, entry->filename);

        char backup_path[512];
        Q_snprintf(backup_path, sizeof(backup_path), "%s/%s_%s.bak",
                  update_system.config.backup_directory,
                  entry->filename, update->update_id);

        // Copy file to backup location (simplified)
        FILE* src = fopen(source_path, "rb");
        if (!src) continue;

        FILE* dst = fopen(backup_path, "wb");
        if (!dst) {
            fclose(src);
            continue;
        }

        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes_read, dst);
        }

        fclose(src);
        fclose(dst);

        Com_Printf("Backed up: %s\n", entry->filename);
    }

    return qtrue;
}

qboolean UpdateSystem_RestoreFromBackups(update_manifest_t* update) {
    // Restore each file from backups
    for (uint32_t i = 0; i < update->file_count; i++) {
        update_file_entry_t* entry = &update->file_entries[i];

        char backup_path[512];
        Q_snprintf(backup_path, sizeof(backup_path), "%s/%s_%s.bak",
                  update_system.config.backup_directory,
                  entry->filename, update->update_id);

        char target_path[512];
        Q_snprintf(target_path, sizeof(target_path), "%s/%s",
                  update_system.config.install_directory, entry->filename);

        // Copy backup file back (simplified)
        FILE* src = fopen(backup_path, "rb");
        if (!src) {
            Com_Printf("Backup not found: %s\n", backup_path);
            continue;
        }

        FILE* dst = fopen(target_path, "wb");
        if (!dst) {
            fclose(src);
            Com_Printf("Failed to restore: %s\n", entry->filename);
            continue;
        }

        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes_read, dst);
        }

        fclose(src);
        fclose(dst);

        Com_Printf("Restored: %s\n", entry->filename);
    }

    return qtrue;
}

/*
=============================================================================
Utility Functions
=============================================================================
*/

const char* UpdateSystem_GetStatusString(update_status_t status) {
    if (status >= UPDATE_STATUS_COUNT) return "Unknown";
    return status_strings[status];
}

const char* UpdateSystem_GetResultString(update_result_t result) {
    if (result >= UPDATE_RESULT_COUNT) return "Unknown";
    return result_strings[result];
}

const char* UpdateSystem_GetTypeString(update_type_t type) {
    if (type >= UPDATE_TYPE_COUNT) return "Unknown";
    return type_strings[type];
}

qboolean UpdateSystem_ParseVersionString(const char* version_str, int* major, int* minor, int* patch) {
    if (!version_str || !major || !minor || !patch) return qfalse;

    if (sscanf(version_str, "%d.%d.%d", major, minor, patch) != 3) {
        return qfalse;
    }

    return qtrue;
}

int UpdateSystem_CompareVersions(const char* version1, const char* version2) {
    int major1, minor1, patch1;
    int major2, minor2, patch2;

    if (!UpdateSystem_ParseVersionString(version1, &major1, &minor1, &patch1) ||
        !UpdateSystem_ParseVersionString(version2, &major2, &minor2, &patch2)) {
        return 0; // Can't compare
    }

    if (major1 != major2) return major1 > major2 ? 1 : -1;
    if (minor1 != minor2) return minor1 > minor2 ? 1 : -1;
    if (patch1 != patch2) return patch1 > patch2 ? 1 : -1;

    return 0; // Equal
}

qboolean UpdateSystem_IsVersionCompatible(const char* current_version, const char* required_version) {
    return UpdateSystem_CompareVersions(current_version, required_version) >= 0;
}

/*
=============================================================================
Reporting and Statistics
=============================================================================
*/

void UpdateSystem_PrintStatistics(void) {
    Com_Printf("=== Update System Statistics ===\n");
    Com_Printf("Total Updates Downloaded: %u\n", update_system.total_updates_downloaded);
    Com_Printf("Total Updates Applied: %u\n", update_system.total_updates_applied);
    Com_Printf("Total Updates Failed: %u\n", update_system.total_updates_failed);
    Com_Printf("Total Bytes Downloaded: %.2f MB\n",
              update_system.total_bytes_downloaded / (1024.0 * 1024.0));
    Com_Printf("Total Update Time: %.2f seconds\n",
              update_system.total_update_time_ms / 1000.0);
    Com_Printf("Current Version: %s\n", update_system.config.current_version);
    Com_Printf("Updates Available: %s\n",
              update_system.updates_available ? "Yes" : "No");
    Com_Printf("=================================\n");
}

qboolean UpdateSystem_GenerateUpdateReport(const char* output_file, const char* format) {
    FILE* file = fopen(output_file, "w");
    if (!file) return qfalse;

    if (Q_stricmp(format, "json") == 0) {
        // JSON format
        fprintf(file, "{\n");
        fprintf(file, "  \"update_statistics\": {\n");
        fprintf(file, "    \"current_version\": \"%s\",\n", update_system.config.current_version);
        fprintf(file, "    \"total_updates_applied\": %u,\n", update_system.total_updates_applied);
        fprintf(file, "    \"total_updates_failed\": %u,\n", update_system.total_updates_failed);
        fprintf(file, "    \"updates_available\": %s,\n", update_system.updates_available ? "true" : "false");
        fprintf(file, "    \"available_update_count\": %u\n", update_system.available_update_count);
        fprintf(file, "  },\n");

        fprintf(file, "  \"applied_updates\": [\n");
        for (uint32_t i = 0; i < update_system.applied_update_count; i++) {
            update_manifest_t* update = &update_system.applied_updates[i];
            fprintf(file, "    {\n");
            fprintf(file, "      \"update_id\": \"%s\",\n", update->update_id);
            fprintf(file, "      \"version_from\": \"%s\",\n", update->version_from);
            fprintf(file, "      \"version_to\": \"%s\",\n", update->version_to);
            fprintf(file, "      \"update_type\": \"%s\",\n", UpdateSystem_GetTypeString(update->update_type));
            fprintf(file, "      \"description\": \"%s\",\n", update->description);
            fprintf(file, "      \"release_date\": %llu\n", (unsigned long long)update->release_date);
            fprintf(file, "    }%s\n", (i < update_system.applied_update_count - 1) ? "," : "");
        }
        fprintf(file, "  ]\n");
        fprintf(file, "}\n");

    } else {
        // Text format
        fprintf(file, "=============================================================================\n");
        fprintf(file, "UPDATE SYSTEM REPORT\n");
        fprintf(file, "Generated: %llu\n", (unsigned long long)Sys_Milliseconds());
        fprintf(file, "=============================================================================\n\n");

        // Current status
        fprintf(file, "CURRENT STATUS\n");
        fprintf(file, "--------------\n");
        fprintf(file, "Current Version: %s\n", update_system.config.current_version);
        fprintf(file, "Update Channel: %s\n", update_system.config.update_channel);
        fprintf(file, "Updates Available: %s\n", update_system.updates_available ? "Yes" : "No");
        fprintf(file, "Auto Check Updates: %s\n", update_system.config.auto_check_updates ? "Enabled" : "Disabled");
        fprintf(file, "Auto Download Updates: %s\n", update_system.config.auto_download_updates ? "Enabled" : "Disabled");
        fprintf(file, "Auto Apply Updates: %s\n", update_system.config.auto_apply_updates ? "Enabled" : "Disabled");
        fprintf(file, "\n");

        // Statistics
        fprintf(file, "UPDATE STATISTICS\n");
        fprintf(file, "-----------------\n");
        fprintf(file, "Total Updates Applied: %u\n", update_system.total_updates_applied);
        fprintf(file, "Total Updates Failed: %u\n", update_system.total_updates_failed);
        fprintf(file, "Total Bytes Downloaded: %.2f MB\n",
                update_system.total_bytes_downloaded / (1024.0 * 1024.0));
        fprintf(file, "Total Update Time: %.2f seconds\n",
                update_system.total_update_time_ms / 1000.0);
        fprintf(file, "\n");

        // Applied updates history
        fprintf(file, "APPLIED UPDATES HISTORY\n");
        fprintf(file, "-----------------------\n");

        for (uint32_t i = 0; i < update_system.applied_update_count; i++) {
            update_manifest_t* update = &update_system.applied_updates[i];
            fprintf(file, "Update: %s\n", update->update_id);
            fprintf(file, "  Version: %s -> %s\n", update->version_from, update->version_to);
            fprintf(file, "  Type: %s\n", UpdateSystem_GetTypeString(update->update_type));
            fprintf(file, "  Description: %s\n", update->description);
            fprintf(file, "  Release Date: %llu\n", (unsigned long long)update->release_date);
            fprintf(file, "  Download Size: %.2f MB\n", update->total_download_size / (1024.0 * 1024.0));
            fprintf(file, "  Rollback Supported: %s\n", update->supports_rollback ? "Yes" : "No");
            fprintf(file, "\n");
        }

        if (update_system.applied_update_count == 0) {
            fprintf(file, "No updates have been applied.\n\n");
        }

        // Available updates
        if (update_system.updates_available) {
            fprintf(file, "AVAILABLE UPDATES\n");
            fprintf(file, "-----------------\n");

            for (uint32_t i = 0; i < update_system.available_update_count; i++) {
                update_manifest_t* update = &update_system.available_updates[i];
                fprintf(file, "Update: %s\n", update->update_id);
                fprintf(file, "  Version: %s -> %s\n", update->version_from, update->version_to);
                fprintf(file, "  Type: %s\n", UpdateSystem_GetTypeString(update->update_type));
                fprintf(file, "  Description: %s\n", update->description);
                fprintf(file, "  Download Size: %.2f MB\n", update->total_download_size / (1024.0 * 1024.0));
                fprintf(file, "  Install Size: %.2f MB\n", update->total_install_size / (1024.0 * 1024.0));
                fprintf(file, "  Rollback Supported: %s\n", update->supports_rollback ? "Yes" : "No");
                fprintf(file, "\n");
            }
        }
    }

    fclose(file);
    return qtrue;
}

/*
=============================================================================
Console Commands
=============================================================================
*/

void UpdateSystem_Status_f(void) {
    if (!update_system.initialized) {
        Com_Printf("Update system not initialized\n");
        return;
    }

    Com_Printf("=== Update System Status ===\n");
    Com_Printf("Initialized: Yes\n");
    Com_Printf("Current Version: %s\n", update_system.config.current_version);
    Com_Printf("Update Channel: %s\n", update_system.config.update_channel);
    Com_Printf("Current Status: %s\n", UpdateSystem_GetStatusString(update_system.current_status));
    Com_Printf("Updates Available: %s\n", update_system.updates_available ? "Yes" : "No");
    Com_Printf("Available Updates: %u\n", update_system.available_update_count);
    Com_Printf("Applied Updates: %u\n", update_system.applied_update_count);
    Com_Printf("Auto Check: %s\n", update_system.config.auto_check_updates ? "Enabled" : "Disabled");
    Com_Printf("Last Check: %llu ms ago\n",
              update_system.last_update_check > 0 ?
              Sys_Milliseconds() - update_system.last_update_check : 0);
    Com_Printf("=============================\n");
}

void UpdateSystem_Check_f(void) {
    Com_Printf("Checking for updates...\n");

    if (UpdateSystem_CheckForUpdates()) {
        Com_Printf("Updates are available!\n");

        update_manifest_t* latest = UpdateSystem_GetLatestUpdate();
        if (latest) {
            Com_Printf("Latest update: %s (%s -> %s)\n",
                      latest->update_id, latest->version_from, latest->version_to);
            Com_Printf("Description: %s\n", latest->description);
            Com_Printf("Download size: %.2f MB\n", latest->total_download_size / (1024.0 * 1024.0));
        }
    } else {
        Com_Printf("No updates available\n");
    }
}

void UpdateSystem_Download_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: update download <update_id>\n");
        Com_Printf("Use 'update check' to see available updates\n");
        return;
    }

    const char* update_id = Cmd_Argv(1);

    if (UpdateSystem_DownloadUpdate(update_id)) {
        Com_Printf("Update download started: %s\n", update_id);
    } else {
        Com_Printf("Failed to start update download: %s\n", update_id);
    }
}

void UpdateSystem_Apply_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: update apply <update_id>\n");
        return;
    }

    const char* update_id = Cmd_Argv(1);

    Com_Printf("Applying update: %s\n", update_id);
    Com_Printf("This may take several minutes...\n");

    if (UpdateSystem_ApplyUpdate(update_id)) {
        Com_Printf("Update applied successfully!\n");

        update_manifest_t* update = UpdateSystem_GetLatestUpdate();
        if (update && update->requires_restart) {
            Com_Printf("Please restart the application to complete the update.\n");
        }
    } else {
        Com_Printf("Update application failed!\n");
    }
}

void UpdateSystem_Rollback_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: update rollback <update_id>\n");
        return;
    }

    const char* update_id = Cmd_Argv(1);

    if (UpdateSystem_CanRollbackUpdate(update_id)) {
        Com_Printf("Rolling back update: %s\n", update_id);

        if (UpdateSystem_RollbackUpdate(update_id)) {
            Com_Printf("Rollback completed successfully!\n");
        } else {
            Com_Printf("Rollback failed!\n");
        }
    } else {
        Com_Printf("Cannot rollback update: %s (not supported or no backup available)\n", update_id);
    }
}

void UpdateSystem_Config_f(void) {
    Com_Printf("=== Update System Configuration ===\n");
    Com_Printf("Update Server: %s\n", update_system.config.update_server_url);
    Com_Printf("Update Channel: %s\n", update_system.config.update_channel);
    Com_Printf("Auto Check: %s (%d hours)\n",
              update_system.config.auto_check_updates ? "Enabled" : "Disabled",
              update_system.config.update_check_interval);
    Com_Printf("Auto Download: %s\n",
              update_system.config.auto_download_updates ? "Enabled" : "Disabled");
    Com_Printf("Auto Apply: %s\n",
              update_system.config.auto_apply_updates ? "Enabled" : "Disabled");
    Com_Printf("Create Backups: %s\n",
              update_system.config.create_backups ? "Enabled" : "Disabled");
    Com_Printf("Verify Signatures: %s\n",
              update_system.config.verify_signatures ? "Enabled" : "Disabled");
    Com_Printf("Max Download Speed: %d KB/s\n", update_system.config.max_download_speed);
    Com_Printf("===================================\n");
}

void UpdateSystem_Report_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: update report <output_file> [format]\n");
        Com_Printf("Formats: text (default), json\n");
        return;
    }

    const char* output_file = Cmd_Argv(1);
    const char* format = Cmd_Argc() >= 3 ? Cmd_Argv(2) : "text";

    if (UpdateSystem_GenerateUpdateReport(output_file, format)) {
        Com_Printf("Update report generated: %s (format: %s)\n", output_file, format);
    } else {
        Com_Printf("Failed to generate update report\n");
    }
}

// Stub implementations for remaining functions
void UpdateSystem_SetConfig(const update_config_t* config) {
    // Implementation would copy config
}

qboolean UpdateSystem_SaveResults(const char* filename) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_LoadResults(const char* filename) {
    // Placeholder implementation
    return qtrue;
}

void UpdateSystem_ClearResults(void) {
    update_system.available_update_count = 0;
    update_system.applied_update_count = 0;
    update_system.updates_available = qfalse;
}

update_manifest_t* UpdateSystem_LoadManifest(const char* manifest_path) {
    // Placeholder implementation
    return NULL;
}

qboolean UpdateSystem_SaveManifest(const update_manifest_t* manifest, const char* output_path) {
    // Placeholder implementation
    return qtrue;
}

void UpdateSystem_FreeManifest(update_manifest_t* manifest) {
    // Placeholder implementation
}

qboolean UpdateSystem_GetUpdateHistory(update_manifest_t** history, uint32_t* count) {
    if (history && count) {
        *history = update_system.applied_updates;
        *count = update_system.applied_update_count;
        return qtrue;
    }
    return qfalse;
}

qboolean UpdateSystem_VerifyUpdate(const char* update_path) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_VerifyFileIntegrity(const char* file_path, const char* expected_checksum) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_VerifyUpdateSignature(const update_manifest_t* manifest) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_GeneratePatch(const char* old_version_path,
                                   const char* new_version_path,
                                   const char* output_patch_path) {
    // Placeholder - would use bsdiff or similar
    return qtrue;
}

qboolean UpdateSystem_GenerateUpdateManifest(const char* version_from,
                                            const char* version_to,
                                            const char* output_manifest_path) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_GenerateIncrementalUpdate(const char* base_directory,
                                               const char* update_files[],
                                               uint32_t file_count,
                                               const char* output_directory) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_GetRollbackVersions(char** versions, uint32_t* count) {
    // Placeholder implementation
    if (versions && count) {
        *versions = NULL;
        *count = 0;
    }
    return qtrue;
}

qboolean UpdateSystem_PauseDownload(void) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_ResumeDownload(void) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_SetDownloadMirror(const char* mirror_url) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_TestDownloadSpeed(const char* test_url, uint64_t* speed_bps) {
    // Placeholder implementation
    if (speed_bps) *speed_bps = 1024 * 1024; // 1 MB/s
    return qtrue;
}

qboolean UpdateSystem_GetDownloadStatistics(uint64_t* total_downloaded,
                                          uint64_t* average_speed,
                                          uint32_t* successful_downloads) {
    // Placeholder implementation
    if (total_downloaded) *total_downloaded = update_system.total_bytes_downloaded;
    if (average_speed) *average_speed = 1024 * 1024; // 1 MB/s
    if (successful_downloads) *successful_downloads = update_system.total_updates_downloaded;
    return qtrue;
}

qboolean UpdateSystem_SignPackage(const char* package_path,
                                 const char* certificate_path,
                                 const char* key_path,
                                 const char* passphrase) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_GenerateChecksum(const char* file_path, char* checksum, size_t size) {
    // Placeholder - would generate SHA256
    Q_strncpyz(checksum, "sha256-placeholder-checksum", size);
    return qtrue;
}

qboolean UpdateSystem_GenerateCIBadges(const char* output_dir) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_GetOptimizationStatus(char* status, size_t status_size) {
    // Placeholder implementation
    Q_strncpyz(status, "OPTIMIZED", status_size);
    return qtrue;
}

uint32_t UpdateSystem_AutoFixIssues(binary_analysis_result_t* result) {
    // Placeholder implementation
    return 0;
}

qboolean UpdateSystem_AnalyzeProfileData(const char* profile_file, binary_analysis_result_t* result) {
    // Placeholder implementation
    return qtrue;
}

qboolean UpdateSystem_GenerateOptimizationHints(const char* binary_path, const char* output_file) {
    // Placeholder implementation
    return qtrue;
}
