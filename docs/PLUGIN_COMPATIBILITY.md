# GTK Radiant Plugin Compatibility for Qt Radiant

## Overview

Qt Radiant aims to maintain compatibility with GTK Radiant plugins to allow users to use existing plugins without modification. This document describes the compatibility layer and implementation status.

## GTK Radiant Plugin System

GTK Radiant uses the **Synapse** plugin system:

- Plugins are shared libraries (`.so` on Linux, `.dll` on Windows)
- Plugins export `Synapse_EnumerateInterfaces()` function
- Plugins implement function tables (e.g., `_QERPluginTable`, `_QERFuncTable_1`)
- Plugins are loaded from `plugins/` and `modules/` directories
- Plugins can provide multiple APIs (IMAGE_MAJOR, PLUGIN_MAJOR, TOOLBAR_MAJOR, etc.)

## Plugin Loading Locations

GTK Radiant loads plugins from:
1. `<appPath>/plugins/` - Application plugins directory
2. `<gameToolsPath>/plugins/` - Game-specific plugins directory
3. `<appPath>/modules/` - Application modules directory (if different from plugins)
4. `<gameToolsPath>/modules/` - Game-specific modules directory

## Implementation Status

### ✅ Completed
- Plugin loader framework (`radiant/qt/plugin_loader.h`)
- Basic plugin discovery and loading structure

### 🚧 In Progress
- Synapse API compatibility layer
- GTK-to-Qt function call bridging

### ⏳ Pending
- Full Synapse server/client implementation
- GTK widget API bridging (for plugins that create UI)
- OpenGL context bridging (for plugins that render)
- File system API compatibility
- Entity/brush API compatibility

## Plugin API Compatibility

### Core APIs Required

1. **RADIANT_MAJOR** - Core editor functionality
   - Brush creation/manipulation
   - Selection management
   - Texture operations
   - Map operations

2. **PLUGIN_MAJOR** - Plugin menu commands
   - `QERPlug_Init()`
   - `QERPlug_GetName()`
   - `QERPlug_GetCommandList()`
   - `QERPlug_Dispatch()`

3. **IMAGE_MAJOR** - Image format support
   - Texture loading
   - Image format handlers

4. **QGL_MAJOR** - OpenGL operations
   - OpenGL function table
   - Context management

5. **UI_MAJOR / UIGTK_MAJOR** - User interface
   - Window creation
   - Widget management
   - **Note**: GTK-specific APIs need Qt equivalents

### GTK-to-Qt Bridging

Many plugins use GTK-specific APIs that need Qt equivalents:

| GTK API | Qt Equivalent | Status |
|---------|---------------|--------|
| `gtk_MessageBox` | `QMessageBox` | ✅ Easy |
| `file_dialog` | `QFileDialog` | ✅ Easy |
| `color_dialog` | `QColorDialog` | ✅ Easy |
| `gtk_glwidget_*` | `QOpenGLWidget` | 🚧 Needs work |
| `GtkWidget*` | `QWidget*` | 🚧 Needs adapter |
| `g_pMainWidget` | `QMainWindow*` | ✅ Easy |

## Usage

### Loading Plugins

```cpp
#include "plugin_loader.h"

PluginLoader loader;
int count = loader.loadPlugins(appPath, gameToolsPath);
qDebug() << "Loaded" << count << "plugins";
```

### Accessing Plugin Commands

```cpp
QStringList plugins = loader.loadedPlugins();
for (const QString& name : plugins) {
    QString commands = loader.getPluginCommands(name);
    // Parse commands and add to menu
}
```

### Dispatching Commands

```cpp
loader.dispatchCommand("plugin_name", "CommandName");
```

## Implementation Notes

### Synapse Compatibility

The Synapse plugin system is complex and requires:
- Full Synapse server implementation (or reuse from `radiant/libs/synapse`)
- API descriptor management
- Function table population
- Dependency resolution

**Option 1**: Reuse existing Synapse code from `radiant/libs/synapse/`
- Pros: Full compatibility, less code to maintain
- Cons: Depends on GLib, libxml, GTK dependencies

**Option 2**: Implement minimal Synapse compatibility layer
- Pros: Qt-native, no external dependencies
- Cons: May not support all plugins, more work

### Recommended Approach

1. **Phase 1**: Implement basic plugin loading
   - Load `.so`/`.dll` files
   - Find `Synapse_EnumerateInterfaces` symbol
   - Basic function table population

2. **Phase 2**: Implement core API bridges
   - RADIANT_MAJOR (brush/texture/map operations)
   - PLUGIN_MAJOR (menu commands)
   - Basic UI dialogs (message box, file dialog)

3. **Phase 3**: Advanced compatibility
   - OpenGL context bridging
   - GTK widget to Qt widget adapters
   - Full Synapse server (if needed)

## Testing

To test plugin compatibility:

1. Place a GTK Radiant plugin in `plugins/` directory
2. Start Qt Radiant
3. Check console for plugin load messages
4. Verify plugin commands appear in menu
5. Test plugin functionality

## Known Limitations

- **GTK Widget APIs**: Plugins that create GTK widgets directly won't work without adapters
- **OpenGL Context**: Plugins that manage their own OpenGL contexts may have issues
- **Threading**: Some plugins may assume GTK's threading model
- **File System**: VFS (Virtual File System) plugins may need special handling

## Future Enhancements

- Plugin manager UI (enable/disable plugins)
- Plugin dependency resolution
- Plugin API versioning
- Hot-reload plugins (for development)
- Plugin sandboxing (security)
