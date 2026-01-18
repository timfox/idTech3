/*
===========================================================================
Bullet Physics Integration for BSP Collision

Converts Quake 3 BSP geometry to Bullet Physics collision shapes
for static world collision detection.

This integrates with the existing CM_* collision system while adding
Bullet physics capabilities.
===========================================================================
*/

#ifdef USE_BULLET
#include <btBulletDynamicsCommon.h>
#include <vector>
#include "cm_local.h"
#include "cm_bullet.h"

// Static collision world for BSP geometry
static btBroadphaseInterface *bspBroadphase = nullptr;
static btDefaultCollisionConfiguration *bspConfig = nullptr;
static btCollisionDispatcher *bspDispatcher = nullptr;
static btSequentialImpulseConstraintSolver *bspSolver = nullptr;
static btDiscreteDynamicsWorld *bspWorld = nullptr;

static qboolean bspPhysicsInitialized = qfalse;

// Cache for collision shapes per model
#define MAX_BSP_MODELS 1024
static btCollisionShape *bspModelShapes[MAX_BSP_MODELS];
static qboolean bspModelShapeValid[MAX_BSP_MODELS];

// Dynamic rigid body management
#define MAX_RIGID_BODIES 512
static btRigidBody *rigidBodies[MAX_RIGID_BODIES];
static btCollisionShape *rigidBodyShapes[MAX_RIGID_BODIES];
static int nextRigidBodyId = 0;

/*
================
CM_Bullet_Init
Initialize Bullet physics for BSP collision

Initializes the Bullet Physics engine for static world collision detection.
Creates the collision world, broadphase, dispatcher, and solver components.
Sets gravity to zero since we're only using Bullet for static geometry collision.

Note: This function is idempotent - calling it multiple times is safe.
================
*/
void CM_Bullet_Init(void) {
	// Idempotent initialization check
	if (bspPhysicsInitialized) {
		return;
	}

	// Initialize Bullet components
	// These are allocated with 'new' and must be freed in CM_Bullet_Shutdown()
	bspBroadphase = new btDbvtBroadphase();
	bspConfig = new btDefaultCollisionConfiguration();
	bspDispatcher = new btCollisionDispatcher(bspConfig);
	bspSolver = new btSequentialImpulseConstraintSolver();
	bspWorld = new btDiscreteDynamicsWorld(bspDispatcher, bspBroadphase, bspSolver, bspConfig);

	// Set gravity to zero for static world collision
	// We only use Bullet for static geometry, not dynamic physics simulation
	bspWorld->setGravity(btVector3(0.0f, 0.0f, 0.0f));

	// Initialize shape cache to track collision shapes per model
	// This cache prevents rebuilding shapes on every map load
	Com_Memset(bspModelShapes, 0, sizeof(bspModelShapes));
	Com_Memset(bspModelShapeValid, 0, sizeof(bspModelShapeValid));

	bspPhysicsInitialized = qtrue;
	Com_Printf("Bullet BSP collision initialized\n");
}

/*
================
CM_Bullet_Shutdown
Shutdown Bullet physics for BSP collision

Cleans up all Bullet Physics components and frees allocated memory.
Removes all collision shapes from the world and deletes the physics world.

Note: This function is idempotent - calling it multiple times is safe.
================
*/
void CM_Bullet_Shutdown(void) {
	// Idempotent shutdown check
	if (!bspPhysicsInitialized) {
		return;
	}

	// Clean up collision shapes from cache
	// Iterate through all cached shapes and delete them, then mark cache as invalid
	for (int i = 0; i < MAX_BSP_MODELS; i++) {
		if (bspModelShapes[i]) {
			delete bspModelShapes[i];
			bspModelShapes[i] = nullptr;  // Clear pointer to prevent use-after-free
		}
		bspModelShapeValid[i] = qfalse;  // Mark cache entry as invalid
	}

	// Clean up world and all collision objects
	// Iterate backwards to safely remove objects while iterating
	if (bspWorld) {
		// Remove all collision objects from the world
		// Iterate backwards to avoid index shifting issues when removing items
		for (int i = bspWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
			btCollisionObject *obj = bspWorld->getCollisionObjectArray()[i];
			btRigidBody *body = btRigidBody::upcast(obj);
			// Clean up motion state if this is a rigid body with one
			if (body && body->getMotionState()) {
				delete body->getMotionState();
			}
			// Remove from world and delete the object
			bspWorld->removeCollisionObject(obj);
			delete obj;
		}

		// Delete Bullet components in reverse order of initialization
		// This ensures dependencies are cleaned up properly
		delete bspWorld;        // Delete world first (depends on others)
		delete bspSolver;       // Delete solver
		delete bspDispatcher;   // Delete dispatcher (depends on config)
		delete bspConfig;        // Delete configuration
		delete bspBroadphase;   // Delete broadphase last

		bspWorld = nullptr;
		bspSolver = nullptr;
		bspDispatcher = nullptr;
		bspConfig = nullptr;
		bspBroadphase = nullptr;
	}

	bspPhysicsInitialized = qfalse;
}

