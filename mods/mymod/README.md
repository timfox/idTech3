# MyMod - Hybrid Gameplay and Visual Enhancement Mod

A modern mod for Quake III Arena featuring enhanced gameplay mechanics and physically-based rendering (PBR) visual improvements using the quake3e engine.

## Installation

1. Copy the `mymod` directory to your Quake III Arena installation directory
2. Launch the game with: `./quake3e +set fs_game mymod`
3. Or add `+set fs_game mymod` to your launch options

The mod will automatically load the configuration from `config/mymod.cfg` on startup.

## Features

### Visual Enhancements
- **Physically Based Rendering (PBR)** - Realistic material rendering with proper light interaction
- **HDR Rendering** - High dynamic range for better color accuracy and reduced banding
- **Environment Mapping** - Realistic reflections using cube mapping
- **Normal Mapping** - Enhanced surface detail with parallax support
- **Anisotropic Filtering** - Improved texture quality at oblique angles

### Gameplay Enhancements
- Custom game logic via QVM modules
- Enhanced client-side effects
- Improved user interface

## PBR Feature Usage

This mod utilizes the Vulkan renderer's PBR capabilities. To get the best visual results:

1. **Enable PBR Rendering**: The mod automatically sets `r_pbr 1` in the config
2. **Required Settings**: `r_fbo 1` must be enabled (set automatically)
3. **Recommended Settings**: `r_vbo 1` for better performance with static geometry

### Manual Configuration

If you need to adjust settings manually, use these console commands:

```
set r_pbr 1          // Enable PBR rendering
set r_fbo 1          // Enable framebuffer objects (REQUIRED)
set r_vbo 1          // Enable vertex buffer objects (recommended)
set r_cubeMapping 1  // Enable environment reflections
set r_hdr 1          // Enable HDR rendering
```

## Texture Naming Conventions

For PBR materials to work correctly, textures should follow these naming conventions:

- **Base Texture**: `texturename.tga` (or `.jpg`, `.png`)
- **Normal Map**: `texturename_normal.tga`
- **ORM Map**: `texturename_orm.tga` (Occlusion in R, Roughness in G, Metallic in B)
- **Alternative ORM**: `texturename_rmo.tga` (Roughness in R, Metallic in G, Occlusion in B)
- **Specular Map**: `texturename_spec.tga` (for specular/gloss workflow)

See `PBR_GUIDE.md` for detailed information on creating PBR materials.

## Game Module Compilation

This mod supports **native C compilation** (recommended) or traditional QVM compilation.

### Native C Compilation (Recommended)

Compile your game modules as native shared libraries to use modern C features:

1. Place your source code in the `gamesrc/` directory:
   - `gamesrc/game/` - Server-side game logic
   - `gamesrc/cgame/` - Client-side game logic
   - `gamesrc/ui/` - User interface code

2. Build using CMake or Makefile:
   ```bash
   cd scripts
   mkdir build && cd build
   cmake ..
   make
   ```
   Or use the Makefile:
   ```bash
   cd scripts
   make all
   ```

3. Compiled libraries will be in `vm/` directory:
   - `vm/game.x86_64.so` (Linux) or `vm/game.x64.dll` (Windows)
   - `vm/cgame.x86_64.so` / `vm/cgame.x64.dll`
   - `vm/ui.x86_64.so` / `vm/ui.x64.dll`

4. Enable native loading:
   ```
   set vm_game 0
   set vm_cgame 0
   set vm_ui 0
   ```

**Benefits**: Modern C11/C17 features, full standard library, better performance, easier debugging.

See `NATIVE_COMPILATION.md` for detailed instructions.

### QVM Compilation (Legacy)

If you prefer QVM compilation:

1. Place your source code in the `gamesrc/` directory
2. Compile using a Quake III compiler (q3asm or similar tools)
3. Place compiled `.qvm` files in the `vm/` directory:
   - `vm/game.qvm`
   - `vm/cgame.qvm`
   - `vm/ui.qvm`

**Note**: QVM uses an old C compiler with limited features. Native compilation is recommended for modern development.

## Asset Creation Guidelines

### Maps
- Place `.bsp` map files in the `maps/` directory
- Maps should be compatible with Quake III Arena format
- Consider PBR material usage when creating maps

### Textures
- Supported formats: TGA, JPG, PNG
- Place standard textures in `textures/`
- Place PBR material textures in `textures/pbr/`
- See `PBR_GUIDE.md` for texture creation guidelines

### Models
- Place model files (`.md3`, `.md3mesh`, etc.) in `models/`
- Models should follow Quake III Arena model format

### Sounds
- Place sound files (`.wav`) in `sound/`
- Supported format: WAV

### Shaders
- Custom shader files go in `shaders/`
- Shaders use Quake III shader syntax
- PBR shaders are handled automatically by the renderer

## Directory Structure

