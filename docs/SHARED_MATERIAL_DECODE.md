# Shared material decode

`renderers/vulkan/shaders/glsl/surface_material_decode.glsl` defines the
canonical `SurfaceMaterial` boundary. Forward+, Deferred, G-buffer generation,
and WBOIT-compatible Forward+ lighting include this module.

Values cross the boundary as scene-linear base color/emissive, normalized
world-space normal, perceptual roughness, metallic, material AO, opacity,
clearcoat, clearcoat roughness, sheen, flags, shading model, lighting owner,
and lightmap identity. Clamping and normal fallback occur exactly once here.

`material_decode_validate` remains pending until matching GPU buffers are
captured from both paths; identical source inclusion alone is static evidence.
