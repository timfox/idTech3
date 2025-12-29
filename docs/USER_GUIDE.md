# idTech3 Engine User Guide

## Installation and Setup

### Prerequisites

- Linux x86_64 system
- Graphics drivers (OpenGL or Vulkan)
- Game content files (.pk3 files)

### Quick Start

1. **Build the engine** (if not already built):
   ```bash
   ./tools/compile_engine.sh opengl
   ```

2. **Place game content** in the `release/base/` directory:
   - Place `.pk3` files (pak0.pk3, pak1.pk3, etc.) in `release/base/`
   - Or place content files directly in `release/base/` (maps/, textures/, etc.)

3. **Launch the engine**:
   ```bash
   cd release
   ./idtech3.x86_64
   ```

   Or use the launcher:
   ```bash
   ./idtech3_launcher
   ```

## Using the Launcher

The game launcher (`idtech3_launcher`) provides an easy way to start the engine:

- **Auto-detects content**: Automatically finds game content in common locations
- **Validates content**: Checks for required files before launching
- **Mod selection**: Supports mod selection via command line

### Launcher Usage

```bash
# Launch with base game
./idtech3_launcher

# Launch with a mod
./idtech3_launcher +set fs_game mymod

# Launch with custom arguments
./idtech3_launcher +set r_mode -1 +set r_customwidth 1920 +set r_customheight 1080
```

## Running the Engine

### Direct Execution

Run the engine binary directly:

```bash
cd release
./idtech3.x86_64
```

### Command Line Options

Common command line options:

- `+set fs_game <modname>` - Load a specific mod
- `+set cl_renderer <vulkan|opengl|opengl2>` - Select renderer
- `+set r_mode -1` - Use custom resolution
- `+set r_customwidth <width>` - Set custom width
- `+set r_customheight <height>` - Set custom height
- `+set r_fullscreen <0|1>` - Windowed (0) or fullscreen (1)
- `+map <mapname>` - Load a specific map on startup
- `+devmap <mapname>` - Load map in developer mode
- `+quit` - Exit after loading

### Examples

```bash
# Launch with Vulkan renderer
./idtech3.x86_64 +set cl_renderer vulkan

# Launch a specific map
./idtech3.x86_64 +map q3dm1

# Launch with a mod
./idtech3.x86_64 +set fs_game mymod +map mymap

# Launch in windowed mode with custom resolution
./idtech3.x86_64 +set r_mode -1 +set r_customwidth 1920 +set r_customheight 1080 +set r_fullscreen 0
```

## Content Organization

### Directory Structure

```
release/
├── idtech3.x86_64          # Engine binary
├── idtech3_vulkan_x86_64.so # Vulkan renderer
├── idtech3_opengl_x86_64.so  # OpenGL renderer
├── base/                    # Base game content
│   ├── pak0.pk3            # Game content pak files
│   ├── pak1.pk3
│   ├── maps/               # BSP map files
│   ├── textures/           # Texture files
│   ├── scripts/            # Shader files
│   └── default.cfg         # Default configuration
└── mods/                   # Mod directories
    └── mymod/
        ├── maps/
        ├── textures/
        └── scripts/
```

### Content Files

- **Pak files (.pk3)**: Zip files containing game content (maps, textures, sounds, etc.)
- **BSP files**: Compiled map files (placed in `maps/` directory)
- **Shader files**: Material definitions (placed in `scripts/` directory)
- **Texture files**: Image files (.tga, .jpg) for textures

## Renderer Selection

The engine supports multiple renderers with automatic fallback:

1. **Vulkan** (default, recommended for modern systems)
2. **OpenGL2** (fallback)
3. **OpenGL** (legacy fallback)

### Selecting a Renderer

```bash
# Use Vulkan (default)
./idtech3.x86_64 +set cl_renderer vulkan

# Use OpenGL
./idtech3.x86_64 +set cl_renderer opengl

# Use OpenGL2
./idtech3.x86_64 +set cl_renderer opengl2
```

If the requested renderer fails to load, the engine automatically tries the next available renderer.

## Loading Mods

### Method 1: Using the Launcher

```bash
./idtech3_launcher +set fs_game mymod
```

### Method 2: Direct Command Line

```bash
./idtech3.x86_64 +set fs_game mymod
```

### Method 3: Mod in mods/ Directory

If your mod is in `mods/mymod/`, the engine will find it automatically when you use `+set fs_game mymod`.

## Configuration

### Configuration Files

- `default.cfg`: Default engine settings (in `base/` directory)
- `q3config.cfg`: User configuration (saved in home directory)

### Common Settings

Edit `base/default.cfg` or use console commands:

```
seta r_mode "-1"              # Custom resolution
seta r_customwidth "1920"     # Custom width
seta r_customheight "1080"    # Custom height
seta r_fullscreen "0"         # Windowed mode
seta com_maxfps "125"         # Maximum FPS
seta s_volume "0.5"           # Sound volume
seta s_musicvolume "0.5"      # Music volume
```

## Troubleshooting

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for common issues and solutions.

## Getting Help

- Check the console output for error messages
- Review `crash_report.txt` if the engine crashes
- See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for common problems