```
mymod/
├── description.txt          # Mod description (shown in mod menu)
├── README.md               # This file
├── PBR_GUIDE.md           # PBR material creation guide
├── config/
│   └── mymod.cfg          # Default configuration
├── vm/                     # Compiled QVM files
│   ├── game.qvm
│   ├── cgame.qvm
│   └── ui.qvm
├── gamesrc/                # Source code (game modules)
│   ├── game/
│   ├── cgame/
│   └── ui/
├── maps/                   # Custom maps (.bsp files)
├── textures/               # Standard textures
│   └── pbr/               # PBR material textures
├── models/                 # 3D models
├── sound/                  # Sound effects
└── shaders/                # Custom shader files
```

## Testing

To test the mod:

1. Launch: `./quake3e +set fs_game mymod`
2. Check console for any errors
3. Verify PBR is enabled: `r_pbr` should show `1`
4. Verify FBO is enabled: `r_fbo` should show `1`
5. Load a map to see visual improvements

## Troubleshooting

### PBR Not Working
- Ensure `r_fbo 1` is set (required for PBR)
- Check that you're using the Vulkan renderer: `cl_renderer vulkan`
- Verify texture naming conventions are correct

### Mod Not Loading
- Check that `mymod` directory is in the correct location
- Verify `description.txt` exists in the mod root
- Check console for filesystem errors

### Performance Issues
- Disable HDR if needed: `set r_hdr 0`
- Reduce cube mapping: `set r_cubeMapping 0`
- Adjust PBR quality settings

## Additional Resources

