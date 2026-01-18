# Qt Integration for id Tech 3

This document describes the comprehensive Qt integration system that provides modern UI development capabilities for id Tech 3.

## Overview

The Qt integration brings modern UI development to id Tech 3 by providing:

- **Modern Qt-based User Interface**: Professional-looking menus, dialogs, and controls
- **Asset Management Tools**: Browser and editor for game assets
- **Performance Profiling**: Real-time performance monitoring and analysis
- **Enhanced Console**: Syntax-highlighted console with command completion
- **Mod Management**: User-friendly mod installation and management
- **Cross-platform Compatibility**: Native look and feel on all platforms

## Architecture

### Core Components

#### 1. IdTech3Application (`src/qt/idtech3_application.h/cpp`)
Main Qt application wrapper that integrates with the id Tech 3 engine:

- **Application Lifecycle**: Manages Qt application initialization and shutdown
- **Window Management**: Handles main window, fullscreen, and UI modes
- **Game Integration**: Provides seamless switching between Qt UI and game
- **Event Processing**: Processes Qt events alongside game events

#### 2. GameRenderer (`src/qt/idtech3_application.cpp`)
OpenGL widget that hosts the game engine:

- **OpenGL Integration**: Proper OpenGL context sharing with game engine
- **Input Handling**: Forwards mouse/keyboard events to game
- **Performance Control**: Target FPS and rendering control
- **Fullscreen Support**: Seamless fullscreen/windowed mode switching

#### 3. UI Components

##### ConsoleWidget (`src/qt/console_widget.h/cpp`)
Advanced console interface:

- **Syntax Highlighting**: Color-coded output for errors, warnings, info
- **Command History**: Navigate through previous commands
- **Auto-completion**: Intelligent command and argument completion
- **Search/Filter**: Filter console output by type

##### MenuWidget (`src/qt/menu_widget.h/cpp`)
Main menu system:

- **Page-based Navigation**: Modular menu pages (Main, Single Player, Multiplayer, etc.)
- **Game Launcher**: Integrated game launching with mod/map selection
- **Settings Management**: Comprehensive video, audio, input settings
- **Mod Browser**: Browse and manage installed mods

##### AssetBrowser (`src/qt/asset_browser.h/cpp`)
Asset management interface:

- **File System Browser**: Navigate game directories and assets
- **Asset Preview**: Live preview of textures, models, sounds
- **Property Editor**: Edit asset metadata and properties
- **Import/Export**: Drag-and-drop asset import/export

##### ProfilerWidget (`src/qt/profiler_widget.h/cpp`)
Performance monitoring:

- **Real-time Graphs**: FPS, frame time, memory, CPU/GPU usage
- **Performance Stats**: Min/max/average statistics
- **Data Export**: Save profiling data for analysis
- **GPU Monitoring**: Hardware-accelerated performance tracking

### Integration Points

#### Engine Integration (`src/qt/qt_main.cpp`)
C API functions that bridge Qt and the game engine:

```c
// Initialize Qt system
void Sys_InitQt(int argc, char* argv[]);

// Process Qt events in game loop
void Sys_ProcessQtEvents();

// UI mode switching
void Sys_ShowQtUI(qboolean show);

// Console integration
void Sys_QtConsolePrint(const char* text);

// Performance monitoring
void Sys_QtRecordPerformanceSample(float fps, float frameTime, ...);
```

#### Build System Integration
CMake configuration for Qt:

```cmake
OPTION(USE_QT "Enable Qt integration for modern UI" OFF)

IF(USE_QT)
    FIND_PACKAGE(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL Network Charts)
    TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} Qt6::Core Qt6::Widgets ...)
    TARGET_COMPILE_DEFINITIONS(${CMAKE_PROJECT_NAME} PRIVATE QT_ENABLED)
ENDIF()
```

## Features

### User Interface

#### Modern Design
- **Dark Theme**: Professional dark color scheme
- **High DPI Support**: Proper scaling on high-resolution displays
- **Responsive Layout**: Adapts to different window sizes
- **Smooth Animations**: Polished transitions and effects

#### Accessibility
- **Keyboard Navigation**: Full keyboard support
- **Screen Reader Support**: Proper accessibility labels
- **Customizable Fonts**: User-selectable fonts and sizes
- **Color Schemes**: Multiple theme options

### Asset Management

#### Asset Browser
- **Directory Navigation**: Tree view of game assets
- **Type Filtering**: Filter by asset type (texture, model, sound, etc.)
- **Search Functionality**: Find assets by name or path
- **Bulk Operations**: Select and operate on multiple assets

#### Asset Preview
- **Live Preview**: Real-time asset visualization
- **Property Editing**: Modify asset properties
- **Metadata Display**: Show asset information and dependencies
- **Format Support**: Handles all id Tech 3 asset formats

### Performance Monitoring

#### Real-time Profiling
- **Frame Rate Analysis**: FPS monitoring with statistics
- **Memory Tracking**: RAM usage and leak detection
- **CPU/GPU Monitoring**: Hardware utilization tracking
- **Custom Metrics**: Engine-specific performance data

#### Data Visualization
- **Interactive Graphs**: Zoomable, pannable performance graphs
- **Export Capabilities**: Save data for external analysis
- **Historical Data**: Maintain performance history
- **Threshold Alerts**: Visual indicators for performance issues

