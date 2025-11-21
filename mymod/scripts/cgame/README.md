# CGame Module (Client-Side)

This directory contains the client-side game logic source code.

## Overview

The cgame module runs on the client and handles:
- Client-side prediction
- Visual effects
- HUD rendering
- Client-side events
- View rendering
- Input handling

## Compilation

### Native Compilation (Recommended)

To compile as a native shared library:

1. Place your C source files in this directory
2. Build using CMake or Makefile from parent directory:
   ```bash
   cd ..
   make cgame
   ```
3. Output: `../vm/cgame.x86_64.so` (or `.dll` on Windows)
4. Enable: `set vm_cgame 0`

### QVM Compilation (Legacy)

To compile as QVM bytecode:

1. Place your C source files in this directory
2. Use a Quake III compiler (q3asm or similar)
3. Compile to `../vm/cgame.qvm`
4. The compiled QVM file will be loaded automatically

## File Structure

Typical cgame module files:
- `cg_main.c` - Main client game initialization
- `cg_view.c` - View rendering
- `cg_draw.c` - HUD drawing
- `cg_players.c` - Player rendering
- `cg_weapons.c` - Weapon rendering
- `cg_effects.c` - Visual effects
- `cg_utils.c` - Utility functions

## API Reference

The cgame module communicates with the engine through system calls defined in `cg_public.h`.

## PBR Integration

When modifying visual effects, consider:
- PBR materials are handled by the renderer automatically
- Custom shaders can be placed in `../shaders/`
- Visual enhancements should work with PBR rendering

## Notes

- This is client-side code only
- Changes here affect visual presentation
- Requires QVM compilation tools
- See main mod README.md for compilation instructions

