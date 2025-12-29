# Engine Startup Prerequisites and Troubleshooting

## System Requirements

### Minimum Requirements
- **CPU**: 2+ cores recommended (single core supported but suboptimal)
- **RAM**: 512 MB minimum, 2 GB recommended
- **Storage**: 500 MB free space
- **OS**: Linux (Ubuntu 18.04+, CentOS 7+, etc.)

### Graphics Requirements
- **Vulkan**: Vulkan 1.1+ capable GPU (recommended)
- **OpenGL**: OpenGL 3.3+ capable GPU (fallback)
- **Drivers**: Up-to-date graphics drivers

### Software Dependencies
- **Vulkan SDK**: For Vulkan renderer development
- **SDL2**: For windowing and input
- **OpenAL**: For audio (optional)
- **Freetype**: For font rendering (optional)

## Startup Process

### Phase 1: Core Initialization
1. **Command Line Parsing**: Process startup parameters
2. **Filesystem Setup**: Initialize virtual filesystem
3. **CVAR System**: Load configuration variables
4. **Memory Management**: Initialize zone and hunk allocators

### Phase 2: Subsystem Initialization
1. **Network**: Initialize networking (unless dedicated server)
2. **Console**: Set up command console and history
3. **Renderer**: Load and initialize graphics renderer
4. **Client**: Initialize client subsystems (if not dedicated)

### Phase 3: Content Loading
1. **Base Assets**: Load core game assets
2. **Mod Detection**: Scan for installed modifications
3. **Shader Compilation**: Compile required shaders
4. **Map Loading**: Load initial map/level

## Renderer Selection and Fallback

### Priority Order
1. **Vulkan** (default) - Modern, high-performance
2. **OpenGL2** - Modern OpenGL with shaders
3. **OpenGL** (legacy) - Deprecated immediate mode

### Automatic Fallback
The engine automatically tries renderers in priority order:

```bash
# Default behavior (Vulkan → OpenGL2 → OpenGL)
./idtech3.x86_64

# Force specific renderer
./idtech3.x86_64 +set cl_renderer vulkan
./idtech3.x86_64 +set cl_renderer opengl2

# Force legacy (not recommended)
./idtech3.x86_64 +set cl_renderer opengl
```

### Renderer Compatibility

| Renderer | Vulkan | OpenGL2 | Legacy GL |
|----------|--------|---------|-----------|
| **Performance** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ |
| **Features** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ |
| **Compatibility** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| **Maintenance** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐ |

## Common Startup Issues

### "Failed to load renderer"
**Symptoms**: Engine exits immediately with renderer loading error

**Causes**:
- Missing graphics drivers
- Incompatible GPU
- Corrupted renderer libraries
- Missing Vulkan runtime

**Solutions**:
1. Update graphics drivers
2. Install Vulkan runtime (LunarG SDK)
3. Try different renderer: `+set cl_renderer opengl2`
4. Check GPU compatibility

### "Console history file" errors
**Symptoms**: Warnings about console history file creation

**Causes**:
- Insufficient filesystem permissions
- Read-only filesystem
- Missing home directory

**Solutions**:
- Check write permissions in home directory
- Create `.q3a` directory manually
- Run with elevated permissions (not recommended)

### Memory Allocation Failures
**Symptoms**: "Out of memory" errors during startup

**Causes**:
- Insufficient RAM
- Memory fragmentation
- Large page file requirements

**Solutions**:
- Close other applications
- Increase virtual memory
- Reduce graphics settings
- Use 32-bit build if available

### Shader Compilation Errors
**Symptoms**: Pipeline creation failures in Vulkan

**Causes**:
- Outdated graphics drivers
- GPU not supporting required features
- Corrupted shader cache

**Solutions**:
- Update graphics drivers
- Clear shader cache: delete `shader_cache/` directory
- Try OpenGL2 renderer as fallback

## Performance Optimization

### Vulkan-Specific Optimizations
- Enable hardware-specific features
- Use appropriate memory types
- Optimize pipeline layouts
- Enable shader caching

### General Optimizations
- Use appropriate texture compression
- Enable anisotropic filtering
- Configure appropriate LOD settings
- Monitor memory usage

## Debugging and Diagnostics

### Verbose Logging
Enable detailed logging for troubleshooting:

```bash
./idtech3.x86_64 +set developer 1 +set cl_renderer vulkan
```

### Log File Analysis
Check `logs/` directory for detailed error information:
- `console.log` - Main console output
- `crash.log` - Crash reports
- `performance.log` - Performance metrics

### GPU Information
Use these commands in-game to diagnose GPU issues:
- `gpuinfo` - Display GPU capabilities
- `r_renderer` - Show current renderer
- `vk_device` - Vulkan device information

## Known Issues and Limitations

### Vulkan Renderer
- Requires Vulkan 1.1+ compatible GPU
- Some integrated GPUs may have limited support
- Shader compilation may fail on some drivers

### OpenGL2 Renderer
- May have compatibility issues with very old GPUs
- Performance may be lower than Vulkan
- Some advanced features unavailable

### Legacy OpenGL Renderer
- Deprecated and unmaintained
- Missing modern features
- Poor performance on modern GPUs

### Linux-Specific Issues
- Wayland may require X11 fallback
- Some desktop environments need specific configurations
- Steam overlay may interfere with Vulkan

## Configuration Files

### Auto-execution
Create `baseq3/autoexec.cfg` for automatic configuration:

```c
// Renderer settings
set cl_renderer "vulkan"
set r_mode "6"          // 1280x720
set r_fullscreen "0"    // Windowed mode

// Performance settings
set com_maxfps "125"
set r_lodbias "0"
set r_subdivisions "4"
```

### Hardware-Specific Configurations
Different GPUs may require specific settings:

**NVIDIA GPUs**:
```
set r_ext_multisample "4"
set r_ext_anisotropic_filtering "1"
```

**AMD GPUs**:
```
set r_ext_multisample "2"
set r_arb_vertex_buffer_object "1"
```

**Intel Integrated**:
```
set cl_renderer "opengl2"
set r_mode "4"  // Lower resolution
```

## Getting Help

### Community Resources
- **Forums**: Official Quake 3 community forums
- **Discord**: Real-time support channels
- **GitHub Issues**: Report bugs and request features

### Diagnostic Information
When reporting issues, include:
- System information (`uname -a`)
- GPU information (`lspci | grep VGA`)
- Driver versions (`glxinfo | grep version`)
- Engine version and build
- Full console output with `developer 1`

## Development Notes

### Renderer Development
- Use unified renderer interface for new features
- Test across all supported renderers
- Document renderer-specific limitations
- Maintain backwards compatibility

### Build System
- Renderer libraries are built as shared objects
- Use `USE_RENDERER_DLOPEN=ON` for dynamic loading
- Vulkan requires `USE_VULKAN=ON`
- OpenGL2 requires `USE_OPENGL2=ON`

This documentation is continuously updated as the engine evolves. Check the latest version for the most current information.