# RGB9E5 HDR Image Format Support

## Overview

idTech3++ now supports the RGB9E5 HDR (High Dynamic Range) image format, providing efficient storage of HDR textures while maintaining full compatibility with existing image formats.

## RGB9E5 Format Specification

RGB9E5 is a shared exponent format that stores 3 RGB values and 1 shared exponent in 32 bits total:

### Bit Layout
```
Bits:  31  30  29  28  27  26  25  24  23  22  21  20  19  18  17  16
       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
       |E4 |E3 |E2 |E1 |E0 |R8 |R7 |R6 |R5 |R4 |R3 |R2 |R1 |R0 |G8 |G7 |
       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

Bits:  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
       |G6 |G5 |G4 |G3 |G2 |G1 |G0 |B8 |B7 |B6 |B5 |B4 |B3 |B2 |B1 |B0 |
       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

### Field Descriptions
- **R[8:0]**: 9-bit red mantissa
- **G[8:0]**: 9-bit green mantissa
- **B[8:0]**: 9-bit blue mantissa
- **E[4:0]**: 5-bit shared exponent (biased by +15)

### Value Calculation
```
float_value = (mantissa / 511.0) * (2^(exponent - 15))
```

## File Format

RGB9E5 files use the `.rgb9e5` extension and have a simple header structure:

```c
typedef struct {
    char magic[4];      // "RGB9" - magic identifier
    int width;          // Image width in pixels
    int height;         // Image height in pixels
    int flags;          // Reserved for future use (set to 0)
} rgb9e5_header_t;
```

Followed by `width * height` RGB9E5 pixel values.

## Usage

### Loading RGB9E5 Textures
RGB9E5 textures are loaded automatically by the engine when files with the `.rgb9e5` extension are present:

```c
// The engine will automatically detect and load RGB9E5 files
image_t *hdr_texture = R_FindImageFile("textures/hdr/sky.rgb9e5", qfalse, qfalse, GL_CLAMP_TO_EDGE);
```

### Supported Renderers
- ✅ **Vulkan Renderer**: Full RGB9E5 support with optimal performance
- ✅ **OpenGL Renderer**: RGB9E5 support with fallback to float conversion
- ✅ **OpenGL ES**: RGB9E5 support for mobile platforms

## Benefits

### Storage Efficiency
- **32 bits per pixel** vs 96 bits (RGB float) or 128 bits (RGBA float)
- **75% memory reduction** compared to traditional HDR formats
- **Better texture cache utilization**

### Dynamic Range
- **Wide color gamut** support with shared exponent encoding
- **HDR rendering** capabilities for realistic lighting
- **Tone mapping** compatibility for advanced post-processing

### Performance
- **Reduced bandwidth** requirements
- **Smaller texture sizes** for better loading performance
- **GPU-friendly** compressed format

## Technical Details

### Precision Characteristics
- **Mantissa**: 9 bits per channel (511 levels)
- **Exponent**: 5 bits shared (-15 to +16 range)
- **Effective Range**: ~2^-15 to 2^16 (very wide dynamic range)
- **Relative Precision**: Higher precision for darker values

### Color Space
- **Linear color space** assumed
- **No gamma correction** applied during load/save
- **Tone mapping** should be applied in shaders

## Implementation

### Core Functions
```c
// Load RGB9E5 HDR image
void R_LoadRGB9E5(const char *name, unsigned char **pic, int *width, int *height);

// Save image as RGB9E5 HDR format
qboolean R_SaveRGB9E5(const char *name, const float *float_data, int width, int height);
```

### File Location
- **Source**: `src/renderers/renderercommon/tr_image_rgb9e5.c`
- **Headers**: `src/renderers/renderercommon/tr_public.h`
- **Integration**: Both OpenGL and Vulkan renderers

## Examples

### Creating RGB9E5 Textures
```c
// Example: Convert existing HDR float data to RGB9E5
float *hdr_float_data = /* your HDR float RGBA data */;
int width = 1024, height = 1024;

// Save as RGB9E5 format
if (R_SaveRGB9E5("textures/hdr/my_hdr_texture.rgb9e5", hdr_float_data, width, height)) {
    Com_Printf("HDR texture saved successfully\n");
}
```

### Shader Usage
```glsl
// Sample RGB9E5 texture in shader
uniform sampler2D hdrTexture;
vec4 hdrColor = texture(hdrTexture, texCoord);

// Apply tone mapping (example: simple Reinhard)
vec3 ldrColor = hdrColor.rgb / (hdrColor.rgb + vec3(1.0));
```

## Compatibility

### Engine Versions
- ✅ **idTech3++**: Full native support
- ❌ **Original idTech3**: Not supported (requires engine modification)
- ❌ **ioquake3**: Not supported

### Platform Support
- ✅ **Linux**: Full support
- ✅ **Windows**: Full support
- ✅ **macOS**: Full support
- ✅ **Android**: Full support (with Vulkan)

## Future Enhancements

### Planned Features
- **BC6H Fallback**: Automatic fallback to BC6H compression for unsupported hardware
- **Mipmap Generation**: Automatic mipmap generation for RGB9E5 textures
- **Compression Tools**: Built-in conversion tools for content creation
- **Format Validation**: Enhanced validation of RGB9E5 file integrity

### Research Areas
- **RGB9E5E5**: Extended format with separate exponents per channel
- **Lossless Variants**: Higher precision variants for critical content
- **Compression**: Further compression using entropy coding

## References

- **OpenGL Extensions**: `GL_EXT_texture_shared_exponent`
- **Vulkan Formats**: `VK_FORMAT_E5B9G9R9_UFLOAT_PACK32`
- **Shared Exponent Encoding**: Industry standard for HDR textures

---

*RGB9E5 HDR support brings modern high dynamic range rendering capabilities to idTech3++ while maintaining the engine's focus on performance and compatibility.*