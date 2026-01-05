#ifdef USE_BULLET

#include "physics_bullet.h"
#include "ecs.h"
#include "ecs_components.h"
#include "ecs_systems.h"
#include "cm_bullet.h"

// Bullet includes
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btIDebugDraw.h>

// Internal physics world state
static btBroadphaseInterface *g_broadphase = nullptr;
static btDefaultCollisionConfiguration *g_config = nullptr;
static btCollisionDispatcher *g_dispatcher = nullptr;
static btSequentialImpulseConstraintSolver *g_solver = nullptr;
static btDiscreteDynamicsWorld *g_world = nullptr;

static qboolean g_physicsInitialized = qfalse;

// Body handle management (simple array for now)
#define MAX_PHYSICS_BODIES 1024
static btRigidBody *g_bodyHandles[MAX_PHYSICS_BODIES];
static int g_nextBodyHandle = 1; // Start at 1, 0 = invalid

// Collision callback
static physicsCollisionCallback_t g_collisionCallback = nullptr;
static void *g_collisionUserData = nullptr;

// Debug drawing
class PhysicsDebugDraw : public btIDebugDraw {
public:
    void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override {
        // TODO: Implement debug line drawing
        (void)from; (void)to; (void)color;
    }

    void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB,
                         btScalar distance, int lifeTime, const btVector3& color) override {
        (void)PointOnB; (void)normalOnB; (void)distance; (void)lifeTime; (void)color;
    }

    void reportErrorWarning(const char* warningString) override {
        Com_Printf("Bullet Physics Warning: %s\n", warningString);
    }

    void draw3dText(const btVector3& location, const char* textString) override {
        (void)location; (void)textString;
    }

    void setDebugMode(int debugMode) override {
        m_debugMode = debugMode;
    }

    int getDebugMode() const override {
        return m_debugMode;
    }

private:
    int m_debugMode = 0;
};

static PhysicsDebugDraw *g_debugDraw = nullptr;

/*
================
Physics_Init
================
*/
physicsResult_t Physics_Init(void) {
    if (g_physicsInitialized) {
        return PHYSICS_OK;
    }

    // Initialize Bullet components
    g_broadphase = new btDbvtBroadphase();
    g_config = new btDefaultCollisionConfiguration();
    g_dispatcher = new btCollisionDispatcher(g_config);
    g_solver = new btSequentialImpulseConstraintSolver();
    g_world = new btDiscreteDynamicsWorld(g_dispatcher, g_broadphase, g_solver, g_config);

    // Set default gravity
    g_world->setGravity(btVector3(0.0f, 0.0f, -800.0f)); // Quake units

    // Initialize BSP collision
    CM_Bullet_Init();

    // Initialize debug drawing
    g_debugDraw = new PhysicsDebugDraw();
    g_world->setDebugDrawer(g_debugDraw);

    // Initialize body handle array
    memset(g_bodyHandles, 0, sizeof(g_bodyHandles));

    g_physicsInitialized = qtrue;
    Com_Printf("Bullet Physics system initialized\n");

    return PHYSICS_OK;
}

/*
================
Physics_Shutdown
================
*/
void Physics_Shutdown(void) {
    if (!g_physicsInitialized) {
        return;
    }

    // Clean up all bodies
    for (int i = 1; i < MAX_PHYSICS_BODIES; i++) {
        if (g_bodyHandles[i]) {
            if (g_bodyHandles[i]->getMotionState()) {
                delete g_bodyHandles[i]->getMotionState();
            }
            if (g_bodyHandles[i]->getCollisionShape()) {
                delete g_bodyHandles[i]->getCollisionShape();
            }
            g_world->removeRigidBody(g_bodyHandles[i]);
            delete g_bodyHandles[i];
            g_bodyHandles[i] = nullptr;
        }
    }

    // Clean up Bullet components
    if (g_world) delete g_world;
    if (g_solver) delete g_solver;
    if (g_dispatcher) delete g_dispatcher;
    if (g_config) delete g_config;
    if (g_broadphase) delete g_broadphase;
    if (g_debugDraw) delete g_debugDraw;

    g_world = nullptr;
    g_solver = nullptr;
    g_dispatcher = nullptr;
    g_config = nullptr;
    g_broadphase = nullptr;
    g_debugDraw = nullptr;

    // Shutdown BSP collision
    CM_Bullet_Shutdown();

    g_physicsInitialized = qfalse;
}

