#version 460
#extension GL_EXT_scalar_block_layout : require

//===============================================================================
// Layered Material Vertex Shader
//
// Vertex shader for the layered material system with proper attribute handling.
//===============================================================================

//===============================================================================
// Shader Inputs
//===============================================================================

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in uint inMaterialID;

//===============================================================================
// Shader Outputs
//===============================================================================

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec3 outBitangent;
layout(location = 5) out flat uint outMaterialID;

//===============================================================================
// Push Constants
//===============================================================================

layout(push_constant, scalar) uniform PushConstants {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 normalMatrix;

    vec3 cameraPosition;
    float time;

    vec3 lightDirection;
    float lightIntensity;

    vec3 ambientColor;
    float exposure;
} pushConstants;

//===============================================================================
// Main Vertex Shader
//===============================================================================

void main() {
    // Transform position to world space
    vec4 worldPos = pushConstants.modelMatrix * vec4(inPosition, 1.0);
    outPosition = worldPos.xyz;

    // Transform to clip space
    gl_Position = pushConstants.projectionMatrix * pushConstants.viewMatrix * worldPos;

    // Transform normal to world space
    outNormal = normalize(mat3(pushConstants.normalMatrix) * inNormal);

    // Transform tangent and bitangent to world space
    outTangent = normalize(mat3(pushConstants.normalMatrix) * inTangent);
    outBitangent = normalize(mat3(pushConstants.normalMatrix) * inBitangent);

    // Pass through texture coordinates
    outTexCoord = inTexCoord;

    // Pass through material ID
    outMaterialID = inMaterialID;
}