# GTK Radiant Plugin Compatibility - Implementation Status

## Overview

This document tracks the implementation status of GTK Radiant plugin compatibility for Qt Radiant. The goal is to allow existing GTK Radiant plugins to work with Qt Radiant without modification.

## Implementation Summary

### ✅ Completed Components

1. **Plugin Loader Framework** (`radiant/qt/plugin_loader.h/cpp`)
   - Discovers plugins in `plugins/` and `modules/` directories
   - Loads `.so`/`.dll` files using QLibrary
   - Detects Synapse and legacy plugin formats
   - Tracks loaded plugins and provides access methods

2. **Synapse Compatibility Layer** (`radiant/qt/synapse_compat.h/cpp`)
   - Minimal Synapse server implementation
   - API descriptor management
   - Client enumeration and tracking
   - Search path management
   - **Status**: Basic structure in place, full resolution logic pending

3. **GTK-to-Qt Bridge** (`radiant/qt/gtk_qt_bridge.h/cpp`)
   - `messageBox()` → `QMessageBox`
   - `file_dialog()` → `QFileDialog`
   - `color_dialog()` → `QColorDialog`
   - `dir_dialog()` → `QFileDialog::getExistingDirectory()`
   - `load_plugin_bitmap()` → `QPixmap::load()`
   - Main widget pointer management
   - Vector conversion utilities

4. **Integration**
   - Plugin loader integrated into main window
   - Automatic plugin loading on startup
   - Console logging for plugin load events
   - Main widget set for plugin access

### 🚧 Partially Implemented

1. **Synapse API Resolution**
   - Basic structure exists
   - API matching logic needs completion
   - Function table population needs implementation
   - Dependency resolution needs work

2. **Plugin Initialization**
   - Basic initialization exists
   - Full Synapse API population pending
   - Function table filling needs completion

### ⏳ Pending Features

1. **Full Synapse Server Implementation**
   - Complete API resolution algorithm
   - Dependency graph resolution
   - Function table population from providers
   - XML config file parsing (if needed)

2. **Core API Bridges**
   - **RADIANT_MAJOR**: Brush/texture/map operations
     - Need to implement `_QERFuncTable_1` with Qt equivalents
     - Bridge brush creation/manipulation
     - Bridge selection management
     - Bridge texture operations
   - **QGL_MAJOR**: OpenGL operations
     - Bridge OpenGL function calls
     - Context management
     - GL widget creation
   - **UI_MAJOR / UIGTK_MAJOR**: User interface
     - Window/widget creation adapters
     - GTK widget to Qt widget conversion
   - **IMAGE_MAJOR**: Image format support
     - Texture loading bridges
   - **VFS_MAJOR**: Virtual file system
     - File system operation bridges

3. **Plugin Menu Integration**
   - Parse plugin command lists
   - Add plugin commands to menu
   - Handle command dispatching

4. **Advanced Features**
   - Plugin enable/disable
   - Plugin dependency resolution
   - Plugin API versioning
   - Hot-reload (development)
   - Plugin sandboxing (security)

## Architecture

```
┌─────────────────────────────────────────┐
│         Qt Radiant Main Window          │
│  ┌───────────────────────────────────┐ │
│  │      PluginLoader                  │ │
│  │  - Discovers plugins               │ │
│  │  - Loads .so/.dll files            │ │
│  │  - Manages plugin lifecycle        │ │
│  └──────────┬──────────────────────────┘ │
│             │                            │
│  ┌──────────▼──────────────────────────┐ │
│  │  SynapseCompatibilityLayer          │ │
│  │  - API descriptor management        │ │
│  │  - Client enumeration               │ │
│  │  - API resolution (stub)            │ │
│  └──────────┬──────────────────────────┘ │
│             │                            │
│  ┌──────────▼──────────────────────────┐ │
│  │      GtkQtBridge                    │ │
│  │  - GTK → Qt function bridges        │ │
│  │  - Dialog conversions               │ │
│  │  - Widget adapters                  │ │
│  └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
             │
             ▼
    ┌─────────────────┐
    │ GTK Radiant    │
    │ Plugins (.so)  │
    └─────────────────┘
```

## Current Limitations

1. **API Resolution**: The Synapse API resolution is stubbed. Plugins that require specific APIs may not fully initialize.

2. **Function Tables**: Core function tables (RADIANT_MAJOR, QGL_MAJOR, etc.) are not yet populated with Qt implementations.

3. **GTK Widget APIs**: Plugins that create GTK widgets directly won't work without widget adapters.

4. **OpenGL Context**: Plugins managing their own OpenGL contexts may have issues.

5. **Threading**: Some plugins may assume GTK's threading model.

## Testing

To test plugin compatibility:

1. Place a GTK Radiant plugin in `plugins/` directory
2. Start Qt Radiant
3. Check console for:
   - "PluginLoader: Found Synapse plugin: ..."
   - "PluginLoader: Successfully resolved Synapse plugin: ..."
   - Or error messages if plugin fails to load

## Next Steps

1. **Complete Synapse Resolution**
   - Implement API matching algorithm
   - Populate function tables from providers
   - Handle dependency chains

2. **Implement Core API Bridges**
   - Start with RADIANT_MAJOR (most commonly used)
   - Add QGL_MAJOR for OpenGL plugins
   - Add UI bridges for plugins with dialogs

3. **Plugin Menu Integration**
   - Parse command lists from plugins
   - Add to appropriate menus
   - Handle command dispatching

4. **Testing & Validation**
   - Test with real GTK Radiant plugins
   - Identify missing APIs
   - Add bridges as needed

## Files Created/Modified

### New Files
- `radiant/qt/plugin_loader.h` - Plugin loader interface
- `radiant/qt/plugin_loader.cpp` - Plugin loader implementation
- `radiant/qt/synapse_compat.h` - Synapse compatibility layer interface
- `radiant/qt/synapse_compat.cpp` - Synapse compatibility layer implementation
- `radiant/qt/gtk_qt_bridge.h` - GTK-to-Qt bridge interface
- `radiant/qt/gtk_qt_bridge.cpp` - GTK-to-Qt bridge implementation
- `docs/PLUGIN_COMPATIBILITY.md` - Plugin compatibility documentation
- `docs/PLUGIN_IMPLEMENTATION_STATUS.md` - This file

### Modified Files
- `radiant/qt/main_window.h` - Added PluginLoader member
- `radiant/qt/main_window.cpp` - Integrated plugin loading
- `radiant/qt/path_expander.cpp` - Fixed compilation errors (QPair usage)
- `radiant/qt/path_expander.h` - Added QPair include
- `radiant/CMakeLists.txt` - Added new source files

## Compilation Status

✅ All compilation errors fixed:
- Fixed `signals:` → `Q_SIGNALS`
- Fixed `emit` → `Q_EMIT`
- Fixed path expander QPair usage
- Fixed missing variable declarations
- Fixed include issues

The code should now compile successfully.