/*
================
CM_Bullet_BrushToTriangles
Convert a BSP brush to triangle data for collision mesh
================
*/
static void CM_Bullet_BrushToTriangles(const cbrush_t *brush,
                                      std::vector<btVector3> &vertices,
                                      std::vector<int> &indices) {
	// For now, create a simple box approximation for each brush
	// TODO: Proper brush geometry extraction - extract actual brush planes
	// and generate proper convex hull geometry instead of AABB approximation.
	// This will improve collision accuracy, especially for sloped surfaces.

	// Validate brush pointer and minimum geometry requirements
	if (!brush) {
		return; // Invalid brush pointer
	}
	if (brush->numsides < 4) {
		// A brush needs at least 4 sides to form a valid 3D volume
		return;
	}

	// Find brush bounds
	vec3_t mins = {999999, 999999, 999999};
	vec3_t maxs = {-999999, -999999, -999999};

	for (int i = 0; i < brush->numsides; i++) {
		const cbrushside_t *side = &brush->sides[i];
		const cplane_t *plane = side->plane;

		// Approximate brush bounds from planes
		// This is a simplified approach - real implementation would
		// need proper brush geometry extraction
		// TODO: Use plane information to calculate proper brush bounds
		// For now, these variables are placeholders for future implementation
		(void)plane;
	}

	// Suppress unused variable warnings - these will be used when proper
	// brush geometry extraction is implemented (see TODO above)
	(void)mins;
	(void)maxs;

	// Create a simple box mesh as placeholder
	// This is a temporary approximation - proper implementation would extract
	// actual brush geometry from the brush planes to create accurate collision shapes.
	// The default size is arbitrary and should be replaced with actual brush bounds.
	// Note: 32.0f units = 64x64x64 unit box, which is a common brush size in Quake maps
	// This provides reasonable collision for most brushes until proper geometry extraction is implemented
	static const float DEFAULT_BRUSH_HALF_SIZE = 32.0f;  // 64x64x64 unit box (temporary)
	float halfWidth = DEFAULT_BRUSH_HALF_SIZE;
	float halfHeight = DEFAULT_BRUSH_HALF_SIZE;
	float halfDepth = DEFAULT_BRUSH_HALF_SIZE;

	// Define box vertices in local space
	// These form a unit box centered at origin, which will be scaled by halfWidth/Height/Depth
	btVector3 boxVerts[8] = {
		btVector3(-halfWidth, -halfHeight, -halfDepth),
		btVector3(halfWidth, -halfHeight, -halfDepth),
		btVector3(halfWidth, halfHeight, -halfDepth),
		btVector3(-halfWidth, halfHeight, -halfDepth),
		btVector3(-halfWidth, -halfHeight, halfDepth),
		btVector3(halfWidth, -halfHeight, halfDepth),
		btVector3(halfWidth, halfHeight, halfDepth),
		btVector3(-halfWidth, halfHeight, halfDepth)
	};

	// Define box triangles (12 triangles, 36 indices)
	// Each face is composed of 2 triangles (6 indices per face)
	// Vertices are ordered to ensure correct winding (counter-clockwise when viewed from outside)
	int boxIndices[36] = {
		// Front face (+Z direction)
		0, 1, 2, 0, 2, 3,
		// Back face (-Z direction)
		5, 4, 7, 5, 7, 6,
		// Left face (-X direction)
		4, 0, 3, 4, 3, 7,
		// Right face
		1, 5, 6, 1, 6, 2,
		// Top face
		3, 2, 6, 3, 6, 7,
		// Bottom face
		4, 5, 1, 4, 1, 0
	};

	// Add to output arrays
	// baseVertex tracks the starting index for this brush's vertices
	// This allows multiple brushes to share the same vertex array
	int baseVertex = vertices.size();
	for (int i = 0; i < 8; i++) {
		vertices.push_back(boxVerts[i]);
	}
	for (int i = 0; i < 36; i++) {
		indices.push_back(baseVertex + boxIndices[i]);
	}
}