### Console System

#### Enhanced Console
- **Syntax Highlighting**: Color-coded output
- **Command Completion**: Intelligent auto-completion
- **History Navigation**: Arrow key navigation through history
- **Output Filtering**: Filter by message type

#### Developer Tools
- **Command Reference**: Built-in help system
- **Variable Inspection**: View and modify cvars
- **Script Execution**: Run console scripts
- **Log Management**: Save and manage console logs

## Usage

### Building with Qt

Enable Qt integration in CMake:

```bash
cmake -DUSE_QT=ON ..
make
```

### Running with Qt UI

Launch with Qt interface:

```bash
./idtech3 +set ui_qt 1
```

Or use the standalone Qt launcher:

```bash
./qt_launcher
```

### Qt Console Commands

```
ui_qt_show_menu      - Show main menu
ui_qt_show_console   - Show console
ui_qt_show_browser   - Show asset browser
ui_qt_show_profiler  - Show profiler
ui_qt_toggle_fullscreen - Toggle fullscreen
```

### Configuration

Qt settings are stored in:

- **Linux/macOS**: `~/.config/id Software/idTech3 Qt.conf`
- **Windows**: `HKEY_CURRENT_USER\Software\id Software\idTech3 Qt`

## API Reference

### Application Control

```cpp
// Get Qt application instance
IdTech3Application* app = Sys_GetQtApplication();

// Show/hide UI components
app->showConsole();
app->showAssetBrowser();
app->showProfiler();

// Game control
app->startGame();
app->pauseGame();
app->setUIMode(true); // Switch to game mode
```

### Performance Monitoring

```cpp
// Record performance sample
PerformanceSample sample;
sample.fps = 60.0f;
sample.frameTime = 16.67f;
sample.memoryUsed = 256 * 1024 * 1024; // 256MB

PerformanceMonitor::instance()->recordSample(sample);
```

### Asset Management

```cpp
// Browse assets
AssetBrowser* browser = app->assetBrowser();
browser->navigateToPath("/baseq3/textures");
browser->setFilterType(AssetType::Texture);

// Get selected assets
QList<AssetInfo> selected = browser->selectedAssets();
```

## Platform Support

### Windows
- **Qt Version**: Qt 6.4+
- **Compiler**: MSVC 2019+
- **Deployment**: Windows Installer or portable

### Linux
- **Qt Version**: Qt 6.4+
- **Compiler**: GCC 9+
- **Deployment**: AppImage or system packages

### macOS
- **Qt Version**: Qt 6.4+
- **Compiler**: Clang 12+
- **Deployment**: DMG installer

## Dependencies

### Required
- **Qt 6.4+**: Core, Widgets, OpenGL, Network, Charts
- **CMake 3.16+**: Build system configuration
- **C++17**: Compiler standard support

### Optional
- **Qt Creator**: IDE for Qt development
- **Qt Designer**: UI design tool
- **Qt Linguist**: Translation management

## Development

### Adding New UI Components

1. Create header and implementation files in `src/qt/`
2. Inherit from appropriate Qt base class
3. Register with `IdTech3Application`
4. Add to CMake build system

### Extending Asset Support

1. Add new `AssetType` enum values
2. Implement preview logic in `AssetPreviewWidget`
3. Add property editors in `AssetPropertyEditor`
4. Update file type detection in `AssetBrowser::detectAssetType()`

### Performance Monitoring

1. Define new performance metrics
2. Update `PerformanceSample` structure
3. Add graphing in `ProfilerWidget`
4. Record samples in engine integration points

## Troubleshooting

### Common Issues

#### Qt Library Not Found
```
Error: Qt6 not found
Solution: Install Qt6 development packages or set CMAKE_PREFIX_PATH
```

#### OpenGL Context Issues
```
Error: OpenGL context creation failed
Solution: Update graphics drivers or disable advanced OpenGL features
```

#### Asset Loading Problems
```
Error: Asset preview failed
Solution: Check file permissions and Qt plugin installation
```

### Debug Mode

Enable debug logging:

```cpp
qputenv("QT_LOGGING_RULES", "qt.*=true");
qputenv("QML_LOGGING", "true");
```

### Performance Optimization

- Use `QTimer` for periodic updates instead of tight loops
- Implement lazy loading for asset previews
- Cache frequently accessed data
- Use Qt's signal/slot connections efficiently

## Future Enhancements

### Planned Features
- **Qt Quick Integration**: QML-based UI components
- **Network Browser**: Online server browser
- **Workshop Integration**: Steam Workshop-style mod management
- **Video Playback**: Integrated cinematics player
- **Screenshot Manager**: Automated screenshot capture and management
- **Localization**: Multi-language support with Qt Linguist

### Community Contributions
- **Custom Themes**: User-created UI themes
- **Plugin System**: Extensible UI plugin architecture
- **Third-party Tools**: Integration with external asset tools
- **Mobile Support**: Qt-based mobile companion app

## License

This Qt integration is part of the id Tech 3 Enhanced Edition and follows the same licensing terms as the original id Tech 3 engine.

## Credits

- **Qt Project**: Cross-platform application framework
- **id Software**: Original id Tech 3 engine
- **Community Contributors**: UI design and implementation
- **Open Source Libraries**: Various Qt-based components

---

For more information, see the individual component documentation or visit the project repository.