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

## License

This mod follows the same license as the quake3e engine and Quake III Arena.