/*
================
CM_Bullet_BuildModelCollision
Build collision shape for a BSP model
================
*/
btCollisionShape* CM_Bullet_BuildModelCollision(clipHandle_t model) {
	// Validate model handle range
	if (model < 0 || model >= MAX_BSP_MODELS) {
		Com_DPrintf("CM_Bullet_BuildModelCollision: invalid model handle %d (range: 0-%d)\n",
		            model, MAX_BSP_MODELS - 1);
		return nullptr;
	}

	// Check cache first
	if (bspModelShapeValid[model] && bspModelShapes[model]) {
		return bspModelShapes[model];
	}

	// Get the collision model from the clip handle
	// Note: CM_ClipHandleToModel is defined in cm_load.c (C) and called from C++.
	// The function should be accessible since cm_local.h is included, but linkage
	// errors occur because cm_local.h declarations need extern "C" when included
	// from C++ files. Until this is fixed, the function call remains commented out.
	// TODO: Fix C/C++ linkage by adding extern "C" guards to cm_local.h or
	//       creating a C++-compatible wrapper function.
	// cmodel_t *cmod = CM_ClipHandleToModel(model);
	// if (!cmod) {
	// 	Com_DPrintf("CM_Bullet_BuildModelCollision: failed to get collision model for handle %d\n", model);
	// 	return nullptr;
	// }
	return nullptr; // Temporary: disabled until linkage issue is resolved

	// Collect all triangles from brushes in this model
	// We accumulate vertices and indices from all brushes to create a single collision mesh
	std::vector<btVector3> vertices;
	std::vector<int> indices;

	// For world model (model 0), include all brushes
	// World model contains all static geometry that needs collision detection
	if (model == 0) {
		// Validate brush count before iteration to prevent invalid memory access
		if (cm.numBrushes < 0 || !cm.brushes) {
			Com_DPrintf("CM_Bullet_BuildModelCollision: invalid brush data for world model\n");
			return nullptr;
		}
		// Convert each brush in the world model to triangle data
		for (int i = 0; i < cm.numBrushes; i++) {
			const cbrush_t *brush = &cm.brushes[i];
			CM_Bullet_BrushToTriangles(brush, vertices, indices);
		}
	} else {
		// TODO: Implement submodel collision geometry.
		// Submodels (brush models) need their own collision shape generation.
		// This requires extracting geometry from the submodel's brushes and
		// creating appropriate Bullet collision shapes (convex hulls or mesh shapes).
		// For now, submodels don't have Bullet collision - fall back to BSP.
		return nullptr;
	}

	// Validate that we have geometry data before creating collision shape
	// Empty vertex list means no collision geometry was generated for this model
	if (vertices.empty()) {
		Com_DPrintf("CM_Bullet_BuildModelCollision: no geometry data for model %d\n", model);
		return nullptr;
	}

	// Validate that indices are properly aligned (must be multiple of 3 for triangles)
	if (indices.size() % 3 != 0) {
		Com_DPrintf("CM_Bullet_BuildModelCollision: invalid index count for model %d (%zu indices, not multiple of 3)\n",
		            model, indices.size());
		return nullptr;
	}

	// Create triangle mesh from collected vertices and indices
	// The triangle mesh stores the actual geometry data for Bullet Physics
	btTriangleMesh *mesh = new btTriangleMesh();
	if (!mesh) {
		Com_Printf("CM_Bullet_BuildModelCollision: failed to allocate triangle mesh for model %d\n", model);
		return nullptr;
	}

	// Add triangles to the mesh (indices are validated above to be multiple of 3)
	const size_t vertex_count = vertices.size();
	for (size_t i = 0; i < indices.size(); i += 3) {
		const int index_a = indices[i];
		const int index_b = indices[i + 1];
		const int index_c = indices[i + 2];

		// Validate index bounds before accessing vertices
		if (index_a < 0 || index_b < 0 || index_c < 0 ||
			static_cast<size_t>(index_a) >= vertex_count ||
			static_cast<size_t>(index_b) >= vertex_count ||
			static_cast<size_t>(index_c) >= vertex_count) {
			Com_Printf("CM_Bullet_BuildModelCollision: index out of bounds for model %d (vertex count: %zu)\n",
			           model, vertex_count);
			delete mesh;
			return nullptr;
		}
		// Add each triangle (3 consecutive indices form one triangle)
		mesh->addTriangle(vertices[static_cast<size_t>(index_a)],
		                 vertices[static_cast<size_t>(index_b)],
		                 vertices[static_cast<size_t>(index_c)]);
	}

	// Create BvhTriangleMeshShape for static collision
	// BVH (Bounding Volume Hierarchy) provides efficient collision queries
	// The 'true' parameter enables useQuantizedAabbCompression for memory efficiency
	btBvhTriangleMeshShape *shape = new btBvhTriangleMeshShape(mesh, true);

	// Cache the shape
	bspModelShapes[model] = shape;
	bspModelShapeValid[model] = qtrue;

	Com_DPrintf("Built collision shape for model %d: %d triangles\n", model, (int)(indices.size() / 3));

	return shape;
}

