# NaturalVision Evolved Shader Techniques

## Overview
This document outlines valuable shader techniques from NaturalVision Evolved (GTA 5 mod) that can be adapted for id Tech 3 RTX renderer.

## Key Techniques

### 1. Multi-Octave Bloom (NVE_Bloom.fx)
**Technique:** Hierarchical bloom with multiple octaves and biquadratic sampling

**Key Features:**
- Multi-pass downsampling (1024x512 → 512x256 → 256x128 → 128x64 → 64x32 → 32x16)
- Biquadratic texture sampling for smooth upsampling
- Configurable octave weights (6 octaves)
- Multiple falloff types (exponential, linear, custom curves)
- Threshold-based extraction with curve control
- Color tinting per octave
- Daylight time modulation
- Scanline filter integration

**Adaptation Notes:**
- Can be ported to compute shaders for Vulkan
- Use image downsampling instead of render targets
- Integrate with existing bloom pipeline

### 2. Advanced Color Grading (NVE_Color.fx)
**Technique:** Comprehensive color correction pipeline

**Key Features:**
- **Preset System:** Default, Color Boost, Warm, Vintage, Retro, VHS, Old Cam, Matrix, Noir
- **Basic Controls:**
  - Gamma correction
  - Brightness/Contrast/Saturation
  - Chromatic Aberration (with lens distortion)
  - Bleach Bypass effect
- **Color Temperature:** Kelvin-based temperature adjustment (1000-40000K)
- **Levels:** Input/Output black/white points
- **Color Balance:** Separate shadows/highlights tinting
- **Channel Mixer:** Per-channel color lerping
- **VHS Filter:** Temporal noise, line artifacts, chromatic aberration
- **Lens Distortion:** Barrel/pincushion distortion for "Old Cam" preset

**Adaptation Notes:**
- Port to compute shader or fragment shader
- Integrate with existing tone mapping
- Add CVARs for all controls
- Can enhance Q2RTX/Q3RTX color grading

### 3. CAS (Contrast Adaptive Sharpening) (CAS.fx)
**Technique:** AMD FidelityFX CAS implementation

**Key Features:**
- Adaptive sharpening based on local contrast
- 3x3 neighborhood sampling
- Soft min/max computation
- Contrast adaptation parameter
- Sharpening intensity control
- Optimized with texture gather operations

**Adaptation Notes:**
- Already available in FidelityFX, but good reference implementation
- Can be used as post-process sharpening
- Integrate with FSR pipeline

### 4. Procedural Sky with Milky Way (NVE_Sky.fxh)
**Technique:** Procedural starfield and Milky Way rendering

**Key Features:**
- **Starfield:**
  - Hash-based star generation
  - Variable star sizes and brightness
  - Color variance (white, blue, red stars)
  - Density modulation (core region multiplier)
  - Sharpness control
- **Milky Way:**
  - Procedural galaxy rendering
  - 3D noise texture sampling
  - Core and base regions with different tints
  - Width, length, taper controls
  - Rotation and positioning
  - Texture-based sticker overlay
  - Noise modulation

**Adaptation Notes:**
- Can enhance Q2RTX/Q3RTX physical sky
- Use for space environment rendering
- Integrate with ray-traced sky

### 5. Volumetric Clouds (NVE_VolumetricClouds.fx)
**Technique:** Ray-marched volumetric cloud rendering

**Key Features:**
- Multi-octave noise sampling
- Density and coverage controls
- Shadow computation with early exit
- Distortion and bump mapping
- Earth shadow (ground occlusion)
- Atmospheric scattering integration
- Powder effect (light scattering through clouds)
- Henyey-Greenstein phase function
- Multiple detail scales
- Time-based animation

**Adaptation Notes:**
- Very complex, but excellent reference
- Can inform Q2RTX god rays implementation
- Useful for atmospheric rendering
- May be too expensive for real-time, but good for reference

### 6. Vignette (NVE_Vignette.fx)
**Technique:** Advanced vignette with multiple shapes

**Key Features:**
- Multiple vignette shapes (circular, rectangular, custom)
- Smooth falloff curves
- Color tinting
- Edge softness control
- Center point adjustment

**Adaptation Notes:**
- Simple post-process effect
- Easy to integrate
- Can enhance cinematic feel

