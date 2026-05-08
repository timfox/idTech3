/*
 * Unit tests: ECS lifecycle, components, iteration, and motion integration.
 * Run: ctest -R unit_ecs
 */
#include <cstdio>
#include <cstring>

#include "game/ecs.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
	if ((a) != (b)) { \
		std::fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_FLOAT_EQ(a, b, msg) do { \
	const float diff = (a) > (b) ? (a) - (b) : (b) - (a); \
	if (diff > 0.0001f) { \
		std::fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_STREQ(a, b, msg) do { \
	if (std::strcmp((a), (b)) != 0) { \
		std::fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_uninitialized_defaults()
{
	vec3_t v;

	ECS_Shutdown();

	ASSERT_EQ( ECS_Create(), ECS_INVALID_ENTITY, "create without init is invalid" );
	ASSERT( ECS_Valid( ECS_INVALID_ENTITY ) == qfalse, "invalid entity is not valid without init" );
	ASSERT_EQ( ECS_Count(), 0u, "count without init" );
	ASSERT_EQ( ECS_CountWith( ECS_COMP_POSITION ), 0u, "countWith without init" );

	ECS_GetPosition( ECS_INVALID_ENTITY, v );
	ASSERT_FLOAT_EQ( v[0], 0.0f, "default position x" );
	ASSERT_FLOAT_EQ( v[1], 0.0f, "default position y" );
	ASSERT_FLOAT_EQ( v[2], 0.0f, "default position z" );

	ECS_GetScale( ECS_INVALID_ENTITY, v );
	ASSERT_FLOAT_EQ( v[0], 1.0f, "default scale x" );
	ASSERT_FLOAT_EQ( v[1], 1.0f, "default scale y" );
	ASSERT_FLOAT_EQ( v[2], 1.0f, "default scale z" );

	ASSERT_FLOAT_EQ( ECS_GetHealth( ECS_INVALID_ENTITY ), 0.0f, "default health" );
	ASSERT_STREQ( ECS_GetTag( ECS_INVALID_ENTITY ), "", "default tag" );
	ASSERT_EQ( ECS_GetGentityLink( ECS_INVALID_ENTITY ), -1, "default gentity link" );
	return 0;
}

static int test_component_lookup()
{
	ASSERT_EQ( ECS_ComponentFromName( "position" ), ECS_COMP_POSITION, "position lookup" );
	ASSERT_EQ( ECS_ComponentFromName( "VeLoCiTy" ), ECS_COMP_VELOCITY, "case-insensitive velocity lookup" );
	ASSERT_EQ( ECS_ComponentFromName( "gentity_link" ), ECS_COMP_GENTITY_LINK, "gentity link lookup" );
	ASSERT_EQ( ECS_ComponentFromName( nullptr ), ECS_COMP_COUNT, "NULL component lookup" );
	ASSERT_EQ( ECS_ComponentFromName( "unknown" ), ECS_COMP_COUNT, "unknown component lookup" );
	ASSERT_STREQ( ECS_ComponentName( ECS_COMP_HEALTH ), "health", "component name" );
	ASSERT_STREQ( ECS_ComponentName( ECS_COMP_COUNT ), "", "out-of-range component name" );
	return 0;
}

static int test_lifecycle_components_and_motion()
{
	ecs_entity_t e;
	vec3_t v;

	ECS_Init();
	ECS_Init();

	e = ECS_Create();
	ASSERT( e != ECS_INVALID_ENTITY, "created entity is valid handle" );
	ASSERT( ECS_Valid( e ) == qtrue, "created entity valid" );
	ASSERT_EQ( ECS_Count(), 1u, "one entity after create" );

	ECS_SetPosition( e, 1.0f, 2.0f, 3.0f );
	ECS_SetVelocity( e, 4.0f, 5.0f, 6.0f );
	ASSERT( ECS_Has( e, ECS_COMP_POSITION ) == qtrue, "position component added" );
	ASSERT( ECS_Has( e, ECS_COMP_VELOCITY ) == qtrue, "velocity component added" );
	ASSERT_EQ( ECS_CountWith( ECS_COMP_POSITION ), 1u, "position count" );
	ASSERT_EQ( ECS_CountWith( ECS_COMP_VELOCITY ), 1u, "velocity count" );

	ECS_StepMotion( 0.5f );
	ECS_GetPosition( e, v );
	ASSERT_FLOAT_EQ( v[0], 3.0f, "motion x" );
	ASSERT_FLOAT_EQ( v[1], 4.5f, "motion y" );
	ASSERT_FLOAT_EQ( v[2], 6.0f, "motion z" );

	ECS_StepMotion( 0.0f );
	ECS_StepMotion( -1.0f );
	ECS_GetPosition( e, v );
	ASSERT_FLOAT_EQ( v[0], 3.0f, "non-positive dt leaves x unchanged" );
	ASSERT_FLOAT_EQ( v[1], 4.5f, "non-positive dt leaves y unchanged" );
	ASSERT_FLOAT_EQ( v[2], 6.0f, "non-positive dt leaves z unchanged" );

	ECS_SetScale( e, 2.0f, 3.0f, 4.0f );
	ECS_GetScale( e, v );
	ASSERT_FLOAT_EQ( v[0], 2.0f, "scale x" );
	ASSERT_FLOAT_EQ( v[1], 3.0f, "scale y" );
	ASSERT_FLOAT_EQ( v[2], 4.0f, "scale z" );

	ECS_SetHealth( e, 42.0f );
	ASSERT_FLOAT_EQ( ECS_GetHealth( e ), 42.0f, "health value" );

	ECS_SetGentityLink( e, 7 );
	ASSERT_EQ( ECS_GetGentityLink( e ), 7, "gentity link value" );

	ECS_Remove( e, ECS_COMP_VELOCITY );
	ASSERT( ECS_Has( e, ECS_COMP_VELOCITY ) == qfalse, "velocity removed" );
	ASSERT_EQ( ECS_CountWith( ECS_COMP_VELOCITY ), 0u, "velocity count after remove" );

	ECS_Destroy( e );
	ASSERT( ECS_Valid( e ) == qfalse, "destroyed entity invalid" );
	ASSERT_EQ( ECS_Count(), 0u, "count after destroy" );

	ECS_Shutdown();
	ASSERT_EQ( ECS_Count(), 0u, "count after shutdown" );
	return 0;
}

static int test_tag_truncation()
{
	ecs_entity_t e;
	const char *tag;
	char long_tag[ECS_MAX_COMPONENT_NAME + 16];

	ECS_Init();
	e = ECS_Create();
	ASSERT( e != ECS_INVALID_ENTITY, "created entity for tag test" );

	std::memset( long_tag, 'a', sizeof( long_tag ) );
	long_tag[sizeof( long_tag ) - 1] = '\0';
	ECS_SetTag( e, long_tag );
	tag = ECS_GetTag( e );

	ASSERT_EQ( std::strlen( tag ), (size_t)ECS_MAX_COMPONENT_NAME - 1u, "tag is bounded" );
	ASSERT_EQ( tag[ECS_MAX_COMPONENT_NAME - 1], '\0', "tag is terminated" );

	ECS_SetTag( e, nullptr );
	ASSERT_STREQ( ECS_GetTag( e ), "", "NULL tag clears to empty" );

	ECS_Shutdown();
	return 0;
}

typedef struct {
	int count;
	ecs_entity_t entity;
} each_ctx_t;

static void each_counter( ecs_entity_t e, void *userdata )
{
	each_ctx_t *ctx = (each_ctx_t *)userdata;
	ctx->count++;
	ctx->entity = e;
}

static int test_each_filters_all_components()
{
	ecs_entity_t moving;
	ecs_entity_t stationary;
	ecs_component_id_t moving_components[2] = { ECS_COMP_POSITION, ECS_COMP_VELOCITY };
	each_ctx_t ctx = { 0, ECS_INVALID_ENTITY };

	ECS_Init();
	moving = ECS_Create();
	stationary = ECS_Create();

	ECS_SetPosition( moving, 0.0f, 0.0f, 0.0f );
	ECS_SetVelocity( moving, 1.0f, 0.0f, 0.0f );
	ECS_SetPosition( stationary, 5.0f, 0.0f, 0.0f );

	ECS_Each( moving_components, 2, each_counter, &ctx );
	ASSERT_EQ( ctx.count, 1, "ECS_Each requires all requested components" );
	ASSERT_EQ( ctx.entity, moving, "ECS_Each returned moving entity" );

	ECS_Each( nullptr, 2, each_counter, &ctx );
	ECS_Each( moving_components, 0, each_counter, &ctx );
	ECS_Each( moving_components, 2, nullptr, &ctx );
	ASSERT_EQ( ctx.count, 1, "invalid ECS_Each inputs are no-ops" );

	ECS_Shutdown();
	return 0;
}

int main()
{
	if ( test_uninitialized_defaults() ) return 1;
	if ( test_component_lookup() ) return 1;
	if ( test_lifecycle_components_and_motion() ) return 1;
	if ( test_tag_truncation() ) return 1;
	if ( test_each_filters_all_components() ) return 1;

	std::printf( "PASS: unit_ecs\n" );
	return 0;
}
