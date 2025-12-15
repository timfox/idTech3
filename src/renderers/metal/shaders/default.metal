#include <metal_stdlib>
using namespace metal;

// Vertex input structure
struct VertexIn {
    float3 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
    float3 normal [[attribute(2)]];
    float4 color [[attribute(3)]];
};

// Vertex output structure
struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
    float3 normal;
    float4 color;
};

// Uniform buffer structure
struct Uniforms {
    float4x4 modelViewProjection;
    float4x4 modelView;
    float4x4 normalMatrix;
};

// Vertex shader
vertex VertexOut vertex_main(VertexIn in [[stage_in]],
                              constant Uniforms &uniforms [[buffer(0)]]) {
    VertexOut out;
    out.position = uniforms.modelViewProjection * float4(in.position, 1.0);
    out.texCoord = in.texCoord;
    out.normal = normalize((uniforms.normalMatrix * float4(in.normal, 0.0)).xyz);
    out.color = in.color;
    return out;
}

// Fragment shader
fragment float4 fragment_main(VertexOut in [[stage_in]],
                             texture2d<float> texture [[texture(0)]],
                             sampler textureSampler [[sampler(0)]]) {
    float4 texColor = texture.sample(textureSampler, in.texCoord);
    return texColor * in.color;
}

