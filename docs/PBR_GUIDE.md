# Physically Based Rendering (PBR) Guide

This guide explains the PBR material system implemented in the id Tech 3 engine.

## Overview

The engine supports modern physically-based rendering with metallic/roughness workflow, providing more realistic material appearance compared to traditional specular/glossiness materials.

## Supported Renderers

Both OpenGL and Vulkan renderers support PBR materials:

- **OpenGL Renderer**: Basic PBR support through enhanced shaders
- **Vulkan Renderer**: Full PBR pipeline with advanced features

## PBR Shader Keywords

### Basic Material Properties

```
surfaceparm metal         // Marks surface as metallic (default: dielectric)
surfaceparm roughness 0.5 // Surface roughness (0.0 = mirror, 1.0 = rough)
surfaceparm metallic 0.0  // Metallic value (0.0 = dielectric, 1.0 = metal)
```

### Texture Maps

```
map <diffuse_texture>           // Base color/diffuse texture
normalMap <normal_texture>      // Normal map for surface detail
specularMap <specular_texture>  // Specular reflectivity map
metallicMap <metallic_texture>  // Metallic value texture
roughnessMap <roughness_texture> // Roughness value texture
physicalMap <orm_texture>       // Packed ORM (Occlusion/Roughness/Metallic) texture
```

## Vulkan Renderer Advanced Features

The Vulkan renderer supports additional packed texture formats:

```
rmoMap   // Roughness/Metallic/Occlusion
ormMap   // Occlusion/Roughness/Metallic (standard)
moxrMap  // Metallic/Occlusion/Roughness
```

## CVAR Controls

```
r_pbr           // Enable PBR rendering (default: 1)
r_pbrMetallic   // Default metallic value (default: 0.0)
r_pbrRoughness  // Default roughness value (default: 0.5)
r_normalMapping // Enable normal mapping (default: 1)
r_specularMapping // Enable specular mapping (default: 1)
```

## Material Examples

### Metallic Surface
```
textures/metal/gold
{
    qer_editorimage textures/metal/gold_d
    surfaceparm metal
    surfaceparm roughness 0.1
    surfaceparm metallic 1.0

    {
        map textures/metal/gold_d
        normalMap textures/metal/gold_n
        metallicMap textures/metal/gold_m
        roughnessMap textures/metal/gold_r
    }
}
```

### Dielectric Plastic
```
textures/plastic/blue_plastic
{
    qer_editorimage textures/plastic/blue_plastic_d
    surfaceparm roughness 0.3
    surfaceparm metallic 0.0

    {
        map textures/plastic/blue_plastic_d
        normalMap textures/plastic/blue_plastic_n
        roughnessMap textures/plastic/blue_plastic_r
    }
}
```

### ORM Packed Texture
```
textures/stone/cobblestone
{
    qer_editorimage textures/stone/cobblestone_d

    {
        map textures/stone/cobblestone_d
        normalMap textures/stone/cobblestone_n
        physicalMap textures/stone/cobblestone_orm
    }
}
```

## Texture Format Guidelines

### Diffuse/Albedo (RGB)
- Linear color space
- No gamma correction needed
- Represents base color of material

### Normal Maps (RGB)
- Standard tangent-space normals
- Red = X, Green = Y, Blue = Z
- Should be uncompressed for quality

### Metallic/Roughness (Grayscale or Packed)
- **Metallic**: Black = dielectric, White = metal
- **Roughness**: Black = smooth/mirror, White = rough/diffuse
- Can be packed into RGB channels (R=M, G=R, B=AO)

### ORM Packing Convention
- **Red**: Occlusion (Ambient Occlusion)
- **Green**: Roughness
- **Blue**: Metallic

## Performance Considerations

- PBR materials have higher computational cost
- Use appropriate texture sizes (256x256 to 1024x1024)
- Consider LOD for distant surfaces
- Normal maps significantly increase vertex processing

## Debugging

Use these console commands to inspect PBR systems:

```
physics_info     // Show physics/PBR system status
r_pbr 0/1        // Toggle PBR rendering
r_normalMapping 0/1  // Toggle normal mapping
r_specularMapping 0/1 // Toggle specular mapping
```

## Compatibility

- PBR materials automatically fall back to traditional rendering when disabled
- Materials without PBR textures render as traditional surfaces
- All existing shaders remain compatible