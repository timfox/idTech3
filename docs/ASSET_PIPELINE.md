# Asset Pipeline Architecture

## Asset Philosophy

The asset pipeline is designed with **zero breaking changes** to existing Id Tech 3 content while providing **modern asset management** capabilities:

- **PK3 Compatibility**: All existing assets work unchanged
- **Progressive Enhancement**: Modern formats load when available
- **Streaming Architecture**: Background loading and caching
- **Validation System**: Automatic asset integrity checking

## Asset Loading Architecture

### Layered Loading System
```
┌─────────────────────────────────────┐
│         MOD ASSETS                   │
│  (pk3 files, loose files)            │
├─────────────────────────────────────┤
│      ASSET VALIDATION               │
│  (integrity, format checking)       │
├─────────────────────────────────────┤
│     FORMAT CONVERSION               │
│  (legacy → modern formats)          │
├─────────────────────────────────────┤
│       CACHE SYSTEM                  │
│  (memory, disk caching)             │
├─────────────────────────────────────┤
│     RENDERER UPLOAD                 │
│  (GPU memory management)            │
└─────────────────────────────────────┘
```

### Asset Discovery
```c
// Multi-path asset resolution
const char *paths[] = {
    "mymod/textures/",      // Mod-specific
    "baseq3/textures/",     // Base game
    "textures/",            // Fallback
    NULL
};
```

## PK3 File Format

### Structure Preservation
PK3 files remain the **primary asset container**:
```
mymod/
├── pk0.pk3          # Base assets
├── maps/            # BSP files
├── textures/        # Loose overrides
├── scripts/         # Shader files
└── sound/           # Audio files
```

### Enhanced PK3 Features
- **Streaming Decompression**: Background loading
- **Memory Mapping**: Zero-copy access where possible
- **Integrity Checking**: Automatic corruption detection
- **Hot Reloading**: Development asset replacement

## Shader System

### Shader File Format
Shaders use **material-like syntax** with **backward compatibility**:

```glsl
// Modern shader with fallbacks
textures/mymod/wall01
{
    // Modern PBR properties
    {
        map textures/mymod/wall01_d.ktx2
        normalMap textures/mymod/wall01_n.ktx2
        roughnessMap textures/mymod/wall01_r.ktx2
        metallicMap textures/mymod/wall01_m.ktx2
    }

    // Fallback for legacy compatibility
    {
        map textures/mymod/wall01.tga
        rgbGen identity
    }
}
```

### Shader Processing Pipeline
```
Source Shader → Preprocessing → Optimization → Compilation → Caching
      ↓             ↓              ↓            ↓           ↓
   .shader     Macro expansion  Dead code    SPIR-V     Pipeline
   files       Conditionals     removal     shaders     cache
```

### Shader Features
- **Conditional Compilation**: Platform-specific optimizations
- **Macro System**: Reusable shader components
- **Validation**: Compile-time error checking
- **Hot Reloading**: Real-time shader iteration

## Texture System

### Format Support Hierarchy
```c
// Progressive format loading
const char *extensions[] = {
    ".ktx2",    // Modern: BasisU + ASTC/ETC2
    ".dds",     // Modern: S3TC + custom formats
    ".tga",     // Legacy: Uncompressed
    ".jpg",     // Legacy: Lossy compression
    ".png",     // Legacy: PNG with alpha
    NULL
};
```

### Texture Processing Pipeline
```
Source → Mip Generation → Compression → Upload → Caching
   ↓            ↓              ↓         ↓        ↓
.ktx2     Automatic       GPU format  VRAM    Disk cache
.dds      based on        selection   upload  for fast
.tga      texture size    (ASTC/ETC2)         reload
```

### Texture Features
- **Streaming Mipmaps**: Progressive loading
- **VRAM Management**: Automatic eviction
- **Format Conversion**: Runtime transcoding
- **Quality Scaling**: Dynamic resolution based on settings