/*
================
CM_Bullet_LoadMap
Called when a map is loaded to build collision geometry

Builds Bullet collision shapes for the current BSP map.
Clears any existing collision shapes and rebuilds them from the current map data.
This should be called whenever a new map is loaded.

Note: Automatically initializes Bullet if not already initialized.
================
*/
void CM_Bullet_LoadMap(void) {
	// Ensure Bullet is initialized before loading map geometry
	// This allows the function to be called safely even if initialization was skipped
	if (!bspPhysicsInitialized) {
		CM_Bullet_Init();
	}

	// Clear old shapes from previous map
	// This prevents memory leaks and ensures clean state for new map
	for (int i = 0; i < MAX_BSP_MODELS; i++) {
		if (bspModelShapes[i]) {
			delete bspModelShapes[i];
			bspModelShapes[i] = nullptr;  // Clear pointer
		}
		bspModelShapeValid[i] = qfalse;  // Invalidate cache entry
	}

	// Build collision shapes for all models
	for (int i = 0; i < CM_NumInlineModels(); i++) {
		clipHandle_t model = CM_InlineModel(i);
		CM_Bullet_BuildModelCollision(model);
	}

	// Add world collision to Bullet world
	btCollisionShape *worldShape = CM_Bullet_BuildModelCollision(0); // World model
	if (worldShape) {
		btTransform transform;
		transform.setIdentity();

		btDefaultMotionState *motionState = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, motionState, worldShape, btVector3(0, 0, 0));
		btRigidBody *body = new btRigidBody(rbInfo);

		// Static world geometry doesn't move
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

		bspWorld->addRigidBody(body);
		Com_DPrintf("Added world collision to Bullet physics\n");
	}
}

