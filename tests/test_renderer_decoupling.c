/*
===========================================================================
Renderer Decoupling Unit Tests

Tests for the decoupled renderer API that reduces coupling and enables testing.
===========================================================================
*/

#include "test_framework.h"
#include "../src/renderers/renderercommon/tr_public.h"
#include "../src/renderers/renderercommon/tr_types.h"
#include "../src/common/q_shared.h"
#include <stdlib.h>

#define MAX_MOD_KNOWN 1024

// Minimal model_t definition for testing
typedef enum {
	MOD_BAD = 0,
	MOD_BRUSH,
	MOD_MESH,
	MOD_MD4,
	MOD_MDR,
	MOD_IQM
} modtype_t;

typedef struct model_s {
	char name[MAX_QPATH];
	modtype_t type;
	int index;
	int numLods;
} model_t;

// For testing purposes, we'll declare re as extern without redefining the type
extern refexport_t re;

// Mock implementations for common functions used by q_shared.c
void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

void Com_Error(errorParm_t level, const char *fmt, ...) {
	(void)level;
	va_list argptr;
	va_start(argptr, fmt);
	vfprintf(stderr, fmt, argptr);
	va_end(argptr);
	exit(1);
}

// Stub refexport_t structure for testing
refexport_t re = {0};

// Test the mock renderer API
TEST(renderer_mock_basic) {
	const testable_renderer_api_t *api = R_GetMockAPI();
	ASSERT_TRUE(api != NULL);

	// Reset mock state
	R_Mock_ResetState();
	ASSERT_EQ(R_Mock_GetRegisteredModelCount(), 0);
	ASSERT_EQ(R_Mock_GetRegisteredShaderCount(), 0);
	ASSERT_FALSE(R_Mock_WasSceneCleared());
}

TEST(renderer_mock_model_registration) {
	const testable_renderer_api_t *api = R_GetMockAPI();

	R_Mock_ResetState();

	// Test model registration
	qhandle_t model1 = api->RegisterModel("test_model.md3");
	qhandle_t model2 = api->RegisterModel("another_model.md3");

	ASSERT_TRUE(model1 > 0);
	ASSERT_TRUE(model2 > 0);
	ASSERT_EQ(R_Mock_GetRegisteredModelCount(), 2);

	// Test model bounds
	vec3_t mins, maxs;
	api->ModelBounds(model1, mins, maxs);
	ASSERT_EQ(mins[0], -10.0f);
	ASSERT_EQ(maxs[0], 10.0f);
}

TEST(renderer_mock_scene_management) {
	const testable_renderer_api_t *api = R_GetMockAPI();

	R_Mock_ResetState();

	// Test scene management
	api->ClearScene();
	ASSERT_TRUE(R_Mock_WasSceneCleared());

	// Test adding entities
	refEntity_t entity = {0};
	entity.reType = RT_MODEL;
	api->AddRefEntityToScene(&entity);
	ASSERT_EQ(R_Mock_GetEntitiesAdded(), 1);

	// Test adding polys
	polyVert_t verts[3] = {0};
	api->AddPolyToScene(1, 3, verts);
	ASSERT_EQ(R_Mock_GetPolysAdded(), 1);

	// Test adding lights
	vec3_t lightPos = {0, 0, 0};
	api->AddLightToScene(lightPos, 100.0f, 1.0f, 1.0f, 1.0f);
	ASSERT_EQ(R_Mock_GetLightsAdded(), 1);

	// Test rendering
	refdef_t refdef = {0};
	api->RenderScene(&refdef);
	ASSERT_TRUE(R_Mock_WasSceneRendered());
}

TEST(renderer_mock_shader_registration) {
	const testable_renderer_api_t *api = R_GetMockAPI();

	R_Mock_ResetState();

	// Test shader registration
	qhandle_t shader1 = api->RegisterShader("test_shader");
	qhandle_t shader2 = api->RegisterShaderNoMip("test_shader_nomip");

	ASSERT_TRUE(shader1 > 0);
	ASSERT_TRUE(shader2 > 0);
	ASSERT_EQ(R_Mock_GetRegisteredShaderCount(), 2);
}

TEST(renderer_mock_font_registration) {
	const testable_renderer_api_t *api = R_GetMockAPI();

	R_Mock_ResetState();

	// Test font registration
	fontInfo_t font;
	qhandle_t fontHandle = api->RegisterFont("test_font.ttf", 12, &font);

	ASSERT_TRUE(fontHandle > 0);
	ASSERT_EQ(font.pointSize, 12);
}

// Note: Context-aware tests require additional implementation files
// For now, we focus on basic mock API testing

// Test runner
int main(int argc, char **argv) {
	(void)argc; (void)argv;

	RUN_TEST(renderer_mock_basic);
	RUN_TEST(renderer_mock_model_registration);
	RUN_TEST(renderer_mock_scene_management);
	RUN_TEST(renderer_mock_shader_registration);
	RUN_TEST(renderer_mock_font_registration);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}
