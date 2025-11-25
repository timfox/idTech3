// Ray tracing defines and constants

#define RT_MAX_RECURSION_DEPTH 4
#define RT_SHADOW_RAY_INDEX 1
#define RT_REFLECTION_RAY_INDEX 2
#define RT_AO_RAY_INDEX 3

// Material flags
#define MATERIAL_OPAQUE 0
#define MATERIAL_TRANSPARENT 1
#define MATERIAL_EMISSIVE 2
#define MATERIAL_METALLIC 4
#define MATERIAL_ROUGH 8

// Light types
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2
#define LIGHT_TYPE_AREA 3

// Ray types
#define RAY_TYPE_PRIMARY 0
#define RAY_TYPE_SHADOW 1
#define RAY_TYPE_REFLECTION 2
#define RAY_TYPE_REFRACTION 3
#define RAY_TYPE_AO 4

// Constants
#define PI 3.14159265359
#define INV_PI 0.31830988618
#define EPSILON 0.0001
#define MAX_DISTANCE 10000.0

// PBR constants
#define MIN_ROUGHNESS 0.04
#define MAX_ROUGHNESS 1.0
#define MIN_METALLIC 0.0
#define MAX_METALLIC 1.0

// AO constants
#define AO_NUM_SAMPLES 16
#define AO_RADIUS 1.0
#define AO_MAX_DISTANCE 2.0
#define AO_BIAS 0.01
#define AO_POWER 2.0

// MAO constants
#define MAO_NUM_BOUNCES 2
#define MAO_NUM_SAMPLES_PER_BOUNCE 8