/*
================
CM_Bullet_Trace
Perform a trace using Bullet physics (complement to BSP tracing)
================
*/
void CM_Bullet_Trace(trace_t *results, const vec3_t start, const vec3_t end,
                    const vec3_t mins, const vec3_t maxs, clipHandle_t model,
                    int brushmask, qboolean cylinder) {
	// Validate input parameters
	if (!results) {
		Com_Printf("CM_Bullet_Trace: NULL results pointer\n");
		return;
	}

	// Fall back to BSP tracing if Bullet physics not initialized
	// This ensures the engine always has a working trace function
	if (!bspPhysicsInitialized || !bspWorld) {
		CM_BoxTrace(results, start, end, mins, maxs, model, brushmask, cylinder);
		return;
	}

	// For now, just call the regular BSP trace
	// TODO: Implement Bullet-based tracing for dynamic objects.
	// This would use btCollisionWorld::rayTest() for more accurate dynamic object
	// collision detection, especially for moving entities and complex shapes.
	CM_BoxTrace(results, start, end, mins, maxs, model, brushmask, cylinder);
}

/*
================
CM_Bullet_PointContents
Check point contents using Bullet physics
================
*/
int CM_Bullet_PointContents(const vec3_t p, clipHandle_t model) {
	// Validate input parameter
	if (!p) {
		Com_Printf("CM_Bullet_PointContents: NULL point pointer\n");
		return 0; // Default to empty contents
	}

	// Fall back to BSP point contents if Bullet physics not initialized
	// This ensures the engine always has a working point contents function
	if (!bspPhysicsInitialized || !bspWorld) {
		return CM_PointContents(p, model);
	}

	// For now, just call the regular BSP function
	// TODO: Implement Bullet-based point contents checking.
	// This would use btCollisionWorld::contactTest() or similar to determine
	// if a point is inside collision geometry, useful for trigger volumes and
	// area detection with better accuracy than BSP-based checks.
	return CM_PointContents(p, model);
}

/*
================
CM_Bullet_CreateRigidBody
Create a dynamic rigid body for physics simulation

Creates a new rigid body with the specified mass, collision shape, and initial transform.
Returns a handle to the rigid body for later manipulation.

Parameters:
- mass: Mass of the rigid body (0 for static, >0 for dynamic)
- shape: Collision shape (box, sphere, capsule, etc.)
- position: Initial position in world space
- rotation: Initial rotation quaternion
================
*/
int CM_Bullet_CreateRigidBody(float mass, btCollisionShape *shape,
                             const vec3_t position, const vec4_t rotation) {
	if (!bspPhysicsInitialized || !bspWorld || nextRigidBodyId >= MAX_RIGID_BODIES) {
		return -1; // Invalid handle
	}

	// Create motion state
	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(position[0], position[1], position[2]));
	if (rotation) {
		startTransform.setRotation(btQuaternion(rotation[0], rotation[1], rotation[2], rotation[3]));
	}

	btVector3 localInertia(0, 0, 0);
	if (mass > 0.0f) {
		shape->calculateLocalInertia(mass, localInertia);
	}

	btDefaultMotionState *motionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
	btRigidBody *body = new btRigidBody(rbInfo);

	// Store references
	int handle = nextRigidBodyId++;
	rigidBodies[handle] = body;
	rigidBodyShapes[handle] = shape;

	// Add to world
	bspWorld->addRigidBody(body);

	Com_DPrintf("Created rigid body (handle %d, mass %.2f)\n", handle, mass);
	return handle;
}

/*
================
CM_Bullet_DestroyRigidBody
Destroy a dynamic rigid body

Removes the rigid body from the physics world and frees associated resources.
================
*/
void CM_Bullet_DestroyRigidBody(int handle) {
	if (handle < 0 || handle >= MAX_RIGID_BODIES || !rigidBodies[handle]) {
		return;
	}

	btRigidBody *body = rigidBodies[handle];

	// Remove from world
	if (bspWorld) {
		bspWorld->removeRigidBody(body);
	}

	// Clean up resources
	delete body->getMotionState();
	delete body;

	// Clear references
	rigidBodies[handle] = nullptr;
	rigidBodyShapes[handle] = nullptr;

	Com_DPrintf("Destroyed rigid body (handle %d)\n", handle);
}

