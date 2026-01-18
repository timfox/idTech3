/*
===========================================================================
Minimal g_local.h for ECS integration

This is a minimal header file that provides the basic game entity
definitions needed for ECS integration without conflicting with
existing game structures.
===========================================================================
*/

#include "../common/q_shared.h"
#include "g_public.h"

// Entity flags (complementing those in bg_public.h)
#define EF_DEAD				0x00000001		// entity is dead
#define EF_BOUNCE			0x00000002		// entity bounces

// Render flags
#define RF_GLOW				0x00000001		// entity glows

// Forward declarations to avoid conflicts
struct gclient_t;

// Minimal gentity_t definition for ECS integration
struct gentity_t {
    entityState_t s;              // Entity state (includes number field)
    entityShared_t r;             // Shared entity data (includes currentOrigin, currentAngles)
    qboolean inuse;               // Whether entity is in use
    int health;                   // Health value
    char *model;                  // Model name
    // Add other minimal fields as needed by ECS
};

// Basic constants needed for ECS
#define MAX_GENTITIES 1024
#define MAX_CLIENTS 128

// External references needed by ECS
extern struct gentity_t g_entities[MAX_GENTITIES];