/*
================
Physics_IsInitialized
================
*/
qboolean Physics_IsInitialized(void) {
    return g_physicsInitialized;
}

/*
================
CreateCollisionShape
================
*/
static btCollisionShape* CreateCollisionShape(physicsShapeType_t type, const vec3_t dimensions) {
    switch (type) {
        case PHYSICS_SHAPE_BOX:
            return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));

        case PHYSICS_SHAPE_SPHERE:
            return new btSphereShape(dimensions[0]);

        case PHYSICS_SHAPE_CAPSULE:
            return new btCapsuleShape(dimensions[0], dimensions[1]);

        case PHYSICS_SHAPE_CONVEX_HULL:
            // Placeholder
            return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));

        case PHYSICS_SHAPE_MESH:
            // Placeholder
            return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));

        case PHYSICS_SHAPE_COMPOUND:
            return new btCompoundShape();

        default:
            return new btBoxShape(btVector3(1.0f, 1.0f, 1.0f));
    }
}

/*
================
Physics_CreateBody
================
*/
int Physics_CreateBody(const physicsBodyParams_t *params) {
    if (!g_physicsInitialized || !params) {
        return 0;
    }

    // Find free handle
    int handle = 0;
    for (int i = 1; i < MAX_PHYSICS_BODIES; i++) {
        if (!g_bodyHandles[i]) {
            handle = i;
            break;
        }
    }

    if (!handle) {
        Com_Printf("Physics_CreateBody: No free body handles\n");
        return 0;
    }

    // Create collision shape
    btCollisionShape *shape = CreateCollisionShape(params->shapeType, params->shapeDimensions);
    if (!shape) {
        return 0;
    }

    // Calculate inertia
    btVector3 localInertia(0, 0, 0);
    if (params->mass > 0.0f) {
        shape->calculateLocalInertia(params->mass, localInertia);
    }

    // Create motion state
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(params->position[0], params->position[1], params->position[2]));

    if (params->rotation[0] != 0.0f || params->rotation[1] != 0.0f || params->rotation[2] != 0.0f) {
        btQuaternion rot;
        rot.setEulerZYX(params->rotation[2] * M_PI / 180.0f,
                       params->rotation[1] * M_PI / 180.0f,
                       params->rotation[0] * M_PI / 180.0f);
        startTransform.setRotation(rot);
    }

    btDefaultMotionState *motionState = new btDefaultMotionState(startTransform);

    // Create rigid body
    btRigidBody::btRigidBodyConstructionInfo rbInfo(
        params->mass, motionState, shape, localInertia);

    rbInfo.m_friction = params->friction;
    rbInfo.m_restitution = params->restitution;

    btRigidBody *body = new btRigidBody(rbInfo);

    // Set kinematic flag if requested
    if (params->kinematic) {
        body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        body->setActivationState(DISABLE_DEACTIVATION);
    }

    // Add to world
    g_world->addRigidBody(body);

    // Store handle
    g_bodyHandles[handle] = body;

    return handle;
}

/*
================
Physics_DestroyBody
================
*/
physicsResult_t Physics_DestroyBody(int bodyHandle) {
    if (!g_physicsInitialized || bodyHandle <= 0 || bodyHandle >= MAX_PHYSICS_BODIES) {
        return PHYSICS_INVALID_ENTITY;
    }

    btRigidBody *body = g_bodyHandles[bodyHandle];
    if (!body) {
        return PHYSICS_INVALID_ENTITY;
    }

    // Remove from world
    g_world->removeRigidBody(body);

    // Clean up
    if (body->getMotionState()) {
        delete body->getMotionState();
    }
    if (body->getCollisionShape()) {
        delete body->getCollisionShape();
    }
    delete body;

    g_bodyHandles[bodyHandle] = nullptr;
    return PHYSICS_OK;
}

