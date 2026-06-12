/*
 * Unit tests: ECS C ABI lifecycle, component defaults, filters, and motion.
 */
#include <cmath>
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

#define ASSERT_STREQ(a, b, msg) do { \
	if (std::strcmp((a), (b)) != 0) { \
		std::fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_NEAR(a, b, eps, msg) do { \
	if (std::fabs((a) - (b)) > (eps)) { \
		std::fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_uninitialized_api_is_safe(void)
{
	vec3_t out;

	ECS_Shutdown();
	ASSERT_EQ(ECS_Create(), ECS_INVALID_ENTITY, "create before init returns invalid");
	ASSERT_EQ(ECS_Count(), 0u, "count before init");
	ASSERT(ECS_Valid(ECS_INVALID_ENTITY) == qfalse, "invalid before init");

	VectorSet(out, 9.0f, 9.0f, 9.0f);
	ECS_GetPosition(ECS_INVALID_ENTITY, out);
	ASSERT_NEAR(out[0], 0.0f, 0.0001f, "position default x before init");
	ASSERT_NEAR(out[1], 0.0f, 0.0001f, "position default y before init");
	ASSERT_NEAR(out[2], 0.0f, 0.0001f, "position default z before init");
	ASSERT_NEAR(ECS_GetHealth(ECS_INVALID_ENTITY), 0.0f, 0.0001f, "health before init");
	ASSERT_EQ(ECS_GetGentityLink(ECS_INVALID_ENTITY), -1, "gentity link before init");
	ASSERT_STREQ(ECS_GetTag(ECS_INVALID_ENTITY), "", "tag before init");

	return 0;
}

static int test_lifecycle_and_component_defaults(void)
{
	ecs_entity_t e;
	vec3_t out;

	ECS_Init();
	ECS_Init();
	e = ECS_Create();
	ASSERT(e != ECS_INVALID_ENTITY, "created entity is valid handle");
	ASSERT(ECS_Valid(e) == qtrue, "created entity validates");
	ASSERT_EQ(ECS_Count(), 1u, "one live entity");
	ASSERT_EQ(ECS_CountWith((ecs_component_id_t)-1), 0u, "invalid component count");
	ASSERT(ECS_Has(e, ECS_COMP_POSITION) == qfalse, "new entity has no position");

	ECS_Add(e, ECS_COMP_POSITION);
	ECS_Add(e, ECS_COMP_ROTATION);
	ECS_Add(e, ECS_COMP_SCALE);
	ECS_Add(e, ECS_COMP_VELOCITY);
	ECS_Add(e, ECS_COMP_HEALTH);
	ECS_Add(e, ECS_COMP_GENTITY_LINK);

	ASSERT_EQ(ECS_CountWith(ECS_COMP_POSITION), 1u, "position count");
	ASSERT_EQ(ECS_CountWith(ECS_COMP_ROTATION), 1u, "rotation count");
	ASSERT_EQ(ECS_CountWith(ECS_COMP_SCALE), 1u, "scale count");
	ASSERT_EQ(ECS_CountWith(ECS_COMP_VELOCITY), 1u, "velocity count");
	ASSERT_EQ(ECS_CountWith(ECS_COMP_HEALTH), 1u, "health count");
	ASSERT_EQ(ECS_CountWith(ECS_COMP_GENTITY_LINK), 1u, "gentity link count");

	ECS_GetPosition(e, out);
	ASSERT_NEAR(out[0], 0.0f, 0.0001f, "default position x");
	ASSERT_NEAR(out[1], 0.0f, 0.0001f, "default position y");
	ASSERT_NEAR(out[2], 0.0f, 0.0001f, "default position z");

	ECS_GetRotation(e, out);
	ASSERT_NEAR(out[0], 0.0f, 0.0001f, "default rotation pitch");
	ASSERT_NEAR(out[1], 0.0f, 0.0001f, "default rotation yaw");
	ASSERT_NEAR(out[2], 0.0f, 0.0001f, "default rotation roll");

	ECS_GetScale(e, out);
	ASSERT_NEAR(out[0], 1.0f, 0.0001f, "default scale x");
	ASSERT_NEAR(out[1], 1.0f, 0.0001f, "default scale y");
	ASSERT_NEAR(out[2], 1.0f, 0.0001f, "default scale z");

	ECS_GetVelocity(e, out);
	ASSERT_NEAR(out[0], 0.0f, 0.0001f, "default velocity x");
	ASSERT_NEAR(out[1], 0.0f, 0.0001f, "default velocity y");
	ASSERT_NEAR(out[2], 0.0f, 0.0001f, "default velocity z");

	ASSERT_NEAR(ECS_GetHealth(e), 100.0f, 0.0001f, "default health");
	ASSERT_EQ(ECS_GetGentityLink(e), -1, "default gentity link");

	ECS_Remove(e, ECS_COMP_HEALTH);
	ASSERT(ECS_Has(e, ECS_COMP_HEALTH) == qfalse, "removed health");
	ASSERT_EQ(ECS_CountWith(ECS_COMP_HEALTH), 0u, "removed health count");
	ASSERT_NEAR(ECS_GetHealth(e), 0.0f, 0.0001f, "missing health reads zero");

	ECS_Destroy(e);
	ASSERT(ECS_Valid(e) == qfalse, "destroyed entity invalid");
	ECS_Shutdown();
	return 0;
}

static int test_setters_auto_add_and_bound_tag(void)
{
	ecs_entity_t e;
	vec3_t out;
	char long_tag[96];
	char expected[ECS_MAX_COMPONENT_NAME];

	ECS_Init();
	e = ECS_Create();

	ECS_SetPosition(e, 1.0f, 2.0f, 3.0f);
	ECS_SetRotation(e, 10.0f, 20.0f, 30.0f);
	ECS_SetScale(e, 2.0f, 3.0f, 4.0f);
	ECS_SetVelocity(e, -1.0f, -2.0f, -3.0f);
	ECS_SetHealth(e, 42.0f);
	ECS_SetGentityLink(e, 7);

	ASSERT(ECS_Has(e, ECS_COMP_POSITION) == qtrue, "set position adds component");
	ASSERT(ECS_Has(e, ECS_COMP_ROTATION) == qtrue, "set rotation adds component");
	ASSERT(ECS_Has(e, ECS_COMP_SCALE) == qtrue, "set scale adds component");
	ASSERT(ECS_Has(e, ECS_COMP_VELOCITY) == qtrue, "set velocity adds component");
	ASSERT(ECS_Has(e, ECS_COMP_HEALTH) == qtrue, "set health adds component");
	ASSERT(ECS_Has(e, ECS_COMP_GENTITY_LINK) == qtrue, "set gentity link adds component");

	ECS_GetPosition(e, out);
	ASSERT_NEAR(out[0], 1.0f, 0.0001f, "set position x");
	ASSERT_NEAR(out[1], 2.0f, 0.0001f, "set position y");
	ASSERT_NEAR(out[2], 3.0f, 0.0001f, "set position z");
	ECS_GetRotation(e, out);
	ASSERT_NEAR(out[0], 10.0f, 0.0001f, "set rotation pitch");
	ASSERT_NEAR(out[1], 20.0f, 0.0001f, "set rotation yaw");
	ASSERT_NEAR(out[2], 30.0f, 0.0001f, "set rotation roll");
	ECS_GetScale(e, out);
	ASSERT_NEAR(out[0], 2.0f, 0.0001f, "set scale x");
	ASSERT_NEAR(out[1], 3.0f, 0.0001f, "set scale y");
	ASSERT_NEAR(out[2], 4.0f, 0.0001f, "set scale z");
	ECS_GetVelocity(e, out);
	ASSERT_NEAR(out[0], -1.0f, 0.0001f, "set velocity x");
	ASSERT_NEAR(out[1], -2.0f, 0.0001f, "set velocity y");
	ASSERT_NEAR(out[2], -3.0f, 0.0001f, "set velocity z");
	ASSERT_NEAR(ECS_GetHealth(e), 42.0f, 0.0001f, "set health");
	ASSERT_EQ(ECS_GetGentityLink(e), 7, "set gentity link");

	std::memset(long_tag, 'A', sizeof(long_tag) - 1);
	long_tag[sizeof(long_tag) - 1] = '\0';
	std::memset(expected, 'A', ECS_MAX_COMPONENT_NAME - 1);
	expected[ECS_MAX_COMPONENT_NAME - 1] = '\0';
	ECS_SetTag(e, long_tag);
	ASSERT(ECS_Has(e, ECS_COMP_TAG) == qtrue, "set tag adds component");
	ASSERT_STREQ(ECS_GetTag(e), expected, "tag is bounded and terminated");

	ECS_SetTag(e, nullptr);
	ASSERT_STREQ(ECS_GetTag(e), "", "NULL tag clears component value");

	ECS_Shutdown();
	return 0;
}

static int test_step_motion_requires_position_and_velocity(void)
{
	ecs_entity_t moving;
	ecs_entity_t position_only;
	ecs_entity_t velocity_only;
	vec3_t out;

	ECS_Init();
	moving = ECS_Create();
	position_only = ECS_Create();
	velocity_only = ECS_Create();

	ECS_SetPosition(moving, 1.0f, 2.0f, 3.0f);
	ECS_SetVelocity(moving, 4.0f, -2.0f, 1.0f);
	ECS_SetPosition(position_only, 10.0f, 20.0f, 30.0f);
	ECS_SetVelocity(velocity_only, 100.0f, 100.0f, 100.0f);

	ECS_StepMotion(0.0f);
	ECS_StepMotion(-1.0f);
	ECS_GetPosition(moving, out);
	ASSERT_NEAR(out[0], 1.0f, 0.0001f, "non-positive dt no-op x");
	ASSERT_NEAR(out[1], 2.0f, 0.0001f, "non-positive dt no-op y");
	ASSERT_NEAR(out[2], 3.0f, 0.0001f, "non-positive dt no-op z");

	ECS_StepMotion(0.5f);
	ECS_GetPosition(moving, out);
	ASSERT_NEAR(out[0], 3.0f, 0.0001f, "motion x");
	ASSERT_NEAR(out[1], 1.0f, 0.0001f, "motion y");
	ASSERT_NEAR(out[2], 3.5f, 0.0001f, "motion z");

	ECS_GetPosition(position_only, out);
	ASSERT_NEAR(out[0], 10.0f, 0.0001f, "position-only x unchanged");
	ASSERT_NEAR(out[1], 20.0f, 0.0001f, "position-only y unchanged");
	ASSERT_NEAR(out[2], 30.0f, 0.0001f, "position-only z unchanged");
	ASSERT(ECS_Has(velocity_only, ECS_COMP_POSITION) == qfalse, "velocity-only did not gain position");

	ECS_Remove(moving, ECS_COMP_VELOCITY);
	ECS_StepMotion(1.0f);
	ECS_GetPosition(moving, out);
	ASSERT_NEAR(out[0], 3.0f, 0.0001f, "removed velocity no-op x");
	ASSERT_NEAR(out[1], 1.0f, 0.0001f, "removed velocity no-op y");
	ASSERT_NEAR(out[2], 3.5f, 0.0001f, "removed velocity no-op z");

	ECS_Shutdown();
	return 0;
}

struct EachResult {
	ecs_entity_t seen[4];
	int count;
};

static void collect_entity(ecs_entity_t e, void *userdata)
{
	EachResult *result = static_cast<EachResult *>(userdata);
	if (result->count < 4) {
		result->seen[result->count] = e;
	}
	result->count++;
}

static int test_each_filters_all_requested_components(void)
{
	ecs_entity_t both;
	ecs_entity_t position_only;
	ecs_entity_t health_only;
	ecs_component_id_t filter[2] = { ECS_COMP_POSITION, ECS_COMP_HEALTH };
	EachResult result = {};

	ECS_Init();
	both = ECS_Create();
	position_only = ECS_Create();
	health_only = ECS_Create();

	ECS_SetPosition(both, 1.0f, 0.0f, 0.0f);
	ECS_SetHealth(both, 75.0f);
	ECS_SetPosition(position_only, 2.0f, 0.0f, 0.0f);
	ECS_SetHealth(health_only, 25.0f);

	ECS_Each(filter, 2, collect_entity, &result);
	ASSERT_EQ(result.count, 1, "each matched exactly one entity");
	ASSERT_EQ(result.seen[0], both, "each returned entity with all components");

	ECS_Each(nullptr, 2, collect_entity, &result);
	ECS_Each(filter, 0, collect_entity, &result);
	ECS_Each(filter, 2, nullptr, &result);
	ASSERT_EQ(result.count, 1, "invalid each inputs are no-ops");

	ECS_Shutdown();
	(void)position_only;
	(void)health_only;
	return 0;
}

static int test_component_name_lookup(void)
{
	ASSERT_EQ(ECS_ComponentFromName("position"), ECS_COMP_POSITION, "lookup position");
	ASSERT_EQ(ECS_ComponentFromName("POSITION"), ECS_COMP_POSITION, "lookup case-insensitive");
	ASSERT_EQ(ECS_ComponentFromName("gentity_LINK"), ECS_COMP_GENTITY_LINK, "lookup mixed-case gentity link");
	ASSERT_EQ(ECS_ComponentFromName(nullptr), ECS_COMP_COUNT, "lookup NULL");
	ASSERT_EQ(ECS_ComponentFromName("missing"), ECS_COMP_COUNT, "lookup invalid");

	ASSERT_STREQ(ECS_ComponentName(ECS_COMP_POSITION), "position", "name position");
	ASSERT_STREQ(ECS_ComponentName(ECS_COMP_GENTITY_LINK), "gentity_link", "name gentity link");
	ASSERT_STREQ(ECS_ComponentName((ecs_component_id_t)-1), "", "name invalid low");
	ASSERT_STREQ(ECS_ComponentName(ECS_COMP_COUNT), "", "name invalid high");
	return 0;
}

int main(void)
{
	if (test_uninitialized_api_is_safe()) return 1;
	if (test_lifecycle_and_component_defaults()) return 1;
	if (test_setters_auto_add_and_bound_tag()) return 1;
	if (test_step_motion_requires_position_and_velocity()) return 1;
	if (test_each_filters_all_requested_components()) return 1;
	if (test_component_name_lookup()) return 1;

	std::printf("PASS: unit_ecs\n");
	return 0;
}