## Asset Validation

### Validation Types
```c
typedef enum {
    VALIDATE_EXISTENCE,     // File exists
    VALIDATE_INTEGRITY,     // Checksum validation
    VALIDATE_FORMAT,        // Format compliance
    VALIDATE_DEPENDENCIES,  // Required assets present
    VALIDATE_PERFORMANCE    // Size/time limits
} validation_type_t;
```

### Automatic Validation
- **Startup Checks**: Critical assets validated on load
- **Background Scanning**: Non-critical assets checked asynchronously
- **Error Recovery**: Automatic fallback to working assets
- **Logging**: Detailed validation results

## Asset Cooking Pipeline

### Cooked Asset Benefits
- **Faster Loading**: Preprocessed assets
- **Smaller Size**: Optimized compression
- **Better Quality**: GPU-specific formats
- **Validation**: Pre-load integrity checking

### Cooking Process
```bash
# Asset cooking workflow
./tools/asset_cooker input/ output/ --format ktx2 --quality high

# Validation
./tools/asset_validator cooked_assets/ --manifest manifest.json
```

### Cooking Features
- **Batch Processing**: Multiple assets simultaneously
- **Format Conversion**: Legacy → modern formats
- **Compression Optimization**: Quality vs size tradeoffs
- **Dependency Resolution**: Automatic asset linking

## Mod Compatibility

### Compatibility Levels
```c
typedef enum {
    COMPAT_LEGACY,      // Original Id Tech 3 (100% compatible)
    COMPAT_ENHANCED,    // Modern features with fallbacks
    COMPAT_MODERN       // Modern features required
} compatibility_level_t;
```

### Compatibility Matrix
| Feature | Legacy Mods | Enhanced Mods | Modern Mods |
|---------|-------------|---------------|-------------|
| PK3 Loading | ✅ | ✅ | ✅ |
| TGA Textures | ✅ | ✅ | ✅ |
| Basic Shaders | ✅ | ✅ | ✅ |
| PBR Materials | ⚠️ (fallback) | ✅ | ✅ |
| KTX2 Textures | ⚠️ (fallback) | ✅ | ✅ |
| Mesh Shaders | ❌ | ⚠️ (fallback) | ✅ |

## Development Workflow

### Asset Iteration
```bash
# Development mode with hot reloading
./idtech3.x86_64 +set fs_game mymod +set developer 1

# Asset cooking for release
./scripts/cook_assets.sh mymod/

# Validation before commit
./scripts/validate_assets.sh mymod/
```

### Asset Organization
```
mymod/
├── assets/           # Source assets (TGA, PNG, etc.)
├── cooked/           # Processed assets (KTX2, optimized)
├── scripts/          # Shader definitions
├── maps/            # BSP levels
├── models/          # MD3/MD5 models
├── sound/           # WAV/OGG audio
└── manifests/       # Asset metadata
```

## Performance Considerations

### Loading Priorities
- **Critical Assets**: Block loading (weapons, HUD)
- **Important Assets**: High priority streaming
- **Background Assets**: Low priority prefetching

### Memory Management
- **VRAM Budgeting**: Automatic quality scaling
- **Cache Management**: LRU eviction policies
- **Streaming Limits**: Bandwidth and memory caps

### Quality Settings
```c
// Dynamic quality scaling
if (r_picmip->integer > 0) {
    // Reduce texture resolution
    scale_factor = 1 << r_picmip->integer;
}
```

---

## Key Design Decisions

### Why Preserve PK3?
**Zero Breaking Changes**: Existing mods work without modification.

### Why Modern Formats?
**Performance**: Better compression and GPU compatibility.

### Why Validation?
**Reliability**: Prevent crashes from corrupted assets.

### Why Cooking?
**Optimization**: Preprocessing improves runtime performance.

The asset pipeline maintains **perfect backward compatibility** while providing a **clear upgrade path** to modern asset formats and workflows.