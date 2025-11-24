# Animated Skybox (Flipbook) Feature

## Overview

The animated skybox feature allows skyboxes to cycle through multiple texture frames over time, creating dynamic sky effects. Each side of the skybox (6 sides for both outerbox and innerbox) can animate independently through numbered frames, similar to how `animMap` works for regular textures.

## Shader Syntax

### Basic Syntax

```
skyParmsFlipbook <base> <cloudheight> <innerbase> <animSpeed>
```

### Parameters

- **`<base>`**: Base path for outerbox textures (e.g., `env/sky`)
- **`<cloudheight>`**: Cloud height parameter (same as regular `skyParms`)
- **`<innerbase>`**: Base path for innerbox textures (e.g., `env/sky_inner`, or `-` to disable)
- **`<animSpeed>`**: Animation speed in frames per second (e.g., `8.0`)

## File Naming Convention

The engine automatically loads frames using the following naming pattern:

```
<base>_<side>_<frame>.tga
```

Where:
- **`<base>`** is the base path specified in the shader
- **`<side>`** is one of: `rt` (right), `bk` (back), `lf` (left), `ft` (front), `up` (up), `dn` (down)
- **`<frame>`** is the frame number starting from `0` (e.g., `0`, `1`, `2`, ...)

### Example File Structure

For a shader using `skyParmsFlipbook env/sky 512 env/sky_inner 8.0`, the engine will look for:

**Outerbox:**
- `env/sky_rt_0.tga`, `env/sky_rt_1.tga`, `env/sky_rt_2.tga`, ...
- `env/sky_bk_0.tga`, `env/sky_bk_1.tga`, `env/sky_bk_2.tga`, ...
- `env/sky_lf_0.tga`, `env/sky_lf_1.tga`, `env/sky_lf_2.tga`, ...
- `env/sky_ft_0.tga`, `env/sky_ft_1.tga`, `env/sky_ft_2.tga`, ...
- `env/sky_up_0.tga`, `env/sky_up_1.tga`, `env/sky_up_2.tga`, ...
- `env/sky_dn_0.tga`, `env/sky_dn_1.tga`, `env/sky_dn_2.tga`, ...

**Innerbox:**
- `env/sky_inner_rt_0.tga`, `env/sky_inner_rt_1.tga`, ...
- (same pattern for all 6 sides)

## Shader Examples

### Example 1: Simple Animated Skybox

```
textures/skies/animated_sky
{
    skyParmsFlipbook env/sky 512 env/sky_inner 8.0
    // Animation speed: 8 frames per second
}
```

This will animate through all available frames for each side at 8 frames per second.

### Example 2: Fast Animation

```
textures/skies/fast_animated_sky
{
    skyParmsFlipbook env/storm 512 - 15.0
    // Fast animation: 15 frames per second
    // No innerbox (using "-")
}
```

### Example 3: Slow, Smooth Animation

```
textures/skies/slow_animated_sky
{
    skyParmsFlipbook env/clouds 512 env/clouds_inner 2.0
    // Slow animation: 2 frames per second for smooth transitions
}
```

## Technical Details

### Frame Loading

- The engine automatically loads frames starting from frame `0` and continues until a frame file is not found
- Each side can have a different number of frames (flexible per-side animation)
- Maximum frames per side: **24** (defined by `MAX_SKY_ANIMATIONS`)
- If no frames are found for a side, the default image is used

### Animation Timing

- Animation speed is specified in frames per second
- Frame selection uses modulo arithmetic to wrap animation cycles smoothly
- Animation is synchronized with shader time (`tess.shaderTime`)
- The calculation matches the logic used by `animMap` for regular textures

### Per-Side Animation

Each side of the skybox animates independently:
- Different sides can have different frame counts
- All sides use the same animation speed
- Frame selection is calculated per-side based on shader time

### Backward Compatibility

- Existing `skyParms` shaders continue to work unchanged
- Non-animated skyboxes use `outerbox[i][0]` automatically
- The `isAnimated` flag defaults to `qfalse` for regular skyboxes

## Limitations

1. **Frame Count**: Maximum 24 frames per side
2. **File Format**: Currently supports `.tga` format only (same as regular skyboxes)
3. **Naming**: Frame numbers must start from `0` and be sequential (no gaps)
4. **Animation Speed**: Must be greater than `0.0` (defaults to `8.0` if invalid)

## Tips

1. **Consistent Frame Counts**: While different sides can have different frame counts, using the same count for all sides creates smoother, more predictable animations

2. **Animation Speed**: 
   - Lower speeds (2-5 fps) create smooth, slow transitions
   - Medium speeds (8-12 fps) work well for most effects
   - Higher speeds (15+ fps) create fast, dynamic effects

3. **File Organization**: Keep all frames for a skybox in the same directory for easier management

4. **Testing**: Use `r_showsky 1` to see skybox rendering clearly during development

5. **Performance**: Animated skyboxes have minimal performance impact - frame selection is O(1) per side

## Comparison with Regular Skyboxes

| Feature | Regular `skyParms` | Animated `skyParmsFlipbook` |
|---------|-------------------|----------------------------|
| Frames per side | 1 | Up to 24 |
| Animation | Static | Animated |
| File naming | `<base>_<side>.tga` | `<base>_<side>_<frame>.tga` |
| Speed control | N/A | Configurable fps |
| Per-side frames | Same for all | Can differ |

## Implementation Notes

- Supported in all three renderers: OpenGL, Vulkan, and renderer2
- Uses the same animation timing system as `animMap` for consistency
- Frame selection happens at render time, ensuring smooth animation
- Memory overhead is minimal - only stores pointers to loaded images

## See Also

- `animMap` - Similar animation system for regular textures
- `skyParms` - Standard skybox shader command
- Shader documentation for other sky-related features