/*
================
CM_Bullet_UpdateRigidBody
Update rigid body transform and properties

Allows external systems to update the position, rotation, and other properties
of dynamic rigid bodies.
================
*/
void CM_Bullet_UpdateRigidBody(int handle, const vec3_t position,
                              const vec4_t rotation, const vec3_t velocity) {
	if (handle < 0 || handle >= MAX_RIGID_BODIES || !rigidBodies[handle]) {
		return;
	}

	btRigidBody *body = rigidBodies[handle];

	// Update transform
	if (position || rotation) {
		btTransform transform = body->getWorldTransform();

		if (position) {
			transform.setOrigin(btVector3(position[0], position[1], position[2]));
		}

		if (rotation) {
			transform.setRotation(btQuaternion(rotation[0], rotation[1], rotation[2], rotation[3]));
		}

		body->setWorldTransform(transform);
	}

	// Update velocity
	if (velocity) {
		body->setLinearVelocity(btVector3(velocity[0], velocity[1], velocity[2]));
	}

	// Activate the body to ensure it's processed in the next simulation step
	body->activate(true);
}

/*
================
CM_Bullet_GetRigidBodyTransform
Get current rigid body transform

Retrieves the current position and rotation of a rigid body.
================
*/
void CM_Bullet_GetRigidBodyTransform(int handle, vec3_t position, vec4_t rotation) {
	if (handle < 0 || handle >= MAX_RIGID_BODIES || !rigidBodies[handle] ||
	    !position || !rotation) {
		return;
	}

	btRigidBody *body = rigidBodies[handle];
	const btTransform &transform = body->getWorldTransform();

	// Get position
	const btVector3 &origin = transform.getOrigin();
	position[0] = origin.x();
	position[1] = origin.y();
	position[2] = origin.z();

	// Get rotation
	const btQuaternion &quat = transform.getRotation();
	rotation[0] = quat.x();
	rotation[1] = quat.y();
	rotation[2] = quat.z();
	rotation[3] = quat.w();
}

/*
================
CM_Bullet_StepSimulation
Step the physics simulation forward

Advances the physics simulation by the specified time step.
Should be called once per frame or at a fixed rate.
================
*/
void CM_Bullet_StepSimulation(float timeStep) {
	if (!bspPhysicsInitialized || !bspWorld) {
		return;
	}

	// Step the simulation
	// Use fixed time step for stability, accumulate time if needed
	static float accumulatedTime = 0.0f;
	const float fixedTimeStep = 1.0f / 60.0f; // 60 Hz simulation
	const int maxSubSteps = 10;

	accumulatedTime += timeStep;

	int numSteps = 0;
	while (accumulatedTime >= fixedTimeStep && numSteps < maxSubSteps) {
		bspWorld->stepSimulation(fixedTimeStep, 0, fixedTimeStep);
		accumulatedTime -= fixedTimeStep;
		numSteps++;
	}

	// Prevent accumulation from growing too large
	if (accumulatedTime > fixedTimeStep * 5.0f) {
		accumulatedTime = fixedTimeStep * 5.0f;
	}
}

/*
================
CM_Bullet_CreateBoxShape
Create a box collision shape
================
*/
btCollisionShape *CM_Bullet_CreateBoxShape(const vec3_t halfExtents) {
	return new btBoxShape(btVector3(halfExtents[0], halfExtents[1], halfExtents[2]));
}

/*
================
CM_Bullet_CreateSphereShape
Create a sphere collision shape
================
*/
btCollisionShape *CM_Bullet_CreateSphereShape(float radius) {
	return new btSphereShape(radius);
}

/*
================
CM_Bullet_CreateCapsuleShape
Create a capsule collision shape
================
*/
btCollisionShape *CM_Bullet_CreateCapsuleShape(float radius, float height) {
	return new btCapsuleShape(radius, height);
}

/*
================
CM_Bullet_DestroyShape
Destroy a collision shape
================
*/
void CM_Bullet_DestroyShape(btCollisionShape *shape) {
	if (shape) {
		delete shape;
	}
}

#endif // USE_BULLET