# ImGui Debug Overlays

## Overview

The id Tech 3 engine now includes comprehensive ImGui-based debug overlays for real-time debugging and profiling. These overlays provide detailed information about performance, memory, networking, rendering, and more.

## Features

### Performance Overlay

Displays real-time performance metrics:
- **FPS**: Current frames per second
- **Frame Time**: Time per frame in milliseconds
- **Frame Time History**: Graph showing frame time over the last 120 frames
- **FPS History**: Graph showing FPS over the last 120 frames
- **Client State**: Current connection state
- **Server/Client Time**: Time synchronization information

**CVar**: `cl_imgui_debug_performance`

### Memory Overlay

Shows memory usage statistics (requires `ENABLE_MEMORY_TRACKING`):
- **Total Memory**: Allocated, freed, current, and peak usage
- **Memory by Type**: Breakdown by memory type (HUNK, ZONE, TEMP, SOUND, RENDERER, etc.)
- **Leak Detection**: Number of detected memory leaks
- **Leak Reporting**: Button to generate detailed leak reports

**CVar**: `cl_imgui_debug_memory`

### Network Overlay

Displays network statistics:
- **Bytes Sent/Received**: Total network traffic
- **Request Statistics**: Total, successful, and failed requests
- **Success Rate**: Percentage of successful requests
- **Response Times**: Average and last response time
- **Protocol Usage**: HTTP/2 vs HTTP/1.1, IPv6 vs IPv4
- **Connection Info**: Server address, ping, packet loss

**CVar**: `cl_imgui_debug_network`

### Renderer Overlay

Shows renderer information:
- **Renderer Info**: Renderer name, vendor, version, extensions
- **Display Settings**: Resolution, color/depth/stencil bits
- **Performance Counters**: Renderer performance metrics (when `r_speeds` is enabled)

**CVar**: `cl_imgui_debug_renderer`

### CVar Browser

Interactive console variable browser:
- **Filter**: Search/filter CVars by name
- **CVar List**: Scrollable list of all CVars
- **CVar Details**: View name, value, type, flags, description
- **Edit Values**: Change CVar values directly
- **Reset**: Reset CVars to default values
- **Flags Display**: Shows CVar flags (Archive, UserInfo, ServerInfo, ROM, Init, Latch)

**CVar**: `cl_imgui_debug_cvars`

### Console Overlay

Console output viewer:
- **Filter**: Filter console output
- **Auto-scroll**: Automatically scroll to latest output
- **Command Input**: Execute console commands
- **Scrollable History**: View console history

**CVar**: `cl_imgui_debug_console`

### Main Menu

Debug menu bar with quick access to all overlays:
- **Debug Menu**: Toggle individual overlays
- **Help Menu**: Access help and information

**CVar**: `cl_imgui_debug_mainmenu`

## Usage

### Enabling ImGui

First, enable ImGui support:
```
/set cl_imgui 1
```

### Opening Debug Overlays

**Via Main Menu:**
- The debug menu bar appears at the top when `cl_imgui_debug_mainmenu` is enabled (default: 1)
- Click "Debug" → Select overlay to toggle

**Via CVars:**
```
/set cl_imgui_debug_performance 1
/set cl_imgui_debug_memory 1
/set cl_imgui_debug_network 1
/set cl_imgui_debug_renderer 1
/set cl_imgui_debug_cvars 1
/set cl_imgui_debug_console 1
```

### Closing Overlays

- Click the "X" button on each overlay window, or
- Toggle the CVar off: `/set cl_imgui_debug_performance 0`

## Keyboard Shortcuts

- **F12**: Toggle ImGui (if configured)
- **Mouse**: Click and drag to move windows
- **Enter**: Execute commands in console overlay

## Performance Impact

- **Minimal**: Overlays are lightweight and only render when visible
- **Frame Time**: Typically adds < 0.1ms per visible overlay
- **Memory**: Negligible memory overhead

## Tips

1. **Performance Monitoring**: Keep the performance overlay visible during development to catch frame time spikes
2. **Memory Debugging**: Enable memory tracking (`ENABLE_MEMORY_TRACKING=ON`) and use the memory overlay to find leaks
3. **Network Debugging**: Use the network overlay to monitor connection quality and protocol usage
4. **CVar Tweaking**: Use the CVar browser to quickly find and modify settings without typing commands
5. **Console History**: Use the console overlay to review past output and filter for specific messages

## Integration

The debug overlays integrate seamlessly with:
- **Memory Tracking System**: Shows memory statistics when enabled
- **Enhanced Networking**: Displays network statistics from enhanced networking features
- **Structured Logging**: Can display log output in console overlay (future enhancement)
- **Renderer**: Shows renderer statistics and performance counters

## Customization

All overlays can be customized via CVars:
- Window positions are remembered
- Window sizes can be adjusted
- Overlays can be enabled/disabled individually
- Main menu can be hidden if desired

## Future Enhancements

Planned improvements:
- **Console Integration**: Full console output capture and display
- **Log Viewer**: View structured logs with filtering
- **Profiler Integration**: Display Tracy profiler data
- **Entity Browser**: Browse and inspect game entities
- **Shader Debugger**: Debug shader compilation and execution
- **Asset Browser**: Browse loaded assets (models, textures, sounds)

## Troubleshooting

**Overlays not showing:**
- Ensure `cl_imgui` is enabled: `/set cl_imgui 1`
- Check that ImGui backend is initialized (renderer support required)
- Verify CVars are set correctly

**Performance issues:**
- Disable unused overlays
- Reduce frame history size if needed
- Check renderer performance counters

**Memory overlay empty:**
- Build with `ENABLE_MEMORY_TRACKING=ON`
- Ensure memory tracking is initialized

**Network overlay empty:**
- Ensure enhanced networking is enabled
- Check that network statistics are being collected

