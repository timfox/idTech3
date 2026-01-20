# Layered Materials System

## Overview

The Layered Materials System extends idTech3++'s traditional shader system with advanced material layering capabilities, procedural texture generation, and modern Physically-Based Rendering (PBR) workflows. This system enables artists and developers to create complex, realistic materials through layer composition and procedural techniques.

## Architecture

### Material Layers
Materials are composed of multiple layers that blend together using various compositing modes:

```c
typedef struct {
    char name[64];
    qboolean enabled;

    // Base properties
    materialBlendMode_t blendMode;    // OPAQUE, ALPHA, ADDITIVE, etc.
    float opacity;
    vec2_t uvOffset;
    vec2_t uvScale;
    float uvRotation;

    // Texture maps (traditional approach)
    char diffuseMap[MAX_QPATH];
    char normalMap[MAX_QPATH];
    char specularMap[MAX_QPATH];
    char metallicMap[MAX_QPATH];
    char roughnessMap[MAX_QPATH];
    char emissiveMap[MAX_QPATH];
    char heightMap[MAX_QPATH];
    char occlusionMap[MAX_QPATH];

    // Procedural alternatives
    proceduralType_t diffuseProcedural;
    proceduralType_t normalProcedural;
    proceduralType_t specularProcedural;

    // PBR Material properties
    vec3_t baseColor;
    float metallic;
    float roughness;
    float emissiveIntensity;
    float normalStrength;
    float heightScale;

    // Animation properties
    float scrollSpeedU, scrollSpeedV;
    float rotationSpeed;
    float waveFrequency, waveAmplitude;

    // Custom parameters
    materialParameter_t parameters[MAX_MATERIAL_PARAMETERS];
    int numParameters;
} materialLayer_t;
```

### Blend Modes
The system supports comprehensive blending operations:

- **OPAQUE**: No blending, fully opaque
- **ALPHA**: Standard alpha blending
- **ADDITIVE**: Additive blending (glows, particles)
- **MULTIPLY**: Multiply blending (shadows, dirt)
- **SCREEN**: Screen blending (highlights)
- **OVERLAY**: Overlay blending (textures)
- **SOFT_LIGHT**: Soft light blending
- **HARD_LIGHT**: Hard light blending
- **DIFFERENCE**: Difference blending
- **EXCLUSION**: Exclusion blending
- **COLOR_DODGE**: Color dodge
- **COLOR_BURN**: Color burn
- **NORMAL_MAP**: Normal map blending
- **HEIGHT_MAP**: Height map blending
- **MASK**: Layer masking
- **SCREEN**: Screen blending

## Procedural Textures

### Available Procedural Types

#### **Noise-Based Textures**
- **PERLIN**: Classic Perlin noise with fractal octaves
- **SIMPLEX**: Improved Simplex noise (currently uses Perlin fallback)
- **VORONOI**: Cellular/Voronoi noise patterns
- **FRACTAL**: Multi-octave fractal noise
- **RIDGED**: Ridged multifractal (mountain-like terrain)

#### **Pattern-Based Textures**
- **CHECKERBOARD**: Classic checkerboard pattern
- **BRICK**: Brick wall pattern with mortar lines
- **GRADIENT**: Radial gradient from center

#### **Natural Effects**
- **MARBLE**: Marble texture with swirling patterns
- **WOOD**: Wood grain with concentric rings
- **TERRAIN**: Terrain heightmap using ridged noise
- **CLOUDS**: Soft cloud formations
- **SMOKE**: Smoke/cloud effects with movement

#### **Dynamic Effects**
- **WAVE**: Sine wave interference patterns
- **WATER**: Animated water surface
- **FIRE**: Flame effect with upward bias
- **TURBULENCE**: Distorted noise field
- **CRACKED**: Dried mud cracking effect

### Procedural Parameters
```c
typedef struct {
    proceduralType_t type;
    int octaves;           // Fractal detail levels
    float frequency;       // Base frequency
    float amplitude;       // Base amplitude
    float lacunarity;      // Frequency multiplier per octave
    float persistence;     // Amplitude multiplier per octave
    vec3_t offset;         // Position offset (Z can be time)
    float scale;           // Overall scale
    int seed;             // Random seed
} proceduralParams_t;
```

## Material Compilation

### Shader Generation
The system automatically generates GLSL shaders from layered material definitions:

1. **Vertex Shader**: Handles UV transformations, animations, and vertex processing
2. **Fragment Shader**: Implements layer blending, PBR lighting, and procedural evaluation
3. **Uniform Setup**: Configures shader parameters and texture bindings

### PBR Implementation
- **Metallic/Roughness Workflow**: Industry-standard PBR
- **Image-Based Lighting**: Environment reflections
- **Subsurface Scattering**: For skin, wax, etc.
- **Anisotropic Materials**: Hair, brushed metal

### Animation Support
- **UV Scrolling**: Texture animation
- **Rotation**: Spinning textures
- **Wave Deformation**: Rippling effects
- **Time-Based Parameters**: Animated material properties

## Usage Examples

### Creating a Complex Material

```c
// Create base material
layeredMaterial_t* material = Material_Create("complex_wall");

// Add base layer (diffuse)
int baseLayer = Material_AddLayer(material, "base_diffuse");
materialLayer_t* layer = &material->layers[baseLayer];
Q_strncpyz(layer->diffuseMap, "textures/wall/brick.tga", MAX_QPATH);
layer->metallic = 0.1f;
layer->roughness = 0.8f;

// Add dirt layer (procedural)
int dirtLayer = Material_AddLayer(material, "dirt_overlay");
layer = &material->layers[dirtLayer];
layer->diffuseProcedural = PROC_FRACTAL;
layer->blendMode = BLEND_MULTIPLY;
layer->opacity = 0.3f;
layer->uvScale[0] = layer->uvScale[1] = 4.0f;

// Add moss layer (animated)
int mossLayer = Material_AddLayer(material, "moss_growth");
layer = &material->layers[mossLayer];
layer->diffuseProcedural = PROC_CLOUDS;
layer->blendMode = BLEND_OVERLAY;
layer->opacity = 0.2f;
layer->scrollSpeedV = 0.01f; // Slow upward growth

// Configure global properties
material->usePBR = qtrue;
material->useIBL = qtrue;
material->depthWrite = qtrue;
material->depthTest = qtrue;
```

