#version 450
/*
 * Terrain vertex shader - displacement from heightmap.
 */
layout(location = 0) in vec2 inPosition;  /* x,z in [0,1] */

layout(location = 0) out vec2 outTexCoord;

layout(std140, binding = 0) uniform TerrainUBO {
    mat4 mvp;
    vec4 scale;   /* x,z = world size, y = height scale */
} ubo;

layout(binding = 1) uniform sampler2D heightmap;

void main() {
    vec2 uv = inPosition;
    float h = texture(heightmap, uv).r * ubo.scale.y;
    vec3 worldPos = vec3(inPosition.x * ubo.scale.x, h, inPosition.y * ubo.scale.z);
    gl_Position = ubo.mvp * vec4(worldPos, 1.0);
    outTexCoord = uv;
}
