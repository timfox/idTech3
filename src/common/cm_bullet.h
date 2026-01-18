#ifndef __CM_BULLET_H__
#define __CM_BULLET_H__

#ifdef USE_BULLET
#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bullet physics integration for BSP collision
void CM_Bullet_Init(void);
void CM_Bullet_Shutdown(void);
void CM_Bullet_LoadMap(void);

// Enhanced tracing functions that can use Bullet physics
void CM_Bullet_Trace(trace_t *results, const vec3_t start, const vec3_t end,
                    const vec3_t mins, const vec3_t maxs, clipHandle_t model,
                    int brushmask, qboolean cylinder);
int CM_Bullet_PointContents(const vec3_t p, clipHandle_t model);

#ifdef __cplusplus
// Dynamic rigid body physics functions (C++ only)
int CM_Bullet_CreateRigidBody(float mass, btCollisionShape *shape,
                             const vec3_t position, const vec4_t rotation);
void CM_Bullet_DestroyRigidBody(int handle);
void CM_Bullet_UpdateRigidBody(int handle, const vec3_t position,
                              const vec4_t rotation, const vec3_t velocity);
void CM_Bullet_GetRigidBodyTransform(int handle, vec3_t position, vec4_t rotation);
void CM_Bullet_StepSimulation(float timeStep);

// Collision shape creation functions (C++ only)
btCollisionShape *CM_Bullet_CreateBoxShape(const vec3_t halfExtents);
btCollisionShape *CM_Bullet_CreateSphereShape(float radius);
btCollisionShape *CM_Bullet_CreateCapsuleShape(float radius, float height);
void CM_Bullet_DestroyShape(btCollisionShape *shape);
#endif

#ifdef __cplusplus
}
#endif

#endif // USE_BULLET

#endif // __CM_BULLET_H__