### Procedural Texture Generation

```c
// Generate a terrain heightmap
proceduralParams_t terrainParams = {
    .type = PROC_TERRAIN,
    .octaves = 8,
    .frequency = 0.005f,
    .amplitude = 1.0f,
    .persistence = 0.4f,
    .lacunarity = 2.0f,
    .scale = 1.0f,
    .seed = 42
};

float* heightmap = (float*)malloc(512 * 512 * sizeof(float));
Procedural_GenerateTexture(512, 512, 1, &terrainParams, heightmap);
```

### Material Instancing

```c
// Create material instance for runtime variation
layeredMaterial_t* baseMaterial = Material_Load("materials/wall.mat");
materialInstance_t* instance = MaterialInstance_Create(baseMaterial);

// Override roughness for weathered version
materialParameter_t roughnessParam = {
    .name = "roughness",
    .type = PARAM_FLOAT,
    .value.f = 0.95f
};
MaterialInstance_SetParameter(instance, "roughness", &roughnessParam);

// Compile for rendering
int shaderIndex = MaterialInstance_Compile(instance);
```

## File Format

### Material Files (.mat)
Materials are stored in a custom text format:

```
material "complex_wall"
{
    version 1
    doubleSided false
    translucent false
    cullMode CT_FRONT_SIDED
    usePBR true
    useIBL true

    layer "base_diffuse"
    {
        enabled true
        blendMode BLEND_OPAQUE
        opacity 1.0
        diffuseMap "textures/wall/brick.tga"
        metallic 0.1
        roughness 0.8
    }

    layer "dirt_overlay"
    {
        enabled true
        blendMode BLEND_MULTIPLY
        opacity 0.3
        diffuseProcedural PROC_FRACTAL
        uvScale 4.0 4.0
    }
}
```

## Integration with Renderer

### Vulkan Support
- **Descriptor Management**: Automatic descriptor set allocation
- **Pipeline Generation**: Dynamic pipeline creation for layered materials
- **Memory Management**: Efficient texture streaming and caching
- **Shader Compilation**: Runtime GLSL to SPIR-V conversion

### OpenGL Support
- **Shader Generation**: Compatible GLSL shader generation
- **Texture Units**: Efficient multi-texture management
- **UBO Support**: Uniform buffer objects for material parameters
- **Fallback Paths**: Graceful degradation for older hardware

## Performance Considerations

### Optimization Strategies
- **Layer Culling**: Skip invisible or zero-opacity layers
- **Texture Atlasing**: Combine small textures into atlases
- **Mipmap Generation**: Automatic mipmap creation for procedural textures
- **Shader Caching**: Reuse compiled shaders across similar materials
- **LOD System**: Reduce layer count at distance

### Memory Management
- **Texture Streaming**: Load/unload textures based on usage
- **Instance Pooling**: Reuse material instances
- **Parameter Caching**: Cache computed parameter values
- **Garbage Collection**: Automatic cleanup of unused materials

## Advanced Features

### Material Parameters
Materials support custom parameters for runtime control:

```c
materialParameter_t damageParam = {
    .name = "damage_amount",
    .type = PARAM_FLOAT,
    .value.f = 0.0f,
    .isAnimated = qfalse
};
Material_SetParameter(material, "damage_amount", &damageParam);
```

### Animation System
- **Keyframe Animation**: Time-based parameter interpolation
- **Procedural Animation**: Noise-driven parameter variation
- **State-Based Animation**: Animation triggered by game state

### Material Inheritance
- **Base Materials**: Define common properties in base materials
- **Material Overrides**: Instance-specific parameter overrides
- **Material Libraries**: Reusable material components

## Tools and Utilities

### Material Editor (Planned)
- **Visual Layer Editor**: Drag-and-drop layer composition
- **Real-time Preview**: Live material preview in editor
- **Parameter Tweaking**: Interactive parameter adjustment
- **Shader Debugging**: Visual shader compilation feedback

### Command Line Tools
```bash
# Convert material formats
material_converter input.mat output.gltf

# Optimize materials for target hardware
material_optimizer input.mat --target vulkan --quality high

# Generate material thumbnails
material_thumbnail input.mat output.png
```

## Future Enhancements

### Research Areas
- **Material Graphs**: Node-based material editor
- **Procedural Materials**: Fully procedural material generation
- **Material Instancing**: GPU-accelerated material variation
- **Material Baking**: Precompute complex material effects
- **Material LOD**: Level-of-detail material system

### Integration Plans
- **GLTF Support**: Import materials from GLTF files
- **USD Integration**: Universal Scene Description support
- **Material Libraries**: Shared material repositories
- **Collaborative Editing**: Multi-user material editing

## References

- **Implementation**: `src/common/material_layer.h`, `src/common/material_layer.c`
- **Examples**: `materials/` directory
- **Tools**: `tools/material_*` utilities
- **Documentation**: `docs/LAYERED_MATERIALS.md`

---

*The Layered Materials System brings modern material authoring capabilities to idTech3++, enabling artists to create complex, realistic materials with procedural techniques and advanced blending operations.*