### 7. Debanding (Deband.fx)
**Technique:** Dithering and debanding to reduce color banding

**Key Features:**
- Ordered dithering
- Noise-based dithering
- Threshold-based application
- Preserves detail while reducing banding

**Adaptation Notes:**
- Useful for HDR rendering
- Can reduce banding in path-traced images
- Simple compute shader implementation

## Integration Priority

### High Priority
1. **CAS Sharpening** - Quick win, improves image quality
2. **Advanced Color Grading** - Enhances visual quality significantly
3. **Multi-Octave Bloom** - Better than basic bloom

### Medium Priority
4. **Procedural Sky/Milky Way** - Enhances sky rendering
5. **Vignette** - Simple but effective
6. **Debanding** - Reduces artifacts

### Low Priority
7. **Volumetric Clouds** - Very complex, may be too expensive

## Implementation Notes

### Shader Language Conversion
- NVE uses HLSL (ReShade format)
- Need to convert to GLSL for Vulkan
- ReShade uses specific conventions (ReShade.fxh, ReShadeUI.fxh)
- Need to adapt uniform declarations
- Replace ReShade-specific functions with Vulkan equivalents

### Performance Considerations
- Bloom: Multi-pass but efficient with proper downsampling
- Color Grading: Single pass, very fast
- CAS: Single pass, optimized
- Sky: Procedural, fast
- Volumetric Clouds: Expensive, use sparingly

### Integration Points
- **Post-Processing Pipeline:** Color grading, CAS, vignette, debanding
- **Bloom Pipeline:** Multi-octave bloom
- **Sky System:** Procedural sky/Milky Way
- **Atmospheric:** Volumetric clouds (if performance allows)

## Code Structure

### Recommended File Organization
```
src/renderers/vulkan/postprocess/
  - vk_bloom_nve.cpp/h      - Multi-octave bloom
  - vk_color_grading_nve.cpp/h - Color grading
  - vk_cas.cpp/h            - CAS sharpening
  - vk_vignette.cpp/h       - Vignette
  - vk_deband.cpp/h         - Debanding

src/renderers/vulkan/sky/
  - vk_sky_procedural.cpp/h - Procedural sky/Milky Way

shaders/glsl/postprocess/
  - bloom_nve.comp          - Multi-octave bloom
  - color_grading_nve.comp   - Color grading
  - cas.comp                - CAS sharpening
  - vignette.comp           - Vignette
  - deband.comp             - Debanding

shaders/glsl/sky/
  - sky_procedural.comp     - Procedural sky
```

## CVARs to Add

### Bloom
- `r_bloom_nve_enable` - Enable NVE-style bloom
- `r_bloom_nve_intensity` - Bloom intensity
- `r_bloom_nve_threshold` - Extraction threshold
- `r_bloom_nve_octaves` - Number of octaves (1-6)

### Color Grading
- `r_color_preset` - Preset selector
- `r_color_gamma` - Gamma correction
- `r_color_brightness` - Brightness
- `r_color_contrast` - Contrast
- `r_color_saturation` - Saturation
- `r_color_temperature` - Color temperature (Kelvin)
- `r_color_ca` - Chromatic aberration strength

### CAS
- `r_cas_enable` - Enable CAS
- `r_cas_contrast` - Contrast adaptation
- `r_cas_sharpening` - Sharpening intensity

### Sky
- `r_sky_stars_density` - Star density
- `r_sky_milky_way_enable` - Enable Milky Way
- `r_sky_milky_way_intensity` - Milky Way intensity

## References
- NVE Bloom: `reference/NaturalVisionEvolvedGTA5/ReShade Shaders/reshade-shaders/Shaders/NVE_Bloom.fx`
- NVE Color: `reference/NaturalVisionEvolvedGTA5/ReShade Shaders/reshade-shaders/Shaders/NVE_Color.fx`
- CAS: `reference/NaturalVisionEvolvedGTA5/ReShade Shaders/reshade-shaders/Shaders/CAS.fx`
- Sky: `reference/NaturalVisionEvolvedGTA5/ReShade Shaders/reshade-shaders/Shaders/NVE/NVE_Sky.fxh`
- Clouds: `reference/NaturalVisionEvolvedGTA5/ReShade Shaders/reshade-shaders/Shaders/NVE_VolumetricClouds.fx`
