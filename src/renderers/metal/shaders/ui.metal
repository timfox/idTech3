#include <metal_stdlib>
using namespace metal;

// Vertex structure for 2D UI rendering
struct UIVertex {
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
    float4 color [[attribute(2)]];
};

struct UIVertexOut {
    float4 position [[position]];
    float2 texCoord;
    float4 color;
};

// Uniform buffer for 2D projection matrix
struct UIUniforms {
    float4x4 projectionMatrix;
};

// Vertex shader for UI rendering
vertex UIVertexOut ui_vertex(UIVertex in [[stage_in]],
                            constant UIUniforms &uniforms [[buffer(1)]]) {
    UIVertexOut out;
    
    // Transform position using projection matrix
    float4 pos = float4(in.position.x, in.position.y, 0.0, 1.0);
    out.position = uniforms.projectionMatrix * pos;
    
    out.texCoord = in.texCoord;
    out.color = in.color;
    
    return out;
}

// Fragment shader for UI rendering
fragment float4 ui_fragment(UIVertexOut in [[stage_in]],
                           texture2d<float> texture [[texture(0)]],
                           sampler textureSampler [[sampler(0)]]) {
    float4 texColor = texture.sample(textureSampler, in.texCoord);
    return texColor * in.color;
}

