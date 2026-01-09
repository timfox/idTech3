# Wayland Support Completion

This document outlines the remaining work needed to complete native Wayland support in the id Tech 3 engine.

## Current Status

### ✅ Implemented Features

1. **Basic Wayland Backend**
   - SDL Wayland driver integration
   - Automatic Wayland detection via `WAYLAND_DISPLAY`
   - Fallback to X11 when Wayland fails

2. **Window Management**
   - Server-side decorations (libdecor disabled)
   - Window class setting (`SDL_VIDEO_WAYLAND_WMCLASS`)
   - Basic fullscreen/minimize support

3. **Vulkan Surface Creation**
   - Wayland-specific Vulkan surface handling
   - Automatic fallback for Vulkan surface creation failures

4. **Configuration Options**
   - `r_wayland` CVAR for forcing Wayland/X11
   - `SDL_VIDEODRIVER` environment variable support

## 🔄 Remaining Work

### High Priority

#### 1. Input Handling Improvements
- **Touch/Tablet Support**: Implement touch event handling for touchscreens
- **Gesture Recognition**: Add gesture support for multi-touch operations
- **Tablet Pressure Sensitivity**: Support for graphics tablets

#### 2. Clipboard Integration
- **Wayland Clipboard**: Implement native Wayland clipboard instead of X11 fallback
- **Primary Selection**: Support for primary selection (middle-click paste)
- **Rich Content**: Support for images and formatted text in clipboard

#### 3. Window State Management
- **Maximize/Restore**: Proper handling of maximize/restore window states
- **Window Positioning**: Support for window positioning hints
- **Window Grouping**: Support for window grouping/transient windows

### Medium Priority

#### 4. Display Configuration
- **Multi-Monitor**: Proper multi-monitor support and configuration
- **HiDPI Scaling**: Better support for high-DPI displays
- **Display Rotation**: Support for rotated displays

#### 5. Performance Optimizations
- **Direct Rendering**: Remove X11 dependencies for pure Wayland builds
- **Buffer Management**: Optimize buffer allocation for Wayland
- **Synchronization**: Improve frame synchronization with Wayland compositor

#### 6. Accessibility
- **Screen Reader**: Support for screen readers and accessibility tools
- **Keyboard Navigation**: Enhanced keyboard navigation support
- **High Contrast**: Support for high contrast themes

### Low Priority

#### 7. Advanced Features
- **Layer Shell**: Support for Wayland layer shell protocol
- **DRM Leasing**: Support for direct DRM access
- **PipeWire Integration**: Audio/video over Wayland

#### 8. Compositor Compatibility
- **KDE/Plasma**: Enhanced support for KDE Plasma
- **GNOME**: Enhanced support for GNOME Shell
- **Sway/WLroots**: Support for wlroots-based compositors
- **Weston**: Enhanced support for Weston reference compositor

## Implementation Plan

### Phase 1: Core Input and Clipboard (Week 1-2)

1. **Touch Support Implementation**
   ```c
   // Add touch event handling to SDL input layer
   case SDL_FINGERDOWN:
   case SDL_FINGERUP:
   case SDL_FINGERMOTION:
       // Convert to mouse events or direct touch API
   ```

2. **Native Clipboard Implementation**
   ```c
   // Implement Wayland data device manager
   // Replace X11 clipboard with native Wayland clipboard
   wl_data_device_manager *data_device_manager;
   wl_data_device *data_device;
   ```

### Phase 2: Window Management (Week 3-4)

1. **Enhanced Window States**
   ```c
   // Implement xdg-toplevel state changes
   case XDG_TOPLEVEL_STATE_MAXIMIZED:
   case XDG_TOPLEVEL_STATE_FULLSCREEN:
       // Handle state changes appropriately
   ```

2. **Window Positioning**
   ```c
   // Support for window positioning constraints
   xdg_positioner *positioner;
   xdg_surface *xdg_surface;
   ```

### Phase 3: Display and Performance (Week 5-6)

1. **Multi-Monitor Support**
   ```c
   // Enumerate outputs and handle configuration changes
   wl_output *output;
   wl_output_listener output_listener;
   ```

2. **Performance Optimizations**
   ```c
   // Optimize buffer allocation and presentation
   wp_presentation *presentation;
   wp_presentation_feedback *feedback;
   ```

