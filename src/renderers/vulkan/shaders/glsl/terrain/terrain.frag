#version 450
/*
 * Terrain fragment shader - basic diffuse.
 */
layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D heightmap;
layout(binding = 1) uniform sampler2D diffuseMap;

void main() {
    vec4 diffuse = texture(diffuseMap, inTexCoord);
    outColor = vec4(diffuse.rgb, 1.0);
}