- [quake3e GitHub Repository](https://github.com/quake3e/quake3e)
- [Quake III Arena Modding Documentation](https://ioquake3.org/)
- See `PBR_GUIDE.md` for detailed PBR material creation instructions

## 🚀 Advanced Features Demonstration

This mod showcases the cutting-edge features of the enhanced Quake III engine, including advanced rendering, modern UI systems, and Lua scripting capabilities.

### 🎨 Font Rendering Enhancements

**TrueType Font Support:**
- High-quality font rendering with FreeType 2.x
- Subpixel rendering for crisp text on LCD displays
- Unicode support with fallback font chains
- Font hinting and anti-aliasing controls

**Configuration:**
```bash
// Enable high-quality font rendering
set r_fontQuality 2        // 0=fast, 1=normal, 2=high
set r_fontHinting 2        // Font hinting level
set r_fontSubpixel 1       // Subpixel rendering
```

**Font Files:**
- `fonts/FX300.ttf` - Primary UI font (clean, readable)
- `fonts/roboto-regular.ttf` - System-style font with excellent Unicode support
- `fonts/2197 Heavy.ttf` - Bold decorative headers
- `fonts/WO3.ttf` - Retro game-style font

### 🎮 Lua Scripting System

**Advanced Scripting Features:**
- Event-driven architecture with coroutines
- Sequence/timeline system for cinematics
- Entity scripting with spawn/think/touch hooks
- Performance monitoring integration

**Example Scripts:**
```lua
-- Enhanced cinematic with multiple systems
require("lib/sequence")
require("lib/encounter")

Sequence.create("demo_cinematic", {
    { time = 0.0, action = function() UI.show_subtitle("Welcome!", 3.0) end },
    { time = 2.0, action = function() Effects.spawn_particles("sparks", pos, 10) end },
    { time = 5.0, action = function() Audio.play_sound("explosion") end }
})

-- Event system integration
Events.on("player_spawned", function(data)
    print("Player " .. data.name .. " spawned!")
end)

-- Performance monitoring
if Performance then
    local fps = Performance.get_fps()
    print("Current FPS: " .. fps)
end
```

**Available Libraries:**
- `lib/sequence.lua` - Timeline-based sequences
- `lib/encounter.lua` - Combat encounter management
- Event system integration
- UI and audio control APIs

### 🖼️ UI Enhancement Features

**Modern UI System:**
- Responsive layouts with automatic scaling
- Animation systems with customizable speeds
- Background blur effects
- Enhanced accessibility options

**Configuration:**
```bash
// UI enhancements
set ui_scale 1.2           // UI scaling (0.5-2.0)
set ui_blur 1              // Background blur
set ui_animationSpeed 1.5  // Animation speed

// HUD improvements
set cg_hudScale 1.1        // HUD scaling
set cg_hudGlow 1           // HUD glow effects
set cg_crosshairGlow 1     // Enhanced crosshair
```

**Font Effects:**
- Text glow, outline, and shadow effects
- Color customization for different UI elements
- High contrast mode for accessibility
- Color blind support

### 🎬 Advanced Rendering Pipeline

**Next-Gen Visual Effects:**
```bash
// Post-processing
set r_bloom 1              // Bloom lighting
set r_tonemapMode 1        // HDR tonemapping
set r_ssao 1               // Ambient occlusion

// Advanced features
set r_temporal_aa 1        // Anti-aliasing
set r_fsr 1                // AMD FSR upscaling
set r_volumetric_lighting 1 // Volumetric effects
```

**Material System:**
```bash
// Advanced materials
set r_advanced_materials 1 // Multi-layer materials
set r_parallax_occlusion 1 // Parallax mapping
set r_subsurface_scattering 1 // SSS materials
```

### 📊 Performance Monitoring

**Real-time Performance Tracking:**
```bash
// Enable monitoring
set perf_monitor_enable 1     // Master switch
set perf_gpu_profiler 1       // GPU profiling
set perf_memory_profiler 1    // Memory tracking
set perf_csv_export 1         // Data export

// Display options
set com_speeds 1             // FPS display
set com_memoryStats 1        // Memory stats
```

**Performance Regression Detection:**
```bash
set perf_regression_detection 1    // Enable detection
set perf_alert_threshold 16.67     // Frame time alerts (ms)
set perf_baseline_frames 300       // Baseline frame count
```

### 🛡️ Stability & Recovery Features

**Crash Recovery System:**
```bash
set com_crash_recovery 1       // Auto recovery
set com_crash_minidump 1       // Generate dumps
set com_safe_mode_detect 1     // Safe mode detection
```

**Memory Protection:**
```bash
set com_memory_guard 1         // Memory corruption detection
set com_thread_safety 1        // Thread safety checks
set mem_pool_enable 1          // Memory pooling
```

### 🎯 Quick Start Guide

1. **Enable Advanced Features:**
   ```bash
   cd /home/tim/Desktop/idtech3/release
   ./idtech3.x86_64 +set fs_game mymod
   ```

2. **Test Font Rendering:**
   ```bash
   // In console
   set r_fontQuality 2
   set r_fontHinting 2
   set ui_scale 1.2
   ```

3. **Try Lua Scripting:**
   ```bash
   // Load and run a script
   lua_exec "require('examples/cinematic'); cinematic.play_enhanced_intro()"
   ```

4. **Enable Visual Effects:**
   ```bash
   set r_pbr 1
   set r_bloom 1
   set r_ssao 1
   vid_restart
   ```

5. **Monitor Performance:**
   ```bash
   set perf_monitor_enable 1
   set com_speeds 1
   ```

### 🔧 Development Tools

**Available Scripts:**
- `tools/run_game.sh mymod` - Launch with mod
- `tools/compile_game.sh mymod` - Rebuild mod
- `tools/run_benchmarks.sh` - Performance testing

**Debug Commands:**
- `lua_debug` - Lua script debugging
- `ui_debug` - UI system debugging
- `font_debug` - Font rendering debug info
- `perf_debug` - Performance monitoring

### 📚 Advanced Configuration

**Create Custom Config Files:**
```bash
// advanced_rendering.cfg
set r_pbr 1
set r_ssao 1
set r_bloom 1
set r_temporal_aa 1
set r_volumetric_lighting 1

// ui_enhancements.cfg
set ui_scale 1.2
set ui_blur 1
set ui_animationSpeed 1.5
set cg_hudGlow 1
set cg_crosshairGlow 1

// performance_monitoring.cfg
set perf_monitor_enable 1
set perf_gpu_profiler 1
set perf_csv_export 1
set com_speeds 1
```

**Load Custom Configs:**
```bash
exec advanced_rendering.cfg
exec ui_enhancements.cfg
exec performance_monitoring.cfg
```

### 🎮 Feature Compatibility

| Feature | Vulkan | OpenGL | Requirements |
|---------|--------|--------|--------------|
| PBR Rendering | ✅ | ✅ | r_fbo 1 |
| SSAO | ✅ | ⚠️ | Compute shaders |
| Bloom | ✅ | ✅ | Post-processing |
| Temporal AA | ✅ | ⚠️ | Advanced shaders |
| FSR | ✅ | ❌ | Vulkan only |
| Volumetric Lighting | ✅ | ⚠️ | Compute shaders |
| Advanced Materials | ✅ | ✅ | Shader model 4.0+ |

### 🚀 Future Enhancements

**Planned Features:**
- Neural rendering and DLSS 3.0+
- Cloud gaming capabilities
- AI-assisted development tools
- Metaverse/social features
- Advanced physics simulation

**Research Areas:**
- Real-time ray tracing
- Machine learning integration
- Procedural content generation
- Cross-platform optimization

### 📞 Support & Development

**For issues or feature requests:**
- Check console output for error messages
- Use debug commands for diagnostics
- Review log files in the mod directory
- Check the main engine repository for updates

**Contributing:**
- Test features on different hardware
- Report performance metrics
- Suggest improvements and new features
- Share custom scripts and configurations

---

## License

This mod follows the same license as the quake3e engine and Quake III Arena.

