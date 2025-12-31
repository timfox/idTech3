# idTech3 Vulkan Renderer - Shader System

## Overview

The Vulkan renderer loads SPIR-V shader files at runtime. GLSL source files are compiled to SPIR-V format using `glslangValidator`.

## Shader File Locations

### Source Files (Development)
- **GLSL Source**: `src/shaders/*.glsl` - Edit these files for shader development
- **Compilation Script**: `src/shaders/compile_shaders.sh` - Compiles GLSL to SPIR-V

### Runtime Files (Game Loading)
The Vulkan renderer searches for `.spv` files in the following order:
1. Current mod directory: `mods/<modname>/shaders/`
2. Base game directory: `base/shaders/`
3. Other pak files and directories in the Quake file system

## Required Shaders

The Vulkan renderer requires these basic shaders:

- `dot_vert.spv` - Vertex shader for dot rendering
- `dot_frag.spv` - Fragment shader for dot rendering
- `color_vert.spv` - Vertex shader for solid color rendering
- `color_frag.spv` - Fragment shader for solid color rendering
- `fog_vert.spv` - Vertex shader for fog effects
- `fog_frag.spv` - Fragment shader for fog effects

## Creating New Shaders

1. **Write GLSL source file** (e.g., `myshader_vert.glsl`):
   ```glsl
   #version 450

   layout(location = 0) in vec4 position;
   layout(location = 1) in vec2 texCoord;

   void main() {
       gl_Position = position;
   }
   ```

2. **Compile to SPIR-V**:
   ```bash
   glslangValidator -V myshader_vert.glsl -o myshader_vert.spv
   ```

3. **Load in Vulkan code**:
   ```c
   VkShaderModule shader = vk_load_spirv_shader("myshader_vert");
   ```

## Compilation Script

Use the provided `compile_shaders.sh` script to automatically compile all `.glsl` files:

```bash
./compile_shaders.sh
```

This script will:
- Compile all `.glsl` files in `src/shaders/` to `.spv` format
- Copy compiled shaders to `mods/mymod/shaders/` for runtime loading
- Update both source and game directories automatically

### Manual Compilation

You can also compile individual shaders manually:

```bash
cd src/shaders
glslangValidator -V shader.glsl -o shader.spv
```

## Shader Requirements

- **GLSL Version**: `#version 450` (Vulkan compatible)
- **Input locations**: Use `layout(location = N)` for vertex attributes
- **Bindings**: Use `layout(binding = N)` for uniforms and samplers
- **Entry point**: `main()` function

## Example Shaders

### Basic Vertex Shader
```glsl
#version 450

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 v_texCoord;

void main() {
    gl_Position = position;
    v_texCoord = texCoord;
}
```

### Basic Fragment Shader
```glsl
#version 450

layout(location = 0) in vec2 v_texCoord;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(texSampler, v_texCoord);
}
```

## Troubleshooting

### Shader Loading Issues
- Check that `.spv` files exist in the correct directories
- Verify shader names match exactly (case-sensitive)
- Check console output for "fallback" messages

### Compilation Issues
- Ensure `glslangValidator` is installed (part of Vulkan SDK)
- Check GLSL syntax is valid for Vulkan
- Use `-V` flag for Vulkan target

### Runtime Issues
- Check Vulkan validation layer output
- Verify shader inputs/outputs match pipeline expectations
- Ensure proper descriptor set layouts

## Advanced Features

The Vulkan renderer supports advanced shader features:
- **Dynamic Rendering** (Vulkan 1.4)
- **Push Constants** for uniform data
- **Descriptor Sets** for textures and buffers
- **Specialization Constants** for compile-time configuration

See the Vulkan renderer source code for implementation details.