# id Tech 3 Configuration Guide

This guide covers the configuration and setup of the id Tech 3 engine for development, deployment, and troubleshooting.

## Table of Contents

1. [Build Configuration](#build-configuration)
2. [Runtime Configuration](#runtime-configuration)
3. [Network Configuration](#network-configuration)
4. [Renderer Configuration](#renderer-configuration)
5. [Audio Configuration](#audio-configuration)
6. [Performance Tuning](#performance-tuning)
7. [Security Configuration](#security-configuration)
8. [Development Setup](#development-setup)

## Build Configuration

### CMake Build Options

The engine supports various build configurations through CMake options:

```bash
# Basic build
cmake -S . -B build
make -j$(nproc)

# Debug build with testing
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
make -j$(nproc)

# Vulkan-only build
cmake -S . -B build-vulkan -DUSE_RENDERER_DLOPEN=OFF -DUSE_VULKAN=ON

# Minimal build for embedded systems
cmake -S . -B build-minimal -DUSE_OPENAL=OFF -DUSE_CURL=OFF
```

### Platform-Specific Configuration

#### Linux
```bash
# Install dependencies
sudo apt-get install build-essential cmake libsdl2-dev libopenal-dev libcurl4-openssl-dev

# For Vulkan support
sudo apt-get install libvulkan-dev vulkan-tools

# Build
cmake -S . -B build && make -j$(nproc)
```

#### Windows (MSYS2)
```bash
# Install dependencies via pacman
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-openal

# Build
cmake -S . -B build -G "MSYS Makefiles" && make -j$(nproc)
```

#### macOS
```bash
# Install dependencies via Homebrew
brew install cmake sdl2 openal-soft

# Build
cmake -S . -B build && make -j$(nproc)
```

### Cross-Compilation

For embedded platforms or different architectures:

```bash
# ARM cross-compilation
cmake -S . -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-linux-gnueabihf.cmake \
  -DUSE_OPENGL=OFF \
  -DUSE_VULKAN=OFF

# Android
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24
```

## Runtime Configuration

### Configuration Files

The engine uses several configuration files:

- `default.cfg`: Default game settings
- `q3config.cfg`: User-specific settings
- `autoexec.cfg`: Automatic execution on startup

### CVAR System

The engine uses Console Variables (CVARs) for configuration:

```c
// Setting a CVAR
Cvar_Set("r_mode", "6");  // 800x600 resolution

// Getting a CVAR value
int mode = Cvar_VariableIntegerValue("r_mode");

// Creating a new CVAR
cvar_t *myCvar = Cvar_Get("my_setting", "default_value", CVAR_ARCHIVE);
```

### Important CVARs

#### System
- `com_developer`: Enable developer mode (0/1)
- `com_logfile`: Enable logging to file (0/1)
- `com_maxfps`: Maximum FPS limit

#### Rendering
- `r_renderer`: Renderer backend ("opengl", "vulkan", "rtx")
- `r_mode`: Video mode/resolution
- `r_fullscreen`: Fullscreen mode (0/1)
- `r_vsync`: Vertical sync (0/1)

#### Networking
- `net_port`: Server port
- `net_ip`: Bind address
- `rate`: Network rate limit
- `snaps`: Snapshot rate

#### Audio
- `s_volume`: Master volume (0.0-1.0)
- `s_musicvolume`: Music volume (0.0-1.0)
- `s_doppler`: Doppler effect (0/1)

## Network Configuration

### Server Setup

```bash
# Start a dedicated server
./idtech3.x86_64 +set dedicated 1 +set sv_hostname "My Server" +exec server.cfg

# Start a listen server
./idtech3.x86_64 +set sv_hostname "My Server" +map q3dm1
```

### Client Connection

```bash
# Connect to server
./idtech3.x86_64 +connect 192.168.1.100:27960

# Connect with password
./idtech3.x86_64 +set password "secret" +connect server.example.com
```

### Firewall Configuration

The engine uses UDP ports (default 27960). Ensure these are open:

```bash
# Linux iptables
sudo iptables -A INPUT -p udp --dport 27960 -j ACCEPT

# UFW
sudo ufw allow 27960/udp
```

### Network Optimization

For better network performance:

```c
// Server-side
set sv_fps "20"           // Server tick rate
set sv_reconnectlimit "3" // Connection retry limit
set sv_zombietime "2"     // Zombie client timeout

// Client-side
set rate "25000"          // Connection rate limit
set snaps "20"            // Snapshot rate
set cl_maxpackets "30"    // Max packets per second
```

## Renderer Configuration

### OpenGL Renderer

```c
// Basic OpenGL settings
set r_renderer "opengl"
set r_allowExtensions "1"
set r_ext_compressed_textures "1"
set r_ext_multitexture "1"
set r_ext_compiled_vertex_array "1"

// Performance settings
set r_lodbias "0"
set r_lodscale "5"
set r_subdivisions "4"
set r_vertexlight "0"
```

### Vulkan Renderer

```c
// Vulkan settings
set r_renderer "vulkan"
set r_vkValidation "0"        // Disable validation for performance
set r_vkDevice "0"            // GPU selection
set r_vkMeshShaders "0"       // Enable mesh shaders
set r_vkRayTracing "0"        // Enable ray tracing

// Advanced settings
set r_vkMaxDeviceMemoryMB "1024"
set r_vkStagingBufferSizeMB "64"
```

### Performance Tuning

```c
// General performance
set r_fastsky "1"
set r_drawSun "0"
set r_dynamiclight "0"
set r_dlightBacks "0"

// Texture settings
set r_texturebits "16"
set r_colorbits "16"
set r_depthbits "16"

// Geometry detail
set r_detailtextures "0"
set r_picmip "1"        // Reduce texture quality
set r_roundImagesDown "1"
```

## Audio Configuration

### OpenAL Audio

```c
// Audio device selection
set s_device "default"

// Volume settings
set s_volume "0.8"
set s_musicvolume "0.4"
set s_doppler "1"

// Performance
set s_khz "22"         // Sample rate (11, 22, 44)
set s_loadas8bit "1"   // Load as 8-bit for performance
set s_mixahead "0.2"   // Audio buffer size
```

### Audio Troubleshooting

```bash
# List available audio devices
./idtech3.x86_64 +set s_listAudioDevices 1

# Test audio playback
./idtech3.x86_64 +playSound sound/misc/menu1.wav
```

## Performance Tuning

### CPU Optimization

```c
// Multi-threading
set r_multithread "1"
set r_threadedRenderer "1"

// CPU usage
set com_maxfps "125"
set com_busywait "0"
set com_yield "1"
```

### Memory Optimization

```c
// Memory limits
set com_hunkMegs "128"
set com_zoneMegs "24"
set com_soundMegs "8"

// Cache settings
set r_cache "1"
set r_cacheShaders "1"
set r_cacheModels "1"
```

### Profiling

```c
// Enable profiling
set cl_profile "1"
set r_speeds "1"
set com_speeds "1"

// Performance monitoring
perf_report          // Show performance counters
perf_reset           // Reset performance counters
```

## Security Configuration

### Server Security

```c
// Access control
set sv_privateClients "0"
set sv_privatePassword ""
set rconPassword "your_secure_password"

// Anti-cheat
set sv_pure "1"
set sv_cheats "0"
set g_antiwarp "1"
```

### Client Security

```c
// Input validation
set cl_validate_input "1"

// Network security
set cl_nodelta "0"
set cl_noprint "0"
```

### File System Security

```c
// Path restrictions
set fs_restrict "1"
set fs_basepath "/secure/path"
set fs_homepath "/user/safe/path"
```

## Development Setup

### Development Environment

```bash
# Enable development features
set com_developer "1"
set developer "1"
set r_developer "1"
set sv_cheats "1"

# Logging
set com_logfile "1"
set com_logfileName "q3.log"
set logfile "2"  // Console logging level
```

### Debugging Tools

```c
// Memory debugging
set com_checkMemory "1"
set r_debugSurface "1"

// Network debugging
set net_showpackets "0"
set net_showdrop "0"

// Rendering debugging
set r_showtris "0"
set r_shownormals "0"
set r_showmodelbounds "0"
```

### Testing Configuration

```c
// Automated testing
set com_buildScript "1"
set com_introPlayed "1"

// Benchmarking
set com_aviDemo "0"
set cl_aviFrameRate "25"
```

## Troubleshooting

### Common Issues

#### Engine won't start
```bash
# Check library dependencies
ldd ./idtech3.x86_64

# Check file permissions
ls -la idtech3.x86_64

# Run with verbose output
./idtech3.x86_64 +set com_developer 1 2>&1 | head -50
```

#### Graphics issues
```bash
# Reset renderer settings
./idtech3.x86_64 +set r_renderer opengl +set r_mode 6 +vid_restart

# Check OpenGL extensions
./idtech3.x86_64 +set r_allowExtensions 0

# Force software rendering (for testing)
./idtech3.x86_64 +set r_renderer "software"
```

#### Network connection issues
```bash
# Test basic connectivity
ping server.address

# Check port availability
netstat -uln | grep 27960

# Test with different protocols
./idtech3.x86_64 +set net_ip localhost +connect localhost:27960
```

#### Performance issues
```bash
# Enable performance monitoring
./idtech3.x86_64 +set r_speeds 1 +set com_speeds 1

# Profile CPU usage
perf record ./idtech3.x86_64
perf report

# Memory profiling
valgrind --tool=massif ./idtech3.x86_64
```

### Diagnostic Commands

```c
// System information
version
sysinfo

// Memory status
meminfo
hunkinfo

// Network status
netstat
ping

// File system
fs_info
fs_list

// Performance
perf_report
r_speeds 1
```

### Log Analysis

```bash
# Monitor logs in real-time
tail -f ~/.q3a/q3config.cfg

# Search for errors
grep -i "error\|warning\|failed" q3.log

# Performance analysis
grep "fps\|frame" q3.log | tail -20
```

## Advanced Configuration

### Custom Game Types

```c
// Mod configuration
set fs_game "mymod"
set vm_game "1"        // Enable VM for game module
set vm_cgame "1"       // Enable VM for client game module
set vm_ui "1"          // Enable VM for UI module
```

### Multi-GPU Setup

```c
// GPU selection
set r_device "0"       // Primary GPU
set r_device "1"       // Secondary GPU (if available)

// Vulkan device selection
set r_vkDevice "0"     // Vulkan physical device index
```

### Internationalization

```c
// Language settings
set s_language "english"
set ui_language "english"

// Unicode support
set com_ansiColor "1"
set com_consoleCommand "1"
```

This configuration guide provides comprehensive setup and troubleshooting information for the id Tech 3 engine. For additional support, consult the troubleshooting guide or community forums.