# Game Module (Server-Side)

This directory contains the server-side game logic source code.

## Overview

The game module runs on the server and handles:
- Game rules and mechanics
- Entity management
- Player interactions
- Weapon systems
- Game modes
- Server-side events

## Compilation

### Native Compilation (Recommended)

To compile as a native shared library:

1. Place your C source files in this directory
2. Build using CMake or Makefile from parent directory:
   ```bash
   cd ..
   mkdir build && cd build
   cmake ..
   make game
   ```
   Or:
   ```bash
   cd ..
   make game
   ```
3. Output: `../vm/game.x86_64.so` (or `.dll` on Windows)
4. Enable: `set vm_game 0`

### QVM Compilation (Legacy)

To compile as QVM bytecode:

1. Place your C source files in this directory
2. Use a Quake III compiler (q3asm or similar)
3. Compile to `../vm/game.qvm`
4. The compiled QVM file will be loaded automatically

## File Structure

Typical game module files:
- `g_main.c` - Main game initialization
- `g_weapon.c` - Weapon handling
- `g_client.c` - Client management
- `g_combat.c` - Combat system
- `g_misc.c` - Miscellaneous functions
- `g_utils.c` - Utility functions

## API Reference

The game module communicates with the engine through system calls defined in `g_public.h`.

## Notes

- This is server-side code only
- Changes here affect game mechanics and rules
- Requires QVM compilation tools
- See main mod README.md for compilation instructions