/*
================
Physics_GetBodyState
================
*/
physicsResult_t Physics_GetBodyState(int bodyHandle, physicsBodyState_t *state) {
    if (!g_physicsInitialized || !state || bodyHandle <= 0 || bodyHandle >= MAX_PHYSICS_BODIES) {
        return PHYSICS_INVALID_ENTITY;
    }

    btRigidBody *body = g_bodyHandles[bodyHandle];
    if (!body) {
        return PHYSICS_INVALID_ENTITY;
    }

    // Get transform
    btTransform trans;
    body->getMotionState()->getWorldTransform(trans);

    const btVector3 &origin = trans.getOrigin();
    state->position[0] = origin.x();
    state->position[1] = origin.y();
    state->position[2] = origin.z();

    // Convert quaternion to Euler
    btQuaternion rot = trans.getRotation();
    btScalar yaw, pitch, roll;
    rot.getEulerZYX(yaw, pitch, roll);
    state->rotation[0] = yaw * 180.0f / M_PI;
    state->rotation[1] = pitch * 180.0f / M_PI;
    state->rotation[2] = roll * 180.0f / M_PI;

    // Get velocities
    const btVector3 &linVel = body->getLinearVelocity();
    const btVector3 &angVel = body->getAngularVelocity();
    state->linearVelocity[0] = linVel.x();
    state->linearVelocity[1] = linVel.y();
    state->linearVelocity[2] = linVel.z();
    state->angularVelocity[0] = angVel.x();
    state->angularVelocity[1] = angVel.y();
    state->angularVelocity[2] = angVel.z();

    state->isActive = body->isActive();

    return PHYSICS_OK;
}

/*
================
Physics_SetBodyState
================
*/
physicsResult_t Physics_SetBodyState(int bodyHandle, const physicsBodyState_t *state) {
    if (!g_physicsInitialized || !state || bodyHandle <= 0 || bodyHandle >= MAX_PHYSICS_BODIES) {
        return PHYSICS_INVALID_ENTITY;
    }

    btRigidBody *body = g_bodyHandles[bodyHandle];
    if (!body) {
        return PHYSICS_INVALID_ENTITY;
    }

    // Set transform
    btTransform trans;
    trans.setOrigin(btVector3(state->position[0], state->position[1], state->position[2]));

    btQuaternion rot;
    rot.setEulerZYX(state->rotation[2] * M_PI / 180.0f,
                   state->rotation[1] * M_PI / 180.0f,
                   state->rotation[0] * M_PI / 180.0f);
    trans.setRotation(rot);

    body->getMotionState()->setWorldTransform(trans);

    // Set velocities
    body->setLinearVelocity(btVector3(state->linearVelocity[0],
                                     state->linearVelocity[1],
                                     state->linearVelocity[2]));
    body->setAngularVelocity(btVector3(state->angularVelocity[0],
                                      state->angularVelocity[1],
                                      state->angularVelocity[2]));

    return PHYSICS_OK;
}

/*
================
Physics_ApplyForce
================
*/
physicsResult_t Physics_ApplyForce(int bodyHandle, const physicsForce_t *force) {
    if (!g_physicsInitialized || !force || bodyHandle <= 0 || bodyHandle >= MAX_PHYSICS_BODIES) {
        return PHYSICS_INVALID_ENTITY;
    }

    btRigidBody *body = g_bodyHandles[bodyHandle];
    if (!body) {
        return PHYSICS_INVALID_ENTITY;
    }

    btVector3 f(force->force[0], force->force[1], force->force[2]);

    if (force->isImpulse) {
        body->applyImpulse(f, btVector3(force->position[0], force->position[1], force->position[2]));
    } else {
        body->applyForce(f, btVector3(force->position[0], force->position[1], force->position[2]));
    }

    return PHYSICS_OK;
}

