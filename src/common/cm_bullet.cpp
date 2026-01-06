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
#include "cm_local.h"
#include "cm_bullet.h"
#include <btBulletDynamicsCommon.h>
#include <vector>

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

/*
================
CM_Bullet_Init
Initialize Bullet physics for BSP collision
================
*/
void CM_Bullet_Init(void) {
	if (bspPhysicsInitialized) {
		return;
	}

	// Initialize Bullet components
	bspBroadphase = new btDbvtBroadphase();
	bspConfig = new btDefaultCollisionConfiguration();
	bspDispatcher = new btCollisionDispatcher(bspConfig);
	bspSolver = new btSequentialImpulseConstraintSolver();
	bspWorld = new btDiscreteDynamicsWorld(bspDispatcher, bspBroadphase, bspSolver, bspConfig);

	// Set gravity to zero for static world collision
	bspWorld->setGravity(btVector3(0.0f, 0.0f, 0.0f));

	// Initialize shape cache
	Com_Memset(bspModelShapes, 0, sizeof(bspModelShapes));
	Com_Memset(bspModelShapeValid, 0, sizeof(bspModelShapeValid));

	bspPhysicsInitialized = qtrue;
	Com_Printf("Bullet BSP collision initialized\n");
}

/*
================
CM_Bullet_Shutdown
Shutdown Bullet physics for BSP collision
================
*/
void CM_Bullet_Shutdown(void) {
	if (!bspPhysicsInitialized) {
		return;
	}

	// Clean up collision shapes
	for (int i = 0; i < MAX_BSP_MODELS; i++) {
		if (bspModelShapes[i]) {
			delete bspModelShapes[i];
			bspModelShapes[i] = nullptr;
		}
		bspModelShapeValid[i] = qfalse;
	}

	// Clean up world
	if (bspWorld) {
		// Remove all collision objects
		for (int i = bspWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
			btCollisionObject *obj = bspWorld->getCollisionObjectArray()[i];
			btRigidBody *body = btRigidBody::upcast(obj);
			if (body && body->getMotionState()) {
				delete body->getMotionState();
			}
			bspWorld->removeCollisionObject(obj);
			delete obj;
		}

		delete bspWorld;
		delete bspSolver;
		delete bspDispatcher;
		delete bspConfig;
		delete bspBroadphase;

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
	// TODO: Proper brush geometry extraction

	if (!brush || brush->numsides < 4) {
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
		(void)plane; // Suppress unused variable warning - placeholder for future implementation
	}

	// Suppress unused variable warnings - placeholder for future bounds calculation
	(void)mins;
	(void)maxs;

	// Create a simple box mesh as placeholder
	float halfWidth = 32.0f;  // Default brush size
	float halfHeight = 32.0f;
	float halfDepth = 32.0f;

	// Define box vertices
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
	int boxIndices[36] = {
		// Front face
		0, 1, 2, 0, 2, 3,
		// Back face
		5, 4, 7, 5, 7, 6,
		// Left face
		4, 0, 3, 4, 3, 7,
		// Right face
		1, 5, 6, 1, 6, 2,
		// Top face
		3, 2, 6, 3, 6, 7,
		// Bottom face
		4, 5, 1, 4, 1, 0
	};

	// Add to output arrays
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
	if (model < 0 || model >= MAX_BSP_MODELS) {
		return nullptr;
	}

	// Check cache first
	if (bspModelShapeValid[model] && bspModelShapes[model]) {
		return bspModelShapes[model];
	}

	// cmodel_t *cmod = CM_ClipHandleToModel(model);
	// if (!cmod) {
	//	return nullptr;
	// }
	// TODO: Fix CM_ClipHandleToModel linkage issue
	return nullptr;

	// Collect all triangles from brushes in this model
	std::vector<btVector3> vertices;
	std::vector<int> indices;

	// For world model (model 0), include all brushes
	if (model == 0) {
		for (int i = 0; i < cm.numBrushes; i++) {
			const cbrush_t *brush = &cm.brushes[i];
			CM_Bullet_BrushToTriangles(brush, vertices, indices);
		}
	} else {
		// TODO: Implement submodel collision geometry
		// For now, submodels don't have collision
		return nullptr;
	}

	if (vertices.empty()) {
		return nullptr;
	}

	// Create triangle mesh
	btTriangleMesh *mesh = new btTriangleMesh();
	for (size_t i = 0; i < indices.size(); i += 3) {
		mesh->addTriangle(vertices[indices[i]],
		                 vertices[indices[i + 1]],
		                 vertices[indices[i + 2]]);
	}

	// Create BvhTriangleMeshShape for static collision
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
================
*/
void CM_Bullet_LoadMap(void) {
	if (!bspPhysicsInitialized) {
		CM_Bullet_Init();
	}

	// Clear old shapes
	for (int i = 0; i < MAX_BSP_MODELS; i++) {
		if (bspModelShapes[i]) {
			delete bspModelShapes[i];
			bspModelShapes[i] = nullptr;
		}
		bspModelShapeValid[i] = qfalse;
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
	if (!bspPhysicsInitialized || !bspWorld) {
		// Fall back to BSP tracing if Bullet not available
		CM_BoxTrace(results, start, end, mins, maxs, model, brushmask, cylinder);
		return;
	}

	// For now, just call the regular BSP trace
	// TODO: Implement Bullet-based tracing for dynamic objects
	CM_BoxTrace(results, start, end, mins, maxs, model, brushmask, cylinder);
}

/*
================
CM_Bullet_PointContents
Check point contents using Bullet physics
================
*/
int CM_Bullet_PointContents(const vec3_t p, clipHandle_t model) {
	if (!bspPhysicsInitialized || !bspWorld) {
		// Fall back to BSP point contents
		return CM_PointContents(p, model);
	}

	// For now, just call the regular BSP function
	// TODO: Implement Bullet-based point contents checking
	return CM_PointContents(p, model);
}

#endif // USE_BULLET