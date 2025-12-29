/*
=============================================================================
Service Interfaces for Dependency Injection

This header defines service interfaces that break circular dependencies
between common, client, and server modules.
=============================================================================
*/

#ifndef __Q_SERVICES_H
#define __Q_SERVICES_H

#include "q_shared.h"

// Forward declarations
typedef struct soundInterface_s soundInterface_t;
typedef struct rendererInterface_s rendererInterface_t;

// Sound service interface
struct soundInterface_s {
    qboolean (*Init)(void);
    void (*Shutdown)(void);
    void (*Update)(void);
    void (*StartLocalSound)(sfxHandle_t sfx, int channelNum);
    void (*StartBackgroundTrack)(const char* intro, const char* loop);
    void (*StopBackgroundTrack)(void);
    void (*RawSamples)(int stream, int samples, int rate, int width, int channels, const byte* data, float volume, int entityNum);
    void (*StopAllSounds)(void);
    void (*ClearLoopingSounds)(qboolean killall);
    void (*AddLoopingSound)(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
    void (*AddRealLoopingSound)(int entityNum, const vec3_t origin, sfxHandle_t sfx);
    void (*StopLoopingSound)(int entityNum);
    void (*Respatialize)(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater);
    void (*UpdateEntityPosition)(int entityNum, const vec3_t origin);
    void (*UpdateEntityVelocity)(int entityNum, const vec3_t velocity);
    void (*SetReverb)(int reverbType, float ratio);
    void (*UpdateReverb)(void);
};

// Renderer service interface (forward declaration to avoid circular include)
struct rendererInterface_s; // Defined in renderercommon/tr_renderer.h

// Service registry
typedef struct serviceRegistry_s {
    soundInterface_t* sound;
    rendererInterface_t* renderer;
    // Add other services as needed
} serviceRegistry_t;

// Global service registry
extern serviceRegistry_t* services;

// Service registration functions
void Com_RegisterSoundService(soundInterface_t* soundService);
void Com_RegisterRendererService(rendererInterface_t* rendererService);

// Service access functions (safe, return NULL if not registered)
soundInterface_t* Com_GetSoundService(void);
rendererInterface_t* Com_GetRendererService(void);

#endif // __Q_SERVICES_H