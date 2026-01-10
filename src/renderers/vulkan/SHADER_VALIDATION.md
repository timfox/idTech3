# Vulkan Shader Validation System

## Overview

A comprehensive shader validation system that detects and handles problematic shaders before they cause device loss or crashes. This system centralizes shader validation logic and provides statistics for debugging.

## Features

### Early Detection
- **Shader Name Validation**: Checks shader names before any processing begins
- **Shader Type Validation**: Validates shader types before pipeline creation
- **Pipeline Definition Validation**: Checks for NULL shader modules and pipeline layouts

### Error Handling
- **Device Lost Detection**: Tracks and handles device lost events gracefully
- **Pipeline Creation Error Handling**: Comprehensive error reporting with statistics
- **Automatic Fallback**: Problematic shaders automatically use default shader

### Statistics & Reporting
- Tracks total shaders checked
- Counts problematic shaders skipped
- Monitors device lost events
- Records pipeline creation failures

## Known Problematic Shaders

### By Name
- `models/mapobjects/banner/q3banner02` - Causes device loss
- `models/mapobjects/banner/q3banner04` - Causes device loss

### By Type
- `TYPE_SINGLE_TEXTURE` (1) - Causes SIGFPE
- `TYPE_SINGLE_TEXTURE_FIXED_COLOR` (3) - Causes crashes
- `TYPE_MULTI_TEXTURE_MUL2_IDENTITY` (7) - Causes crashes

## Usage

### Console Command
View validation statistics:
```
r_vk_shaderValidation
```

This will display:
- Total shaders checked
- Problematic shaders skipped
- Device lost events
- Pipeline creation failures
- List of known problematic shaders and types

### Adding New Problematic Shaders

To add a new problematic shader, edit `vk_shader_validation.c`:

```c
static const char *problematic_shader_names[] = {
    "models/mapobjects/banner/q3banner02",
    "models/mapobjects/banner/q3banner04",
    "your/problematic/shader/name",  // Add here
    NULL
};
```

To add a problematic shader type:

```c
static const int problematic_shader_types[] = {
    TYPE_SINGLE_TEXTURE,
    TYPE_SINGLE_TEXTURE_FIXED_COLOR,
    TYPE_MULTI_TEXTURE_MUL2_IDENTITY,
    YOUR_PROBLEMATIC_TYPE,  // Add here
    -1
};
```

## Integration Points

The validation system is integrated at multiple levels:

1. **R_FindShader** (`tr_shader.c`): Early detection before shader processing
2. **FinishShader** (`tr_shader.c`): Secondary check during shader finalization
3. **create_pipeline** (`vk.c`): Validation before pipeline creation
4. **vk_find_pipeline_ext** (`vk_pipeline.c`): Type-based validation
5. **Pipeline Creation** (`tr_shader.c`): Validation with shader name available

## How It Works

1. When a shader is requested via `R_FindShader`, the name is checked against the problematic list
2. If problematic, the default shader is returned immediately
3. During pipeline creation, shader type is validated
4. Pipeline definition is checked for invalid handles
5. Errors during pipeline creation are tracked and reported
6. Statistics are maintained for debugging

## Benefits

- **Prevents Crashes**: Problematic shaders are caught before they cause device loss
- **Better Debugging**: Statistics help identify patterns in shader failures
- **Centralized Management**: All problematic shader logic in one place
- **Easy to Extend**: Simple to add new problematic shaders as they are discovered
- **Non-Breaking**: Falls back to default shader instead of crashing

## Statistics API

```c
// Get statistics
int total, skipped, device_lost, failures;
vk_get_shader_validation_stats(&total, &skipped, &device_lost, &failures);

// Reset statistics
vk_reset_shader_validation_stats();

// Print report
vk_print_shader_validation_report();
```
