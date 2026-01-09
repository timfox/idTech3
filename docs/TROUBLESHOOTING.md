# idTech3 Engine Troubleshooting Guide

## Common Issues

### Engine Won't Start

#### "Failed to load any renderer"

**Symptoms**: Engine exits immediately with renderer loading error

**Solutions**:
1. Check that renderer `.so` files are present:
   ```bash
   ls -la release/*.so
   ```
   Should show `idtech3_vulkan_x86_64.so` and/or `idtech3_opengl_x86_64.so`

2. Install graphics drivers:
   ```bash
   # For Vulkan
   sudo apt-get install vulkan-utils vulkan-tools

   # For OpenGL
   sudo apt-get install mesa-utils
   ```

3. Try forcing OpenGL renderer:
   ```bash
   ./idtech3.x86_64 +set cl_renderer opengl
   ```

4. Check library dependencies:
   ```bash
   ldd release/idtech3_vulkan_x86_64.so
   ldd release/idtech3_opengl_x86_64.so
   ```

5. For build issues, use the recommended build method:
   ```bash
   # Use the wrapper script (recommended)
   bash tools/compile_engine.sh vulkan Release
   ```

6. If CMake times out, try the Makefile directly:
   ```bash
   make vulkan BUILD_TYPE=Release
   ```

#### "pak0.pk3 not found"

**Symptoms**: Warning about missing pak files

**Solutions**:
1. Place game content in `release/base/`:
   ```bash
   cp your_pak_files/*.pk3 release/base/
   ```

2. Or create content directory structure:
   ```bash
   mkdir -p release/base/{maps,textures,scripts}
   ```

3. Use the demo content setup:
   ```bash
   ./tools/create_demo_content.sh
   ```

### Window Doesn't Appear

**Symptoms**: Engine starts but no window appears

**Solutions**:
1. Check if running in headless mode (no display):
   ```bash
   echo $DISPLAY
   ```

2. Try windowed mode explicitly:
   ```bash
   ./idtech3.x86_64 +set r_fullscreen 0
   ```

3. Check Wayland/X11:
   ```bash
   # Try forcing X11
   GDK_BACKEND=x11 ./idtech3.x86_64
   ```

4. Check console output for window creation errors

### Mod Loading Crashes

**Symptoms**: Engine crashes when loading a mod

**Solutions**:
1. Check crash report:
   ```bash
   cat crash_report.txt
   ```

2. Validate mod assets:
   ```bash
   ./tools/validate_assets.sh mods/mymod
   ```

3. Test mod incrementally:
   - Start with empty mod directory
   - Add files one at a time
   - Identify which file causes the crash

4. Check mod VM files (if present):
   - Ensure VM files are for correct architecture
   - Verify VM files are not corrupted

5. Check console output for specific error messages

### Content Not Loading

**Symptoms**: Maps, textures, or other content not appearing

**Solutions**:
1. Verify file locations:
   ```bash
   # Maps should be in:
   release/base/maps/*.bsp
   # Or in mod:
   mods/mymod/maps/*.bsp
   ```

2. Check file names match references:
   - Shader references must match texture file names
   - Map names must match BSP file names

3. Validate pak files:
   ```bash
   unzip -t release/base/pak0.pk3
   ```

4. Check filesystem paths:
   ```bash
   ./idtech3.x86_64 +set fs_debug 1
   ```
   Look for filesystem search path information

### Performance Issues

**Symptoms**: Low FPS, stuttering, lag

**Solutions**:
1. Check renderer selection:
   ```bash
   # Try Vulkan (usually faster)
   ./idtech3.x86_64 +set cl_renderer vulkan
   
   # Or OpenGL
   ./idtech3.x86_64 +set cl_renderer opengl
   ```

2. Adjust resolution:
   ```bash
   ./idtech3.x86_64 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720
   ```

3. Limit FPS:
   ```bash
   ./idtech3.x86_64 +set com_maxfps 60
   ```

4. Check graphics drivers are up to date

### Audio Issues

**Symptoms**: No sound, distorted audio

**Solutions**:
1. Check audio system:
   ```bash
   # Test ALSA
   aplay /usr/share/sounds/alsa/Front_Left.wav
   
   # Test PulseAudio
   pactl list sinks
   ```

2. Adjust audio settings:
   ```bash
   ./idtech3.x86_64 +set s_khz 44 +set s_volume 0.5
   ```

3. Check audio device:
   ```bash
   ./idtech3.x86_64 +set s_device <device_name>
   ```

## Debugging

### Enable Debug Output

```bash
./idtech3.x86_64 +set developer 1 +set logfile 2
```

This enables:
- Verbose console output
- File logging
- Additional diagnostics

### Check Logs

```bash
# Console output is shown in terminal
# Check for error messages and warnings

# Crash reports
cat crash_report.txt

# Filesystem debug
./idtech3.x86_64 +set fs_debug 1
```

### Validate Installation

```bash
# Check engine binary
file release/idtech3.x86_64

# Check renderer libraries
ldd release/idtech3_vulkan_x86_64.so
ldd release/idtech3_opengl_x86_64.so

# Validate content
./tools/validate_assets.sh release/base
```

## Getting More Help

1. **Check console output**: Most errors are logged to console
2. **Review crash reports**: `crash_report.txt` contains detailed crash information
3. **Validate assets**: Use `validate_assets.sh` to check content integrity
4. **Test with minimal setup**: Start with base game, then add mods/content incrementally

## Reporting Issues

When reporting issues, include:

1. Engine version/build information
2. Console output (with `+set developer 1`)
3. Crash report (if applicable)
4. System information:
   ```bash
   uname -a
   lspci | grep VGA
   glxinfo | grep "OpenGL version"  # For OpenGL
   vulkaninfo | grep "apiVersion"    # For Vulkan
   ```
5. Steps to reproduce the issue
