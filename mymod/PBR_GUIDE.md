# PBR Material Creation Guide

This guide explains how to create Physically Based Rendering (PBR) materials for use with the quake3e engine's Vulkan renderer.

## Overview

PBR (Physically Based Rendering) provides realistic material rendering by simulating how light interacts with surfaces. The quake3e engine supports two PBR workflows:

1. **Metallic/Roughness Workflow** (recommended)
2. **Specular/Gloss Workflow** (alternative)

## Prerequisites

Before creating PBR materials, ensure:

- PBR rendering is enabled: `set r_pbr 1`
- Framebuffer objects are enabled: `set r_fbo 1` (REQUIRED)
- You're using the Vulkan renderer: `cl_renderer vulkan`
- Vertex buffer objects recommended: `set r_vbo 1`

## Texture Naming Conventions

The engine automatically detects PBR textures based on naming conventions. Place textures in `textures/` or `textures/pbr/` directories.

### Base Texture
- **File**: `texturename.tga` (or `.jpg`, `.png`)
- **Description**: The main diffuse/albedo texture
- **Format**: RGB color values (sRGB color space)
- **Example**: `brick_wall.tga`

### Normal Map
- **File**: `texturename_normal.tga`
- **Description**: Surface detail and bump mapping
- **Format**: RGB normal map (standard tangent-space normals)
- **Channels**: 
  - R: X normal component
  - G: Y normal component  
  - B: Z normal component (usually 1.0)
- **Example**: `brick_wall_normal.tga`
- **CVAR**: `r_baseNormalX` and `r_baseNormalY` control normal map intensity

### Parallax Mapping
- Normal maps support parallax occlusion mapping
- **CVAR**: `r_baseParallax` controls parallax depth (default: 0.05)
- Higher values create more pronounced depth effect

## Metallic/Roughness Workflow (Recommended)

This is the most common PBR workflow and uses an ORM (Occlusion/Roughness/Metallic) texture.

### ORM Texture
- **File**: `texturename_orm.tga` or `texturename_rmo.tga`
- **Description**: Combined material properties
- **Format**: RGB texture with specific channel meanings

#### ORM Format (Occlusion/Roughness/Metallic)
- **R Channel**: Ambient Occlusion (0 = fully occluded, 1 = no occlusion)
- **G Channel**: Roughness (0 = smooth/mirror-like, 1 = rough/matte)
- **B Channel**: Metallic (0 = dielectric/non-metal, 1 = metal)

#### RMO Format (Roughness/Metallic/Occlusion)
- **R Channel**: Roughness
- **G Channel**: Metallic
- **B Channel**: Ambient Occlusion

**Example**: `brick_wall_orm.tga`

### Material Properties

#### Metallic Surfaces
- **Metallic = 1.0**: Pure metals (gold, silver, chrome)
- **Metallic = 0.0**: Non-metals (wood, stone, fabric)
- **Metallic = 0.5**: Mixed materials (rusted metal, painted metal)

#### Roughness Values
- **Roughness = 0.0**: Mirror-like surfaces (chrome, water)
- **Roughness = 0.1-0.3**: Smooth surfaces (polished wood, ceramic)
- **Roughness = 0.4-0.7**: Average surfaces (concrete, brick)
- **Roughness = 0.8-1.0**: Rough surfaces (fabric, rough stone)

#### Ambient Occlusion
- Darkens crevices and areas where light doesn't reach
- Adds depth and realism to materials
- Usually baked from 3D models or painted manually

## Specular/Gloss Workflow (Alternative)

This workflow uses separate specular and gloss maps.

### Specular Map
- **File**: `texturename_spec.tga`
- **Description**: Specular color and intensity
- **Format**: RGB specular color
- **Usage**: Defines the color and intensity of reflections
- **CVAR**: `r_baseSpecular` controls base specular value (default: 0.04)

### Gloss Map
- Usually stored in the alpha channel of the specular map
- **Gloss = 1.0**: Smooth, reflective surface
- **Gloss = 0.0**: Rough, matte surface

## Texture Formats

### Supported Formats
- **TGA** (recommended for best quality)
- **JPG** (smaller file size, lossy compression)
- **PNG** (lossless compression, supports alpha)

### Color Space
- Textures should be in **sRGB color space**
- The engine handles sRGB to linear conversion automatically
- Normal maps should be in linear space (not sRGB)

### Resolution Guidelines
- Base textures: Match your target resolution (512x512, 1024x1024, 2048x2048)
- Normal maps: Same resolution as base texture
- ORM maps: Same resolution as base texture (can be lower quality)
- Maximum texture size: 2048x2048 (configurable via `r_maxTextureSize`)

## Creating PBR Textures

### Method 1: From 3D Software
1. Create materials in Blender, Substance Painter, or similar
2. Export textures using the ORM workflow
3. Ensure proper channel assignments:
   - Ambient Occlusion → R channel
   - Roughness → G channel
   - Metallic → B channel

### Method 2: Manual Creation
1. **Base Texture**: Create or source your main color texture
2. **Normal Map**: Generate from height map or paint manually
3. **ORM Map**: 
   - Paint or generate ambient occlusion
   - Create roughness map (white = rough, black = smooth)
   - Create metallic map (white = metal, black = non-metal)
   - Combine into RGB channels

