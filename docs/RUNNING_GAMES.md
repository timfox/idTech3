# Running Games with idTech3 Engine

## Quick Start

The engine has been successfully built and you can now run several idTech3-based games! 🎮

**Latest builds are automatically copied to the `release/` folder after compilation.**

## Available Games

### 1. OpenArena
- **Open source Quake 3 clone**
- **Full game content included**
- **Location**: `release/openarena/`

### 2. Quake 3 Arena (Base Game)
- **Classic Quake 3 Arena**
- **Content included in `release/base/`**
- **Location**: `release/base/`

### 3. MyMod (Custom Game)
- **Custom mod with unique content**
- **Includes custom assets and gameplay**
- **Location**: `release/mymod/`

## How to Run Games

### Option 1: Interactive Menu (Recommended)
```bash
cd scripts
./run_menu.sh
```
This will show you a menu to choose which game to run.

### Option 2: Direct Scripts
```bash
cd scripts

# OpenArena (recommended for first try)
./run_openarena.sh

# OpenArena with Vulkan renderer (better performance)
./run_openarena_vulkan.sh

# Quake 3 Arena
./run_quake3.sh

# Custom MyMod
./run_mymod.sh
```

### Option 3: Manual Commands
```bash
cd release

# OpenArena
./idtech3.x86_64 +set fs_game openarena +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 0

# Quake 3 Arena (base game)
./idtech3.x86_64 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 0

# MyMod
./idtech3.x86_64 +set fs_game mymod +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 0
```

## Configuration Options

### Video Settings
- `+set r_mode 6` - Windowed mode at 1024x768
- `+set r_fullscreen 0` - Windowed mode (change to 1 for fullscreen)
- `+set r_vulkan 0` - Use OpenGL renderer (change to 1 for Vulkan)

### Other Useful Commands
- `+set vid_xpos 100 +set vid_ypos 100` - Position window on screen
- `+set s_volume 0.5` - Set sound volume
- `+set sensitivity 5` - Set mouse sensitivity

## Troubleshooting

### Game won't start?
1. Make sure you're in the `release/` directory
2. Check that the game assets exist in the respective directories
3. Try running with OpenGL first (`r_vulkan 0`)

### Performance Issues?
1. Try the Vulkan renderer: `+set r_vulkan 1`
2. Lower resolution: `+set r_mode 3` (640x480)
3. Check your graphics drivers

### Black screen or crash?
1. Try windowed mode: `+set r_fullscreen 0`
2. Check the console output for error messages
3. Try different video modes

## Advanced Features

### Your KVP Cvar System
The engine now includes your advanced JSON-based KVP (Key-Value Pair) cvar system! This allows for complex configuration of:

- **Renderer settings**: multisampling, texture filtering, post-processing
- **Input configuration**: key bindings with modifiers, controller mappings
- **Audio settings**: spatial audio, effects, performance options
- **Network configuration**: QoS parameters, security settings

### Vulkan Renderer Features
- Ray tracing support
- Physically based rendering (PBR)
- Global illumination
- High-quality lighting
- FSR upscaling

## What's Next?

1. **Try OpenArena first** - it's the most complete game
2. **Experiment with Vulkan** for better performance
3. **Customize controls** in the in-game menus
4. **Explore the KVP system** for advanced configuration

## Support

If you encounter issues:
1. Check the terminal output for error messages
2. Try different renderer options
3. Verify game assets are present
4. Check system requirements (OpenGL 3.3+ or Vulkan 1.1+)

Enjoy playing! 🚀