/*
================
Physics_StepSimulation
================
*/
physicsResult_t Physics_StepSimulation(float deltaTime) {
    if (!g_physicsInitialized) {
        return PHYSICS_NOT_INITIALIZED;
    }

    // Use configurable substeps
    float substepSize = Cvar_VariableValue("sv_bulletFixedTimestep");
    int maxSubSteps = (int)Cvar_VariableValue("sv_bulletMaxSubSteps");

    if (substepSize <= 0.0f) {
        g_world->stepSimulation(deltaTime, maxSubSteps);
    } else {
        g_world->stepSimulation(deltaTime, maxSubSteps, substepSize);
    }

    return PHYSICS_OK;
}

// Stub implementations for remaining functions
physicsResult_t Physics_ApplyForceAtPosition(int bodyHandle, const vec3_t force, const vec3_t position) {
    (void)bodyHandle; (void)force; (void)position;
    return PHYSICS_OK;
}

physicsResult_t Physics_ApplyTorque(int bodyHandle, const vec3_t torque) {
    (void)bodyHandle; (void)torque;
    return PHYSICS_OK;
}

qboolean Physics_Raycast(const vec3_t start, const vec3_t end, vec3_t hitPoint,
                        vec3_t hitNormal, int *hitBody) {
    (void)start; (void)end; (void)hitPoint; (void)hitNormal; (void)hitBody;
    return qfalse;
}

int Physics_OverlapSphere(const vec3_t center, float radius, int *overlappingBodies, int maxResults) {
    (void)center; (void)radius; (void)overlappingBodies; (void)maxResults;
    return 0;
}

physicsResult_t Physics_SetCollisionCallback(physicsCollisionCallback_t callback, void *userData) {
    g_collisionCallback = callback;
    g_collisionUserData = userData;
    return PHYSICS_OK;
}

physicsResult_t Physics_EnableEntityPhysics(gentity_t *ent, const physicsBodyParams_t *params) {
    (void)ent; (void)params;
    return PHYSICS_OK;
}

physicsResult_t Physics_DisableEntityPhysics(gentity_t *ent) {
    (void)ent;
    return PHYSICS_OK;
}

physicsResult_t Physics_ECS_EnableEntity(ecs_entity_t entity, const physicsBodyParams_t *params) {
    (void)entity; (void)params;
    return PHYSICS_OK;
}

physicsResult_t Physics_ECS_DisableEntity(ecs_entity_t entity) {
    (void)entity;
    return PHYSICS_OK;
}

physicsResult_t Physics_SetGravity(const vec3_t gravity) {
    if (!g_physicsInitialized) {
        return PHYSICS_NOT_INITIALIZED;
    }
    g_world->setGravity(btVector3(gravity[0], gravity[1], gravity[2]));
    return PHYSICS_OK;
}

physicsResult_t Physics_GetGravity(vec3_t gravity) {
    if (!g_physicsInitialized) {
        return PHYSICS_NOT_INITIALIZED;
    }
    const btVector3 &g = g_world->getGravity();
    gravity[0] = g.x();
    gravity[1] = g.y();
    gravity[2] = g.z();
    return PHYSICS_OK;
}

physicsResult_t Physics_DebugDraw(qboolean enable) {
    if (!g_physicsInitialized || !g_debugDraw) {
        return PHYSICS_NOT_INITIALIZED;
    }
    g_debugDraw->setDebugMode(enable ? btIDebugDraw::DBG_DrawWireframe : btIDebugDraw::DBG_NoDebug);
    return PHYSICS_OK;
}

physicsResult_t Physics_GetStats(int *numBodies, int *numConstraints, float *stepTime) {
    if (!g_physicsInitialized) {
        return PHYSICS_NOT_INITIALIZED;
    }
    if (numBodies) *numBodies = g_world->getNumCollisionObjects();
    if (numConstraints) *numConstraints = g_world->getNumConstraints();
    if (stepTime) *stepTime = 0.0f; // TODO: Track step time
    return PHYSICS_OK;
}

#endif // USE_BULLET