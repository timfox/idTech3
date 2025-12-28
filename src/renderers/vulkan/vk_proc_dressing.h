/*
=============================================================================
Procedural Dressing System Header

Type definitions and constants for the procedural dressing system.
=============================================================================
*/

#ifndef __VK_PROC_DRESSING_H__
#define __VK_PROC_DRESSING_H__

#include "../../common/q_shared.h"

// Maximum number of procedural instances
#define VK_MAX_PROC_INSTANCES 65536

// Rule types
typedef enum {
    PROC_RULE_PAINT,    // Paint sphere rule
    PROC_RULE_VOLUME,   // AABB volume rule
    PROC_RULE_SPLINE    // Spline-based rule
} proc_rule_type_t;

// Procedural biome definition
typedef struct {
    char name[32];
    vec3_t tint;
    float scaleRange[2];      // Min/max scale multiplier
    float densityMultiplier;  // Density adjustment
    int materialIndex;        // Material/material to use
} proc_biome_t;

// Procedural rule definition
typedef struct {
    proc_rule_type_t type;
    union {
        struct { // Paint rule
            vec3_t center;   // Center position
            float radius;    // Paint radius
        } paint;
        struct { // Volume rule
            vec3_t mins;     // AABB min
            vec3_t maxs;     // AABB max
        } volume;
        struct { // Spline rule
            vec3_t start;    // Start point
            vec3_t end;      // End point
            float radius;    // Spline radius
        } spline;
    };
    float density;      // Instances per unit area/volume
    float jitter;       // Position randomization [0,1]
    int biomeId;        // Which biome to use
    int maxInstances;   // Maximum instances for this rule
} proc_rule_t;

// Procedural instance
typedef struct {
    float transform[16]; // Model matrix (4x4)
    int biomeId;         // Biome this instance belongs to
    vec4_t color;        // Tint color (RGBA)
} proc_instance_t;

#endif // __VK_PROC_DRESSING_H__