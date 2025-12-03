# PBR Textures Directory

This directory is intended for Physically Based Rendering (PBR) material textures.

## Purpose

Place your PBR material textures here to keep them organized separately from standard textures. The engine will automatically detect PBR textures based on naming conventions regardless of which directory they're in (`textures/` or `textures/pbr/`).

## Texture Naming Conventions

For PBR materials to work correctly, textures should follow these naming conventions:

### Base Texture
- **File**: `texturename.tga` (or `.jpg`, `.png`)
- **Description**: The main diffuse/albedo texture
- **Example**: `metal_plate.tga`

### Normal Map
- **File**: `texturename_normal.tga`
- **Description**: Surface detail and bump mapping
- **Example**: `metal_plate_normal.tga`

### ORM Map (Occlusion/Roughness/Metallic)
- **File**: `texturename_orm.tga` or `texturename_rmo.tga`
- **Description**: Combined material properties
- **Channels**:
  - **ORM format**: R=Occlusion, G=Roughness, B=Metallic
  - **RMO format**: R=Roughness, G=Metallic, B=Occlusion
- **Example**: `metal_plate_orm.tga`

### Specular Map (Alternative Workflow)
- **File**: `texturename_spec.tga`
- **Description**: Specular color and intensity
- **Example**: `metal_plate_spec.tga`

## Example Material Set

```
metal_plate.tga          # Base color/albedo
metal_plate_normal.tga   # Normal map
metal_plate_orm.tga      # ORM map (Occlusion/Roughness/Metallic)
```

## Requirements

- PBR rendering must be enabled: `set r_pbr 1`
- Framebuffer objects required: `set r_fbo 1`
- Vulkan renderer: `cl_renderer vulkan`

## Documentation

See `PBR_GUIDE.md` in the mod root directory for detailed information on creating PBR materials.

## Notes

- Textures can also be placed in the main `textures/` directory
- The engine automatically detects PBR textures by naming convention
- Supported formats: TGA, JPG, PNG
- Base textures should be in sRGB color space
- Normal maps should be in linear color space

