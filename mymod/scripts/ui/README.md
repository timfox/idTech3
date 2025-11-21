# UI Module (User Interface)

This directory contains the user interface source code.

## Overview

The UI module handles:
- Menu system
- HUD elements
- User interface rendering
- Menu navigation
- Settings interface
- Mod-specific UI elements

## Compilation

### Native Compilation (Recommended)

To compile as a native shared library:

1. Place your C source files in this directory
2. Build using CMake or Makefile from parent directory:
   ```bash
   cd ..
   make ui
   ```
3. Output: `../vm/ui.x86_64.so` (or `.dll` on Windows)
4. Enable: `set vm_ui 0`

### QVM Compilation (Legacy)

To compile as QVM bytecode:

1. Place your C source files in this directory
2. Use a Quake III compiler (q3asm or similar)
3. Compile to `../vm/ui.qvm`
4. The compiled QVM file will be loaded automatically

## File Structure

Typical UI module files:
- `ui_main.c` - Main UI initialization
- `ui_menu.c` - Menu system
- `ui_atoms.c` - UI drawing primitives
- `ui_shared.c` - Shared UI functions
- `ui_gameinfo.c` - Game information display

## API Reference

The UI module communicates with the engine through system calls defined in `ui_public.h`.

## Customization

You can customize:
- Menu layouts and appearance
- HUD elements
- Settings menus
- Mod-specific UI features

## Notes

- UI code runs independently from game logic
- Changes here affect menu and HUD appearance
- Requires QVM compilation tools
- See main mod README.md for compilation instructions

