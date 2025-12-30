# Engine Profiling System

The idTech3 engine includes a comprehensive profiling system that integrates multiple profiling backends for detailed performance analysis.

## Overview

The profiling system combines:
- **Tracy**: Real-time CPU/GPU profiling with visualization
- **Vulkan Render Profiler**: GPU render graph analysis and bottleneck detection
- **Performance Benchmarking**: Automated regression detection and statistical analysis
- **Memory Bandwidth Profiler**: Memory access pattern analysis
- **Custom Profiling**: High-level engine profiling macros

## Quick Start

### Build with Profiling Support

```bash
# Enable Tracy profiling (recommended)
cmake -S . -B build -DUSE_TRACY=1

# Or enable basic profiling only
cmake -S . -B build

# Build
cmake --build build -j
```

### Runtime Control

```bash
# Set profiling mode via CVAR
set profiler_mode 3  # 0=disabled, 1=basic, 2=vulkan, 3=full, 4=benchmark

# Or use console commands
profiler_toggle      # Cycle through modes
profiler_status      # Show current status
profiler_dump        # Dump current frame data
```

### Tracy Integration

When built with `USE_TRACY=1`, the engine integrates with Tracy profiler:

```bash
# Run Tracy profiler server
./tracy-capture -o trace.tracy

# Run the engine with profiling enabled
./build/idtech3.x86_64 +set profiler_mode 3

# View results in Tracy profiler UI
./tracy-profiler trace.tracy
```

## Profiling Modes

### PROFILER_MODE_DISABLED (0)
- No profiling overhead
- Default state

### PROFILER_MODE_BASIC (1)
- Tracy CPU/GPU profiling
- Frame markers and basic zones
- Low overhead

### PROFILER_MODE_VULKAN (2)
- Vulkan render graph profiling
- GPU timestamp queries
- Memory bandwidth analysis
- Higher overhead

### PROFILER_MODE_FULL (3)
- All profiling systems enabled
- Maximum detail and analysis
- Highest overhead

### PROFILER_MODE_BENCHMARK (4)
- Performance benchmarking mode
- Statistical analysis
- Regression detection

## Console Commands

### Status and Control
- `profiler_status` - Show current profiling configuration and statistics
- `profiler_toggle` - Cycle through profiling modes
- `profiler_reset` - Reset all profiling statistics

### Data Export
- `profiler_dump` - Dump current frame profiling data to console
- `profiler_export [filename]` - Export profiling data to text file
- `profiler_export_json [filename]` - Export data in JSON format
- `profiler_export_csv [filename]` - Export data in CSV format

### Detail Control
- `profiler_increase_detail` - Increase profiling detail level
- `profiler_decrease_detail` - Decrease profiling detail level

## CVARs

### profiler_mode
Controls the profiling mode:
- `0` - Disabled (default)
- `1` - Basic Tracy profiling
- `2` - Vulkan render profiling
- `3` - Full profiling
- `4` - Benchmark mode

### profiler_overhead_limit
Maximum acceptable profiling overhead as a percentage (default: 5.0)

## Code Integration

### Basic Profiling Macros

```c
#include "common/profiler.h"

// Function profiling
PROF_FUNCTION();

// Custom scope profiling
PROF_SCOPE("My Custom Scope");

// GPU profiling zones
PROF_VULKAN_FRAME_BEGIN();
PROF_VULKAN_PASS_BEGIN("Shadow Pass");
// ... render shadow pass ...
PROF_VULKAN_PASS_END();
PROF_VULKAN_FRAME_END();

// Value plotting
PROF_VALUE("FPS", current_fps);
PROF_VALUE("Frame Time", frame_time_ms);
```

### Tracy-Specific Features

```c
// Thread naming
PROF_THREAD_NAME("Render Thread");

// Memory tracking
PROF_ALLOC(ptr, size);
PROF_FREE(ptr);

// Messages
PROF_MESSAGE("Starting expensive operation");
```

## Vulkan Profiling

The Vulkan profiler provides detailed GPU analysis:

### Render Graph Profiling
- Per-pass GPU time measurement
- Bottleneck detection and optimization hints
- Memory usage tracking per frame

### Memory Bandwidth Analysis
- Memory access pattern analysis
- Cache performance metrics
- Bandwidth utilization statistics

### Commands
```bash
# Print render profiler stats (requires Vulkan mode)
profiler_status  # Shows Vulkan profiling info

# Export detailed Vulkan profiling data
profiler_export_json vulkan_profile.json
```

## Performance Benchmarking

The benchmarking system provides automated performance regression detection:

### Benchmark Categories
- Rendering performance
- Memory usage
- I/O performance
- Network performance
- Audio processing
- Physics simulation
- AI/pathfinding

### Statistical Analysis
- Confidence intervals
- Regression detection
- Trend analysis
- Comparative reporting

## Test Programs

Several test programs demonstrate profiling integration:

### test_profiler_integration
```bash
./build/tests/test_profiler_integration
```
Tests profiler initialization, mode switching, and data export.

### test_render_profiler
```bash
./build/tests/test_render_profiler
```
Tests Vulkan render profiler integration.

### test_memory_profiler
```bash
./build/tests/test_memory_profiler
```
Tests memory bandwidth profiling.

## Output Files

Profiling data can be exported in multiple formats:

### JSON Format
```json
{
  "mode": "Full Profiling",
  "timestamp": 1234567890,
  "performance": {
    "frame_count": 1000,
    "fps_average": 60.0,
    "frame_time_average_ms": 16.67
  },
  "memory": {
    "current_allocation": 1048576,
    "peak_allocation": 2097152
  }
}
```

### CSV Format
```
timestamp,mode,fps_average,frame_time_average_ms,memory_current
1234567890,Full Profiling,60.0,16.67,1048576
```

## Performance Considerations

### Overhead Management
- Profiling adds overhead - use `profiler_overhead_limit` to control maximum impact
- Basic Tracy profiling: ~1-2% overhead
- Vulkan profiling: ~5-10% overhead
- Full profiling: ~10-20% overhead

### Memory Usage
- Profiling buffers consume memory
- Vulkan timestamp queries require GPU memory
- Statistical analysis buffers grow with frame count

### Best Practices
1. Use basic profiling for development iteration
2. Enable Vulkan profiling for GPU bottleneck analysis
3. Use benchmark mode for performance regression testing
4. Export data regularly to avoid memory bloat
5. Set appropriate overhead limits for your target hardware

## Troubleshooting

### Tracy Connection Issues
- Ensure Tracy server is running
- Check firewall settings for Tracy ports
- Verify USE_TRACY=1 in build configuration

### Vulkan Profiling Not Working
- Ensure Vulkan renderer is active
- Check that profiler_mode includes Vulkan profiling
- Verify GPU supports timestamp queries

### High Overhead
- Reduce profiler_overhead_limit CVAR
- Switch to less detailed profiling mode
- Use profiler_decrease_detail command

### Missing Data
- Check profiler_status for current configuration
- Ensure profiler is enabled and initialized
- Verify export commands are working