## Testing Strategy

### Unit Tests
- **Input Testing**: Verify touch, gesture, and keyboard input
- **Clipboard Testing**: Test text, image, and rich content clipboard operations
- **Window Testing**: Test maximize, minimize, restore, and positioning

### Integration Tests
- **Compositor Compatibility**: Test with multiple Wayland compositors
- **Fallback Testing**: Ensure X11 fallback works when Wayland fails
- **Performance Testing**: Compare performance with X11

### CI/CD Integration
```yaml
# Add to GitHub Actions
- name: Test Wayland
  run: |
    export WAYLAND_DISPLAY=:0
    ./idtech3.x86_64 +set r_wayland 1 +quit
```

## Configuration

### CVars to Add

```c
// Wayland-specific configuration
r_wayland_touch "1"              // Enable touch input
r_wayland_clipboard "1"          // Use native clipboard
r_wayland_decorations "0"        // Use server-side decorations
r_wayland_scaling "1.0"          // HiDPI scaling factor
```

### Environment Variables

```bash
# Force Wayland
export SDL_VIDEODRIVER=wayland

# Force X11 fallback
export SDL_VIDEODRIVER=x11

# Debug Wayland protocol
export WAYLAND_DEBUG=1
```

## Compatibility Matrix

| Feature | Weston | Mutter | KWin | Sway | Status |
|---------|--------|--------|------|------|--------|
| Basic Windows | ✅ | ✅ | ✅ | ✅ | Complete |
| Vulkan | ✅ | ✅ | ✅ | ✅ | Complete |
| Touch Input | ❌ | ❌ | ❌ | ❌ | TODO |
| Native Clipboard | ❌ | ❌ | ❌ | ❌ | TODO |
| Window States | ⚠️ | ⚠️ | ⚠️ | ⚠️ | Partial |
| Multi-Monitor | ⚠️ | ⚠️ | ⚠️ | ⚠️ | Partial |

## Debugging Tools

### Wayland Debug Output
```bash
# Enable protocol debugging
WAYLAND_DEBUG=1 ./idtech3.x86_64

# Log Wayland protocol messages
WAYLAND_DEBUG=client ./idtech3.x86_64
```

### SDL Debug Information
```bash
# SDL debugging
SDL_DEBUG=1 ./idtech3.x86_64

# Video driver information
SDL_VIDEODRIVER=wayland SDL_DEBUG=1 ./idtech3.x86_64
```

### Vulkan Validation
```bash
# Vulkan validation layers
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./idtech3.x86_64
```

## Dependencies

### Required Libraries
- `libwayland-client`
- `libwayland-cursor`
- `libxkbcommon`
- `libdecor` (optional, currently disabled)

### Build System Updates
```cmake
# Add Wayland detection
find_package(Wayland REQUIRED)
find_package(XKBCommon REQUIRED)

# Conditional compilation
if(WAYLAND_FOUND)
    add_definitions(-DUSE_WAYLAND)
endif()
```

## Risk Assessment

### High Risk
- **Input Compatibility**: Touch/gesture input may break existing mouse/keyboard controls
- **Clipboard Integration**: Complex protocol may introduce security issues

### Medium Risk
- **Performance**: Wayland-specific optimizations may impact X11 performance
- **Compatibility**: Compositor-specific features may not work across all environments

### Low Risk
- **Window Management**: Additional window states are additive features
- **Display Configuration**: HiDPI and multi-monitor are enhancements

## Success Criteria

1. **Functional Completeness**: All planned features implemented and working
2. **Performance**: No performance regression compared to X11
3. **Compatibility**: Works with major Wayland compositors (GNOME, KDE, Sway)
4. **Fallback**: Robust fallback to X11 when Wayland fails
5. **Security**: No security vulnerabilities introduced

## Timeline

- **Phase 1**: 2 weeks - Input and clipboard
- **Phase 2**: 2 weeks - Window management
- **Phase 3**: 2 weeks - Display and performance
- **Testing**: 1 week - Integration and compatibility testing
- **Total**: 7 weeks for complete Wayland support

## References

- [Wayland Protocol Documentation](https://wayland.freedesktop.org/docs/html/)
- [SDL Wayland Backend](https://wiki.libsdl.org/SDL_VideoDriver)
- [XDG Shell Protocol](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/tree/main/stable/xdg-shell)