### Method 3: Using Texture Tools
- **Substance Designer/Painter**: Industry standard for PBR materials
- **Materialize**: Free tool for generating PBR textures
- **xNormal**: Generate normal maps from height maps
- **GIMP/Photoshop**: Manual texture creation and editing

## Shader Configuration

The engine automatically handles PBR shaders, but you can customize materials via CVARs:

```
// Normal map intensity
set r_baseNormalX "1.0"      // X-axis intensity
set r_baseNormalY "1.0"      // Y-axis intensity

// Parallax mapping depth
set r_baseParallax "0.05"    // Depth effect (0.0 = disabled)

// Base specular value (for non-metals)
set r_baseSpecular "0.04"    // Standard dielectric specular
```

## Environment Mapping

For realistic reflections, enable cube mapping:

```
set r_cubeMapping "1"
```

This creates environment reflections based on the surrounding scene. The engine generates cubemaps automatically or you can provide custom ones.

### Cubemap Settings
- **Irradiance Size**: 64x64 (for ambient lighting)
- **Reflection Size**: 256x256 (for reflections)
- Configured in renderer code, not via CVARs

## HDR Rendering

Enable HDR for better color accuracy:

```
set r_hdr "1"
```

HDR (High Dynamic Range) provides:
- Better color accuracy
- Reduced color banding
- Improved lighting quality
- Required for bloom effects

## Material Examples

### Example 1: Brick Wall
```
brick_wall.tga          // Base color texture
brick_wall_normal.tga  // Normal map for surface detail
brick_wall_orm.tga     // ORM map:
                       //   R: Ambient occlusion in mortar lines
                       //   G: Roughness (brick is fairly rough)
                       //   B: Metallic (0.0 - brick is non-metallic)
```

### Example 2: Metal Surface
```
metal_plate.tga        // Base color (slightly tinted)
metal_plate_normal.tga // Normal map (scratches, dents)
metal_plate_orm.tga    // ORM map:
                       //   R: Ambient occlusion
                       //   G: Roughness (0.1-0.3 for polished metal)
                       //   B: Metallic (1.0 - pure metal)
```

### Example 3: Wood Surface
```
wood_plank.tga         // Base color (wood grain)
wood_plank_normal.tga  // Normal map (grain detail)
wood_plank_orm.tga     // ORM map:
                       //   R: Ambient occlusion in grain
                       //   G: Roughness (0.3-0.5 for wood)
                       //   B: Metallic (0.0 - wood is non-metallic)
```

## Testing Your Materials

1. **Load the mod**: `./quake3e +set fs_game mymod`
2. **Enable PBR**: Verify `r_pbr` shows `1`
3. **Load a test map**: Create or use a map with your textures
4. **Check console**: Look for texture loading messages
5. **Adjust settings**: Use CVARs to fine-tune appearance

## Troubleshooting

### Textures Not Loading
- Check file naming conventions match exactly
- Verify texture format is supported (TGA, JPG, PNG)
- Check console for error messages
- Ensure textures are in correct directory (`textures/` or `textures/pbr/`)

### PBR Not Working
- Verify `r_pbr 1` is set
- Ensure `r_fbo 1` is enabled (REQUIRED)
- Check that you're using Vulkan renderer: `cl_renderer vulkan`
- Verify texture naming matches conventions

### Materials Look Wrong
- Check ORM channel assignments (R/G/B)
- Verify normal map is in correct format (not sRGB)
- Adjust `r_baseNormalX/Y` for normal map intensity
- Check `r_baseSpecular` for specular workflow

### Performance Issues
- Reduce texture resolution if needed
- Disable cube mapping: `set r_cubeMapping 0`
- Disable HDR: `set r_hdr 0`
- Lower parallax depth: `set r_baseParallax 0.02`

## Advanced Features

### Custom Shaders
- Place custom shader files in `shaders/` directory
- Use Quake III shader syntax
- PBR shaders are handled automatically by the renderer

### Texture Swizzling
The engine supports texture channel swizzling for different ORM formats:
- ORM (Occlusion/Roughness/Metallic) - default
- RMO (Roughness/Metallic/Occlusion)
- Other combinations via renderer configuration

### Material Variants
Create multiple material variants:
- `texture_clean_orm.tga` - Clean version
- `texture_dirty_orm.tga` - Worn version
- `texture_wet_orm.tga` - Wet version (lower roughness)

## Resources

- [PBR Theory Guide](https://learnopengl.com/PBR/Theory)
- [Substance PBR Guide](https://docs.substance3d.com/sddoc/pbr-workflows-172820612.html)
- [Materialize Tool](https://www.boundingboxsoftware.com/materialize/)
- quake3e Renderer Documentation

## Summary

Creating PBR materials requires:
1. Base color texture (albedo)
2. Normal map for surface detail
3. ORM map (Occlusion/Roughness/Metallic)
4. Proper naming conventions
5. Correct texture formats and color spaces

Follow the naming conventions, enable PBR rendering, and your materials will automatically use the PBR pipeline for realistic lighting and reflections.

