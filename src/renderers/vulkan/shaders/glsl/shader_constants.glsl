// Shader constants and utility functions
// Include this file in shaders to avoid magic numbers and improve maintainability

#ifndef SHADER_CONSTANTS_GLSL
#define SHADER_CONSTANTS_GLSL

// Mathematical constants
#define PI 3.1415926535897932384626433832795
#define INV_PI 0.31830988618379067153776752674503
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define EPSILON 1e-5
#define EPSILON_LARGE 1e-3

// Color space constants (sRGB luminance weights)
const vec3 LUMA_WEIGHTS = vec3(0.2126, 0.7152, 0.0722);
const float LUMA_WEIGHT_R = 0.2126;
const float LUMA_WEIGHT_G = 0.7152;
const float LUMA_WEIGHT_B = 0.0722;

// Common color values
const vec3 COLOR_WHITE = vec3(1.0);
const vec3 COLOR_BLACK = vec3(0.0);
const vec4 COLOR_WHITE_A = vec4(1.0);
const vec4 COLOR_BLACK_A = vec4(0.0);

// Precision qualifiers for better performance
#ifdef GL_FRAGMENT_PRECISION_HIGH
    #define PRECISION_HIGHP highp
    #define PRECISION_MEDIUMP mediump
    #define PRECISION_LOWP lowp
#else
    #define PRECISION_HIGHP mediump
    #define PRECISION_MEDIUMP mediump
    #define PRECISION_LOWP lowp
#endif

// Utility functions
float luma(vec3 color) {
    return dot(color, LUMA_WEIGHTS);
}

vec3 toLuma(vec3 color) {
    return vec3(luma(color));
}

// Fast approximate functions
float fastSqrt(float x) {
    return sqrt(x);
}

float fastInverseSqrt(float x) {
    return inversesqrt(x);
}

// Optimized clamp that avoids branching
vec3 safeClamp(vec3 x, float minVal, float maxVal) {
    return max(vec3(minVal), min(vec3(maxVal), x));
}

float safeClamp(float x, float minVal, float maxVal) {
    return max(minVal, min(maxVal, x));
}

// Optimized smoothstep with better precision
float smoothStep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

#endif // SHADER_CONSTANTS_GLSL

