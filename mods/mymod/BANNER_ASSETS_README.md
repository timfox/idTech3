# Enhanced Banner Assets Setup

## Overview
The enhanced engine supports professional PBR banner materials with multiple visual effects. This guide explains how to create the banner assets for the main menu.

## Required Assets

### 1. 3D Model: `models/mapobjects/banner/banner5.md3`
- **Format**: Quake 3 MD3 model
- **Purpose**: 3D banner model for main menu
- **Tools**: Created with modeling software like Blender + MD3 exporter
- **Size**: Approximately 50x50x5 units (scaled in code)
- **LOD**: Single LOD level for main menu use

### 2. Skin File: `models/mapobjects/banner/banner5.skin`
- **Purpose**: Maps textures to model surfaces
- **Format**: Standard Quake 3 skin file

```
tag_origin,*default
banner_main,textures/mapobjects/banner/banner_main
banner_trim,textures/mapobjects/banner/banner_trim
banner_logo,textures/mapobjects/banner/banner_logo
banner_glow,textures/mapobjects/banner/banner_glow
banner_metal,textures/mapobjects/banner/banner_metal
```

## Texture Assets (PBR Material System)

### Base Material Textures
All textures should be 512x512 TGA format with alpha channel where needed.

#### `banner_main_d.tga` - Diffuse/Albedo
- Main banner fabric/cloth texture
- Base color information
- RGB: Color, A: Opacity (if needed)

#### `banner_main_n.tga` - Normal Map
- Surface detail and bump mapping
- RGB: Normal vectors, A: Unused
- Blue channel should be ~128 for flat areas

#### `banner_trim_d.tga` - Trim Diffuse
- Metallic trim elements
- High contrast, reflective material

#### `banner_trim_n.tga` - Trim Normal
- Trim surface detail

#### `banner_trim_m.tga` - Metallic Map
- Black = dielectric, White = metallic
- Grayscale values for metallic surfaces

#### `banner_trim_r.tga` - Roughness Map
- Black = smooth/mirror, White = rough
- Controls reflection blur

### Emissive Textures

#### `banner_logo_d.tga` - Logo Diffuse
- Company/engine logo design
- Should be mostly transparent with logo visible

#### `banner_logo_e.tga` - Logo Emissive
- Glow mask for logo
- White = full glow, Black = no glow

### Effect Textures

#### `banner_glow_d.tga` - Glow Effect
- Soft glow texture around banner
- Radial gradient or soft cloud pattern

#### `banner_metal_d.tga` - Metal Base
- Base color for metallic elements

#### `banner_metal_n.tga` - Metal Normal
- Metal surface detail

#### `banner_metal_m.tga` - Metal Metallic
- Full white (255) for metallic

#### `banner_metal_r.tga` - Metal Roughness
- Low values (0-64) for polished metal

## Animation Textures (Optional)

### `banner_anim1.tga` through `banner_anim3.tga`
- Animated banner effects
- Used for flowing fabric or magical effects
- Same resolution as base textures

## Shader Features

The banner supports these advanced shader features:

### PBR Rendering
- Physically based materials
- Energy-conserving specular
- Metalness/roughness workflow

### Emissive Effects
- Pulsing logo glow
- Animated particle effects
- Dynamic lighting response

### Environmental Effects
- Reflection mapping
- Ambient occlusion
- Screen space effects

## Creation Workflow

### Step 1: Design the Banner
1. Sketch banner layout
2. Plan material zones (fabric, metal, logo)
3. Consider lighting and effects

### Step 2: Create 3D Model
1. Model banner geometry in Blender
2. UV unwrap for texture mapping
3. Assign materials to surfaces
4. Export as MD3 format

### Step 3: Create Textures
1. Create diffuse maps (base colors)
2. Generate normal maps (surface detail)
3. Create metallic/roughness maps
4. Design emissive effects
5. Test in engine with shaders

### Step 4: Test Integration
1. Place assets in correct directories
2. Test model loading
3. Verify shader compilation
4. Check performance impact

## Directory Structure

```
mods/mymod/
├── models/mapobjects/banner/
│   ├── banner5.md3          # 3D model
│   └── banner5.skin         # Texture mapping
├── textures/mapobjects/banner/
│   ├── banner_main_d.tga    # Main diffuse
│   ├── banner_main_n.tga    # Main normal
│   ├── banner_trim_d.tga    # Trim diffuse
│   ├── banner_trim_n.tga    # Trim normal
│   ├── banner_trim_m.tga    # Trim metallic
│   ├── banner_trim_r.tga    # Trim roughness
│   ├── banner_logo_d.tga    # Logo diffuse
│   ├── banner_logo_e.tga    # Logo emissive
│   ├── banner_glow_d.tga    # Glow effect
│   ├── banner_metal_d.tga   # Metal diffuse
│   ├── banner_metal_n.tga   # Metal normal
│   ├── banner_metal_m.tga   # Metal metallic
│   └── banner_metal_r.tga   # Metal roughness
└── shaders/
    └── banner.shader        # Shader definitions
```

## Fallback System

The engine includes automatic fallback systems:

### 3D Model Fallbacks
- Primary: `banner5.md3`
- Fallback 1: `cube.md3`
- Fallback 2: Grenade model
- Fallback 3: Health pack model
- Final: 2D animated banner

### Texture Fallbacks
- Missing PBR textures fall back to diffuse-only
- Missing normal maps use flat normals
- Missing emissive textures disable glow

## Performance Considerations

### Texture Memory
- Base textures: ~2MB (512x512xRGBA)
- Normal/metallic/roughness: ~6MB total
- Emissive effects: ~1MB

### Shader Complexity
- PBR materials: Medium performance impact
- Emissive effects: Low performance impact
- Animation effects: Low performance impact

### Optimization Tips
1. Use texture compression where possible
2. Combine similar materials
3. Use lower resolution for secondary elements
4. Test on target hardware

## Testing Commands

```
// Load and test banner
vid_restart
toggle r_showtris    // Show geometry
toggle r_shownormals // Show surface normals

// Shader debugging
r_showShaders 1      // Show active shaders
imagelist           // List loaded textures

// Performance monitoring
r_speeds 1          // Frame rate info
com_speeds 1        // System performance
```

## Integration Status

✅ **Model Loading**: Enhanced with multiple fallbacks
✅ **Shader System**: Full PBR material support
✅ **Texture Pipeline**: Streaming and compression ready
✅ **Animation System**: Timeline-based effects
✅ **Performance**: Optimized for main menu use
✅ **Fallback System**: Graceful degradation

The enhanced banner system is ready for professional asset creation!
