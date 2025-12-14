# Backwards Compatibility Strategy

## Overview

This document outlines the comprehensive strategy for maintaining backwards compatibility across all engine subsystems, ensuring that mods, save games, network protocols, and assets continue to work across engine versions.

## Table of Contents

1. [Core Principles](#core-principles)
2. [Protocol Versioning](#protocol-versioning)
3. [Save Game Compatibility](#save-game-compatibility)
4. [QVM Compatibility](#qvm-compatibility)
5. [Asset Format Compatibility](#asset-format-compatibility)
6. [API Versioning](#api-versioning)
7. [Feature Detection](#feature-detection)
8. [Deprecation Policy](#deprecation-policy)
9. [Testing Strategy](#testing-strategy)
10. [Migration Tools](#migration-tools)

## Core Principles

### 1. Never Break Existing Content
- Legacy mods must continue to work
- Old save games must be loadable
- Network protocols must support older clients
- Asset formats must remain readable

### 2. Graceful Degradation
- New features are optional
- Missing features fail gracefully
- Provide fallbacks for unsupported features
- Clear error messages for incompatibilities

### 3. Version Everything
- Network protocols are versioned
- Save games include version numbers
- Asset formats have version headers
- APIs have version numbers

### 4. Feature Detection Over Assumptions
- Query capabilities before use
- Don't assume features exist
- Provide detection mechanisms
- Document feature availability

## Protocol Versioning

### Current Protocol Versions

```c
#define PROTOCOL_VERSION_66    66  // Legacy Quake 3
#define PROTOCOL_VERSION_67   67  // Quake 3 1.31
#define OLD_PROTOCOL_VERSION  68  // Q3E older version
#define NEW_PROTOCOL_VERSION  71  // Q3E current version
#define DEFAULT_PROTOCOL_VERSION OLD_PROTOCOL_VERSION
```

### Protocol Negotiation

**Current State**: Basic protocol version checking exists, but no formal negotiation.

**Recommended Implementation:**

```c
typedef struct {
    int protocolVersion;
    uint32_t featureFlags;      // Bitmask of supported features
    qboolean strictMode;         // Strict compatibility mode
    qboolean allowExtensions;    // Allow protocol extensions
} protocolCapabilities_t;

// Negotiate protocol during connection
qboolean NET_NegotiateProtocol(netadr_t *from, protocolCapabilities_t *capabilities);

// Check if feature is supported
qboolean NET_HasFeature(int protocolVersion, uint32_t featureFlag);
```

### Feature Flags

Define feature flags for protocol extensions:

```c
#define PROTO_FEATURE_COMPRESSED_SNAPSHOTS    (1 << 0)
#define PROTO_FEATURE_DELTA_COMPRESSION      (1 << 1)
#define PROTO_FEATURE_EXTENDED_ENTITIES      (1 << 2)
#define PROTO_FEATURE_LARGE_MAPS              (1 << 3)
#define PROTO_FEATURE_ENHANCED_PLAYERSTATE    (1 << 4)
```

### Compatibility Modes

**Strict Mode**: Only use features available in both client and server protocol versions.

**Loose Mode**: Use best available features, fallback gracefully.

**Implementation:**
```c
// Server side
if (client->protocolVersion < NEW_PROTOCOL_VERSION) {
    // Use legacy snapshot format
    SV_BuildLegacySnapshot(client);
} else {
    // Use enhanced snapshot format
    SV_BuildEnhancedSnapshot(client);
}
```

### Protocol Migration

**Strategy:**
1. Support multiple protocol versions simultaneously
2. Negotiate highest common version
3. Use feature flags for optional features
4. Maintain legacy code paths
5. Document deprecation timeline

## Save Game Compatibility

### Current State

Some save systems include basic versioning (e.g., inventory system), but no unified framework.

### Recommended Save Format

```c
// Save file header
typedef struct {
    char magic[4];           // "Q3SV" or similar
    uint32_t version;        // Save format version
    uint32_t engineVersion;  // Engine version that created save
    uint32_t checksum;       // Data integrity check
    uint32_t dataSize;       // Size of save data
    time_t timestamp;        // Save timestamp
} saveFileHeader_t;
```

### Versioned Serialization

```c
// Serialization context
typedef struct {
    uint32_t version;        // Format version being read/written
    qboolean strict;         // Strict mode (fail on unknown fields)
    void *userData;          // User context
} serializeContext_t;

// Versioned save function
qboolean Save_WriteVersioned(serializeContext_t *ctx, const void *data, size_t size);
qboolean Save_ReadVersioned(serializeContext_t *ctx, void *data, size_t size);
```

### Migration Framework

**Automatic Migration:**
```c
// Migration function signature
typedef qboolean (*SaveMigrateFunc_t)(serializeContext_t *from, 
                                      serializeContext_t *to,
                                      void *data);

// Register migration function
void Save_RegisterMigration(uint32_t fromVersion, uint32_t toVersion, 
                           SaveMigrateFunc_t migrateFunc);

// Migrate save file
qboolean Save_Migrate(const char *filename, uint32_t targetVersion);
```

**Example Migration:**
```c
// Migrate from version 1 to version 2
qboolean MigrateV1ToV2(serializeContext_t *from, serializeContext_t *to, void *data) {
    oldSaveFormat_t *old = (oldSaveFormat_t *)data;
    newSaveFormat_t *new = (newSaveFormat_t *)Z_TagMalloc(sizeof(newSaveFormat_t), TAG_TEMP);
    
    // Copy compatible fields
    new->playerHealth = old->playerHealth;
    new->playerArmor = old->playerArmor;
    
    // Set defaults for new fields
    new->playerStamina = 100;  // New field, use default
    
    // Write migrated data
    return Save_WriteVersioned(to, new, sizeof(newSaveFormat_t));
}
```

### Save Corruption Recovery

**Atomic Writes:**
```c
// Write to temporary file first
char tempPath[MAX_QPATH];
Com_sprintf(tempPath, sizeof(tempPath), "%s.tmp", savePath);

// Write save data
if (Save_WriteFile(tempPath, data)) {
    // Atomic rename
    FS_Rename(tempPath, savePath);
    return qtrue;
} else {
    FS_Remove(tempPath);
    return qfalse;
}
```

**Backup System:**
```c
// Create backup before writing
void Save_CreateBackup(const char *savePath) {
    char backupPath[MAX_QPATH];
    Com_sprintf(backupPath, sizeof(backupPath), "%s.bak", savePath);
    FS_CopyFile(savePath, backupPath);
}

// Restore from backup on corruption
qboolean Save_RestoreBackup(const char *savePath) {
    char backupPath[MAX_QPATH];
    Com_sprintf(backupPath, sizeof(backupPath), "%s.bak", savePath);
    
    if (FS_FileExists(backupPath)) {
        return FS_CopyFile(backupPath, savePath);
    }
    return qfalse;
}
```

**Checksum Validation:**
```c
uint32_t Save_CalculateChecksum(const void *data, size_t size) {
    uint32_t checksum = 0;
    const uint32_t *words = (const uint32_t *)data;
    size_t wordCount = size / sizeof(uint32_t);
    
    for (size_t i = 0; i < wordCount; i++) {
        checksum ^= words[i];
        checksum = (checksum << 1) | (checksum >> 31);
    }
    return checksum;
}
```

## QVM Compatibility

See [QVM Compatibility Documentation](QVM_COMPATIBILITY.md) for detailed information.

### Key Points

1. **Always Maintain QVM Support**: QVM is the fallback for all mods
2. **Extension Syscalls**: New features via extensions, not standard syscalls
3. **Feature Detection**: QVMs query for extension availability
4. **Graceful Degradation**: Missing extensions fail gracefully

### Extension System

```c
// Query for extension availability
qboolean VM_Ext_HasFeature(vm_t *vm, const char *featureName);

// Get extension version
int VM_Ext_GetVersion(vm_t *vm, const char *featureName);

// Example usage in QVM
if (VM_Ext_HasFeature(vm, "CVar_SetDescription")) {
    trap_Cvar_SetDescription_Q3E(name, description);
} else {
    // Fallback: just set the CVar
    trap_Cvar_Set(name, value);
}
```

## Asset Format Compatibility

### Asset Versioning

**Manifest Format:**
```json
{
  "manifestVersion": 1,
  "bundleVersion": "1.0.0",
  "gameName": "mymod",
  "assets": [
    {
      "path": "textures/test.tga",
      "hash": "abc123...",
      "formatVersion": 1,
      "dependencies": []
    }
  ]
}
```

### Format Detection

```c
// Detect asset format version
int Asset_DetectFormatVersion(const char *filename);

// Read with version handling
qboolean Asset_LoadVersioned(const char *filename, int expectedVersion, void *data);
```

### Format Migration

**Texture Formats:**
- Support legacy formats (TGA, JPG, PNG)
- Auto-convert to modern formats when possible
- Preserve originals for compatibility

**Model Formats:**
- Support MD3, ASE, OBJ
- Convert to optimized formats
- Maintain source compatibility

**Shader Formats:**
- Support legacy shader syntax
- Auto-upgrade where possible
- Document breaking changes

## API Versioning

### Engine API Version

```c
// Engine API version
#define ENGINE_API_VERSION_MAJOR 1
#define ENGINE_API_VERSION_MINOR 0
#define ENGINE_API_VERSION_PATCH 0

// Get engine API version
void Engine_GetAPIVersion(int *major, int *minor, int *patch);
```

### Module API Versioning

**Syscall Registry:**
```c
// Register syscall with version
void Syscall_Register(const char *name, int number, int minVersion, int maxVersion);

// Check syscall availability
qboolean Syscall_IsAvailable(const char *name, int apiVersion);
```

### Plugin API Versioning

```c
// Plugin API version check
typedef struct {
    int apiVersion;
    const char *engineVersion;
    uint32_t featureFlags;
} pluginAPIInfo_t;

qboolean Plugin_CheckAPIVersion(pluginAPIInfo_t *info);
```

## Feature Detection

### Runtime Feature Detection

**Engine Features:**
```c
typedef enum {
    FEATURE_VULKAN_RENDERER,
    FEATURE_TRACY_PROFILER,
    FEATURE_MEMORY_TRACKING,
    FEATURE_HOT_RELOAD,
    FEATURE_EVENT_SYSTEM,
    FEATURE_STRUCTURED_LOGGING,
    // ...
} engineFeature_t;

qboolean Engine_HasFeature(engineFeature_t feature);
const char *Engine_GetFeatureVersion(engineFeature_t feature);
```

**Renderer Features:**
```c
qboolean R_HasExtension(const char *extension);
int R_GetMaxTextureSize(void);
qboolean R_SupportsCompressedTextures(void);
```

**Network Features:**
```c
qboolean NET_HasFeature(int protocolVersion, uint32_t featureFlag);
qboolean NET_SupportsCompression(void);
```

### Capability Queries

**Server Capabilities:**
```c
typedef struct {
    int protocolVersion;
    uint32_t featureFlags;
    int maxClients;
    qboolean pure;
    // ...
} serverCapabilities_t;

qboolean CL_GetServerCapabilities(netadr_t *server, serverCapabilities_t *caps);
```

**Client Capabilities:**
```c
typedef struct {
    int protocolVersion;
    uint32_t featureFlags;
    const char *renderer;
    // ...
} clientCapabilities_t;

void CL_SendCapabilities(clientCapabilities_t *caps);
```

## Deprecation Policy

### Deprecation Timeline

1. **Announcement**: Feature marked as deprecated in documentation
2. **Warning Period**: 2-3 engine versions with warnings
3. **Removal**: Feature removed after deprecation period

### Deprecation Macros

```c
// Mark function as deprecated
#define DEPRECATED(msg) __attribute__((deprecated(msg)))

// Example
DEPRECATED("Use NewFunction() instead")
void OldFunction(void);

// Compile-time warning
#if defined(__GNUC__) || defined(__clang__)
    #define DEPRECATED_FUNCTION(name, replacement) \
        __attribute__((deprecated("Use " replacement " instead")))
#else
    #define DEPRECATED_FUNCTION(name, replacement)
#endif
```

### Runtime Deprecation Warnings

```c
// Warn on deprecated feature use
void Com_DeprecatedWarning(const char *feature, const char *replacement) {
    static qboolean warned = qfalse;
    if (!warned) {
        Com_Printf("WARNING: %s is deprecated. Use %s instead.\n", 
                   feature, replacement);
        warned = qtrue;
    }
}
```

## Testing Strategy

### Compatibility Test Suite

**Automated Tests:**
```c
// Test protocol compatibility
TEST(protocol_compatibility_v66) {
    // Test with protocol version 66
    ASSERT_TRUE(NET_ConnectWithProtocol(PROTOCOL_VERSION_66));
}

// Test save game migration
TEST(save_migration_v1_to_v2) {
    // Create v1 save
    Save_CreateV1Save("test_v1.save");
    
    // Migrate to v2
    ASSERT_TRUE(Save_Migrate("test_v1.save", 2));
    
    // Verify migration
    ASSERT_TRUE(Save_Load("test_v1.save"));
}
```

### Legacy Content Testing

**Test Matrix:**
- Old QVM mods (Urban Terror, Defrag, OpenArena)
- Old save games (various versions)
- Old network protocols (66, 67, 68)
- Old asset formats (legacy textures, models)

**CI Integration:**
```yaml
# Test legacy compatibility
- name: Test Legacy QVMs
  run: |
    ./tools/test_legacy_qvms.sh
    
- name: Test Save Game Migration
  run: |
    ./tools/test_save_migration.sh
```

### Golden File Testing

**Save Golden Files:**
- Known-good save files for each version
- Test loading and migration
- Verify data integrity

**Protocol Golden Packets:**
- Capture network packets from old versions
- Test parsing and compatibility
- Verify feature negotiation

## Migration Tools

### Save Game Migration Tool

```bash
# Command-line tool for migrating save games
./tools/migrate_save.sh old_save.save new_version
```

**Implementation:**
```c
int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: migrate_save <savefile> <target_version>\n");
        return 1;
    }
    
    const char *savePath = argv[1];
    int targetVersion = atoi(argv[2]);
    
    if (Save_Migrate(savePath, targetVersion)) {
        printf("Successfully migrated %s to version %d\n", savePath, targetVersion);
        return 0;
    } else {
        printf("Failed to migrate %s\n", savePath);
        return 1;
    }
}
```

### Protocol Compatibility Checker

```bash
# Check protocol compatibility
./tools/check_protocol_compat.sh server_address
```

### Asset Format Converter

```bash
# Convert assets to new format
./tools/convert_assets.sh --from-version 1 --to-version 2 mod_directory
```

## Implementation Priorities

### Phase 1: Foundation (High Priority)

1. **Protocol Negotiation**
   - Implement feature flags
   - Add protocol capability queries
   - Support multiple protocol versions

2. **Save Game Framework**
   - Unified save format header
   - Version detection
   - Basic migration framework

3. **Feature Detection**
   - Engine feature queries
   - Renderer capability checks
   - Network feature flags

### Phase 2: Migration (Medium Priority)

1. **Save Migration**
   - Automatic migration functions
   - Backup/restore system
   - Corruption recovery

2. **Asset Migration**
   - Format converters
   - Version detection
   - Compatibility layers

3. **Protocol Migration**
   - Legacy protocol support
   - Feature negotiation
   - Graceful degradation

### Phase 3: Tooling (Lower Priority)

1. **Migration Tools**
   - Save game migrator
   - Asset converter
   - Compatibility checker

2. **Testing Framework**
   - Automated compatibility tests
   - Legacy content testing
   - Golden file validation

## Best Practices

### For Developers

1. **Version Everything**: Always include version numbers
2. **Feature Detection**: Query before using features
3. **Graceful Degradation**: Provide fallbacks
4. **Document Changes**: Update compatibility docs
5. **Test Legacy Content**: Verify with old mods/saves

### For Mod Developers

1. **Check Features**: Query for feature availability
2. **Version Checks**: Verify protocol/save versions
3. **Fallbacks**: Provide legacy code paths
4. **Test Compatibility**: Test with multiple engine versions
5. **Report Issues**: Report compatibility problems

## Related Documentation

- [QVM Compatibility](QVM_COMPATIBILITY.md) - QVM-specific compatibility
- [C/C++ Boundary Rules](C_CPP_BOUNDARY_RULES.md) - C/C++ interop rules
- [Entity OOP Plan](entity_oop_plan.md) - OOP compatibility strategy
- [Asset Validation](ASSET_VALIDATION.md) - Asset compatibility checking

## Summary

**Key Strategies:**
1. ✅ Version all formats (protocol, save, assets, APIs)
2. ✅ Feature detection over assumptions
3. ✅ Graceful degradation for missing features
4. ✅ Migration frameworks for data formats
5. ✅ Deprecation policies with timelines
6. ✅ Comprehensive testing of legacy content
7. ✅ Migration tools for users

Following these strategies ensures that the engine maintains backwards compatibility while allowing for innovation and improvement.
