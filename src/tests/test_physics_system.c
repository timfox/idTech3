/*
===========================================================================
Bullet Physics System Tests

Unit tests and integration tests for the Bullet Physics integration.
===========================================================================
*/

#include "test_framework.h"

#ifdef USE_BULLET
#include "../common/physics_bullet.h"
#include "../common/ecs.h"
#include "../common/ecs_components.h"

// ECS entity constants
#ifdef USE_ENTT
#define ECS_INVALID_ENTITY ((ecs_entity_t)-1)
#endif

// Test physics initialization
static void test_physics_init(void) {
	physicsResult_t result;

	// Test initialization
	result = Physics_Init();
	ASSERT_EQ(result, PHYSICS_OK);

	// Test double initialization (should be ok)
	result = Physics_Init();
	ASSERT_EQ(result, PHYSICS_OK);

	// Test shutdown
	Physics_Shutdown();

	// Test that it's not initialized after shutdown
	ASSERT_EQ(Physics_IsInitialized(), qfalse);
}

// Test rigid body creation
static void test_physics_create_body(void) {
	physicsBodyParams_t params;
	physicsResult_t result;
	int bodyHandle;

	// Initialize physics
	result = Physics_Init();
	ASSERT_EQ(result, PHYSICS_OK);

	// Test creating a box body
	params.shapeType = PHYSICS_SHAPE_BOX;
	VectorSet(params.shapeDimensions, 1.0f, 1.0f, 1.0f);
	VectorSet(params.position, 0.0f, 0.0f, 10.0f);
	VectorClear(params.rotation);
	params.mass = 1.0f;
	params.friction = 0.5f;
	params.restitution = 0.3f;
	params.kinematic = qfalse;

	bodyHandle = Physics_CreateBody(&params);
	ASSERT_NE(bodyHandle, 0); // Should get a valid handle

	// Test getting body state
	physicsBodyState_t state;
	result = Physics_GetBodyState(bodyHandle, &state);
	ASSERT_EQ(result, PHYSICS_OK);

	// Check initial position
	ASSERT_FLOAT_EQ(state.position[0], 0.0f, 0.01f);
	ASSERT_FLOAT_EQ(state.position[1], 0.0f, 0.01f);
	ASSERT_FLOAT_EQ(state.position[2], 10.0f, 0.01f);

	// Test destroying body
	result = Physics_DestroyBody(bodyHandle);
	ASSERT_EQ(result, PHYSICS_OK);

	Physics_Shutdown();
}

// Test physics simulation
static void test_physics_simulation(void) {
	physicsBodyParams_t params;
	physicsResult_t result;
	int bodyHandle;
	physicsBodyState_t state;

	// Initialize physics
	result = Physics_Init();
	ASSERT_EQ(result, PHYSICS_OK);

	// Create a falling box
	params.shapeType = PHYSICS_SHAPE_BOX;
	VectorSet(params.shapeDimensions, 1.0f, 1.0f, 1.0f);
	VectorSet(params.position, 0.0f, 0.0f, 10.0f);
	VectorClear(params.rotation);
	params.mass = 1.0f;
	params.friction = 0.5f;
	params.restitution = 0.3f;
	params.kinematic = qfalse;

	bodyHandle = Physics_CreateBody(&params);
	ASSERT_NE(bodyHandle, 0);

	// Run a few simulation steps
	for (int i = 0; i < 10; i++) {
		result = Physics_StepSimulation(0.016f); // ~60 FPS
		ASSERT_EQ(result, PHYSICS_OK);
	}

	// Check that the body has fallen (position should be less than initial)
	result = Physics_GetBodyState(bodyHandle, &state);
	ASSERT_EQ(result, PHYSICS_OK);
	ASSERT_TRUE(state.position[2] < 10.0f); // Should be lower than initial position

	Physics_DestroyBody(bodyHandle);
	Physics_Shutdown();
}

// Test error handling
static void test_physics_error_handling(void) {
	physicsResult_t result;
	physicsBodyState_t state;

	// Test operations without initialization
	result = Physics_GetBodyState(1, &state);
	ASSERT_EQ(result, PHYSICS_NOT_INITIALIZED);

	result = Physics_DestroyBody(1);
	ASSERT_EQ(result, PHYSICS_NOT_INITIALIZED);

	// Initialize physics
	result = Physics_Init();
	ASSERT_EQ(result, PHYSICS_OK);

	// Test invalid body handle
	result = Physics_GetBodyState(999, &state);
	ASSERT_EQ(result, PHYSICS_INVALID_ENTITY);

	result = Physics_DestroyBody(999);
	ASSERT_EQ(result, PHYSICS_INVALID_ENTITY);

	Physics_Shutdown();
}

// Test gravity settings
static void test_physics_gravity(void) {
	physicsResult_t result;
	vec3_t gravity;

	// Initialize physics
	result = Physics_Init();
	ASSERT_EQ(result, PHYSICS_OK);

	// Test setting gravity
	vec3_t testGravity = {0.0f, 0.0f, -500.0f};
	result = Physics_SetGravity(testGravity);
	ASSERT_EQ(result, PHYSICS_OK);

	// Test getting gravity
	result = Physics_GetGravity(gravity);
	ASSERT_EQ(result, PHYSICS_OK);

	ASSERT_FLOAT_EQ(gravity[0], testGravity[0], 0.01f);
	ASSERT_FLOAT_EQ(gravity[1], testGravity[1], 0.01f);
	ASSERT_FLOAT_EQ(gravity[2], testGravity[2], 0.01f);

	Physics_Shutdown();
}

#endif // USE_BULLET

// Test runner
void Test_PhysicsSystem(void) {
#ifdef USE_BULLET
	Com_Printf("Running physics system tests...\n");

	test_physics_init();
	test_physics_create_body();
	test_physics_simulation();
	test_physics_error_handling();
	test_physics_gravity();

	Com_Printf("Physics system tests completed: %d passed, %d failed\n",
			   test_passed, test_failed);
#else
	Com_Printf("Physics system tests skipped (USE_BULLET not defined)\n");
#endif
}