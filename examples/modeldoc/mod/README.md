# ModelDoc - idTech3 Model Documentation System

## Overview

ModelDoc is a first-party idTech3 mod that provides comprehensive model documentation and validation tools. It runs as a native game mod using the idTech3 engine as its rendering foundation, with ImGui for UI and properties inspection.

## Features

- **Model Browser** - Browse all models in `models/` directories
- **WYSIWYG Preview** - Real-time rendering with PBR, RTX, neural GI
- **Format Validation** - Check model integrity (MD3, glTF, IQM, MD5, USD)
- **Material Inspection** - View material parameters, textures, shaders
- **Animation Preview** - Test skeletal animations, morph targets
- **Collision Validation** - Verify BSP collision, physics bounds
- **Export Tools** - Generate documentation, export to various formats

## Architecture

ModelDoc uses a **Scene + GameObjects + Components** architecture:

### Core Components

- **Scene** - Container for all game objects
- **GameObject** - Entity with components
- **Component** - Reusable functionality modules

### Components

- **ModelPreviewComponent** - Handles model rendering and preview
- **ModelBrowserComponent** - Handles model browsing and selection
- **ValidationComponent** - Handles model validation
- **ExportComponent** - Handles documentation export

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

### Console Commands

- `modeldoc_reload` - Reload ModelDoc scripts
- `modeldoc_open` - Open ModelDoc UI
- `modeldoc_validate <model_path>` - Validate a model
- `modeldoc_export <model_path> <output_path> <format>` - Export documentation
- `modeldoc_browser_refresh` - Refresh model browser
- `modeldoc_preview <model_path>` - Preview a model

### Lua API

```lua
-- Initialize ModelDoc
modeldoc_init()

-- Update ModelDoc (call every frame)
modeldoc_update()

-- Shutdown ModelDoc
modeldoc_shutdown()

-- Validate a model
modeldoc_validate("models/weapon.md3")

-- Export a model
modeldoc_export("models/weapon.md3", "export/weapon.pdf", "pdf")

-- Refresh model browser
modeldoc_browser_refresh()

-- Preview a model
modeldoc_preview("models/weapon.md3")
```

## Mod Structure

```
mod/
├── autoexec.cfg          # Auto-executed on mod load
├── mod.cfg               # Mod configuration
├── ui/
│   └── modeldoc.cfg      # UI configuration
├── shaders/
│   └── modeldoc/         # Custom shaders
│       └── modeldoc.cfg
├── scripts/
│   └── lua/
│       └── modeldoc.lua  # Main Lua script
└── models/               # Demo models (optional)
```

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

## Integration with idTech3

ModelDoc integrates with existing idTech3 systems:

- **Asset Pipeline** - Uses `FS_ReadFile()` for `.pk3` access
- **Renderer** - `RE_RegisterModel()`, `RE_RegisterShader()`
- **Console** - `modeldoc_*` commands
- **CGame** - `trap_ModelDoc_*` functions

## Future Enhancements

- **3D Model Comparison** - Side-by-side model comparison
- **Animation Timeline** - Frame-by-frame animation editor
- **Material Editor** - Live material parameter adjustment
- **Batch Validation** - Validate entire mod directories
- **CI Integration** - Automated validation for CI/CD
- **Web Export** - HTML-based documentation generation

## References

- [MODEL_FORMATS.md](../../docs/MODEL_FORMATS.md) - Supported model formats
- [GLTF.md](../../docs/GLTF.md) - glTF 2.0 documentation
- [FREEUSD.md](../../docs/FREEUSD.md) - USD/DAE support
- [RENDERER_2027.md](../../docs/RENDERER_2027.md) - Vulkan renderer architecture

## License

GPL-2.0 (same as idTech3 engine)

## Author

idTech3 Team

## Version

1.0.0 (2026)