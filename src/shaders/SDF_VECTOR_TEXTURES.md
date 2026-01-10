# Signed Distance Field Vector Texture Rendering

## Overview

This implementation provides high-quality rendering of vector art (text, UI elements, decals) using signed distance fields stored in low-resolution textures. The technique allows crisp rendering at any magnification level while using minimal texture memory.

## Shaders

### `sdf_vector_frag.glsl` / `sdf_vector_vert.glsl`
Full-featured implementation with:
- Alpha testing with configurable threshold (default 0.5)
- Soft edge antialiasing with adaptive screen-space derivatives
- Outlining with configurable width and color
- Outer glow / drop shadows
- Multi-channel SDF support for sharp corners
- Optional base color texture

### `sdf_vector_simple_frag.glsl`
Simplified version with minimal features:
- Basic alpha testing
- Soft edges
- Outlining
- Outer glow / drop shadows

## Usage

### Basic Setup

1. **Generate SDF Texture**: Use the compute shader `sdf_generate.comp` to convert a high-resolution binary image (e.g., 4096x4096) into a low-resolution SDF texture (e.g., 64x64).

2. **Store Distance Field**: The signed distance field should be stored in the alpha channel of an 8-bit texture:
   - `0.0` = maximum negative distance (inside)
   - `0.5` = edge position
   - `1.0` = maximum positive distance (outside)

3. **Render**: Use the SDF shader with the generated texture.

### Uniform Parameters

#### Alpha Testing
```glsl
float alphaThreshold = 0.5;  // Edge position
bool useAlphaTest = true;    // Use binary alpha test
```

#### Soft Edges / Antialiasing
```glsl
bool useSoftEdges = true;
float softEdgeMin = 0.45;    // Start of soft region
float softEdgeMax = 0.55;    // End of soft region
bool useAdaptiveAA = true;   // Use screen-space derivatives
```

#### Outlining
```glsl
bool useOutline = true;
float outlineMinValue0 = 0.48;  // Inner threshold
float outlineMinValue1 = 0.49;  // Inner fade start
float outlineMaxValue0 = 0.51;  // Outer fade end
float outlineMaxValue1 = 0.52;  // Outer threshold
vec4 outlineColor = vec4(0.0, 0.0, 0.0, 1.0);  // Black outline
```

#### Outer Glow / Drop Shadow
```glsl
bool useOuterGlow = true;
vec2 glowUVOffset = vec2(0.01, 0.01);  // Shadow offset
float outerGlowMinDValue = 0.0;        // Glow start
float outerGlowMaxDValue = 0.4;         // Glow end
vec4 outerGlowColor = vec4(0.0, 0.0, 0.0, 0.5);  // Semi-transparent shadow
```

#### Multi-Channel SDF (Sharp Corners)
```glsl
bool useMultiChannel = true;
int channelCount = 2;  // Use red and alpha channels
```

When using multiple channels, the shader performs a logical AND operation:
- Channel 1 (alpha): Primary distance field
- Channel 2 (red): Secondary edge for sharp corners
- Channel 3 (green): Tertiary edge
- Channel 4 (blue): Quaternary edge

## Examples

### Simple Alpha Test (No Shader Required)
For the simplest case, you can use hardware alpha testing with threshold 0.5:
```glsl
// In fragment shader
float dist = texture(sdfTexture, texCoord).a;
if (dist < 0.5) discard;
```

### Soft Edges
```glsl
float dist = texture(sdfTexture, texCoord).a;
float alpha = smoothstep(0.45, 0.55, dist);
fragColor.a *= alpha;
```

### Outlined Text
```glsl
float dist = texture(sdfTexture, texCoord).a;
vec4 color = baseColor;

// Draw outline in distance range [0.48, 0.52]
if (dist >= 0.48 && dist <= 0.52) {
    float factor = smoothstep(0.48, 0.49, dist) * 
                   smoothstep(0.52, 0.51, dist);
    color = mix(color, outlineColor, factor);
}

// Draw main shape
color.a *= smoothstep(0.45, 0.55, dist);
```

### Drop Shadow
```glsl
float dist = texture(sdfTexture, texCoord).a;
float shadowDist = texture(sdfTexture, texCoord + shadowOffset).a;

// Draw shadow
vec4 shadow = shadowColor * smoothstep(0.0, 0.4, shadowDist);

// Draw main shape
vec4 main = baseColor * smoothstep(0.45, 0.55, dist);

// Composite
fragColor = mix(shadow, main, step(0.5, dist));
```

## Performance Notes

- **Memory**: A 64x64 SDF texture can represent vector art that would require 4096x4096 in traditional formats (64x memory savings)
- **Performance**: Minimal overhead - typically 2-5 additional shader instructions
- **Compatibility**: Works on all modern GPUs, including those without programmable shaders (using alpha testing)
