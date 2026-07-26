# ModelDoc - idTech3 Model Documentation System

## Overview

ModelDoc is a first-party idTech3 mod that provides comprehensive model documentation and validation tools. It runs as a native game mod using the idTech3 engine as its rendering foundation, with ImGui for UI and properties inspection.

## Architecture

### Core Components

- **ModelDoc Mod** (`game/modeldoc/`) - Lua-based mod with C++ backend
- **ImGui UI** - Properties panel, model browser, validation dashboard
- **Renderer Integration** - Uses idTech3 Vulkan renderer for WYSIWYG preview
- **Asset Pipeline** - Reads `.pk3` archives, validates model formats

### Features

- **Model Browser** - Browse all models in `models/` directories
- **WYSIWYG Preview** - Real-time rendering with PBR, RTX, neural GI
- **Format Validation** - Check model integrity (MD3, glTF, IQM, MD5, USD)
- **Material Inspection** - View material parameters, textures, shaders
- **Animation Preview** - Test skeletal animations, morph targets
- **Collision Validation** - Verify BSP collision, physics bounds
- **Export Tools** - Generate documentation, export to various formats

## Implementation

### Mod Structure

```
mods/modeldoc/
├── scripts/
│   ├── modeldoc.lua          # Main mod logic
│   ├── model_browser.lua     # Model browsing UI
│   ├── preview.lua           # WYSIWYG preview system
│   └── validation.lua        # Validation checks
├── ui/
│   ├── modeldoc.cfg          # UI configuration
│   └── modeldoc.guiconfig    # ImGui settings
├── shaders/
│   └── modeldoc/             # Custom shaders for documentation
│       ├── model_preview.frag
│       └── model_debug.vert
├── models/                   # Demo models for testing
├── scripts/                  # Lua scripts
└── pk3/                      # Build configuration
```

### Key Files

#### `scripts/modeldoc.lua`

Main mod entry point with:
- Model loading and caching
- ImGui UI initialization
- Validation rule definitions
- Export functionality

#### `ui/modeldoc.cfg`

Configuration file with:
- UI layout settings
- Default viewports
- Validation thresholds
- Export options

### Rendering Pipeline

ModelDoc uses the idTech3 Vulkan renderer with these key features:

1. **PBR Materials** - Base color, metallic/roughness, normal, emissive
2. **RTX Support** - Real-time ray tracing for accurate lighting
3. **Neural GI** - NDGI, NIV, NVC for production-quality lighting
4. **HDR Pipeline** - 16-bit float, tone mapping, bloom
5. **TAA** - Temporal anti-aliasing for smooth motion

### Lua API

```lua
-- ModelDoc API
ModelDoc = {
    -- Load and preview a model
    loadModel = function(modelPath)
        -- Loads model into preview viewport
    end,
    
    -- Validate model format
    validateModel = function(modelPath)
        -- Returns validation report
    end,
    
    -- Get model metadata
    getModelInfo = function(modelPath)
        -- Returns table with model info
    end,
    
    -- Export documentation
    exportModel = function(modelPath, outputPath, format)
        -- Exports to PDF, HTML, etc.
    end,
    
    -- Validation checks
    checks = {
        "mesh_integrity",
        "texture_resolution",
        "material_completeness",
        "animation_validity",
        "collision_bounds",
        "shader_compatibility"
    }
}
```

## Usage

### Starting ModelDoc

```bash
# From release directory
./idtech3 +set fs_game modeldoc +set com_scriptWatch 1

# Or from development
./build-vk-Release/idtech3 +set fs_game modeldoc +set com_scriptWatch 1
```

### UI Controls

- **F1** - Toggle ModelDoc UI
- **F2** - Toggle validation overlay
- **F3** - Cycle preview modes (PBR, RTX, Debug)
- **Ctrl+O** - Open model browser
- **Ctrl+E** - Export documentation

### Validation Dashboard

The main UI shows:
- Model tree view
- Properties panel
- Validation results
- Export options

## Integration with idTech3

ModelDoc integrates with existing idTech3 systems:

- **Asset Pipeline** - Uses `FS_ReadFile()` for `.pk3` access
- **Renderer** - `RE_RegisterModel()`, `RE_RegisterShader()`
- **Console** - `modeldoc_*` commands
- **CGame** - `trap_ModelDoc_*` functions

## Build System

### Development Build

```bash
./scripts/compile_engine.sh vulkan game
./scripts/compile_engine.sh vulkan full  # With all extensions
```

### Package Build

```bash
# Create pk3
./scripts/generate_mod_workspace.sh modeldoc
# Build pk3 with assets
```

## Future Enhancements

- **3D Model Comparison** - Side-by-side model comparison
- **Animation Timeline** - Frame-by-frame animation editor
- **Material Editor** - Live material parameter adjustment
- **Batch Validation** - Validate entire mod directories
- **CI Integration** - Automated validation for CI/CD
- **Web Export** - HTML-based documentation generation

## References

- [MODEL_FORMATS.md](MODEL_FORMATS.md) - Supported model formats
- [GLTF.md](GLTF.md) - glTF 2.0 documentation
- [FREEUSD.md](FREEUSD.md) - USD/DAE support
- [RENDERER_2027.md](RENDERER_2027.md) - Vulkan renderer architecture