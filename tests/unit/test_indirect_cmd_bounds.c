/*
 * Unit test: indirect draw command field bounds vs mesh / index buffer limits.
 *
 * Validates vkGpuSceneDrawCmd_t-style fields before GPU consumption.
 */
#include <stdint.h>
#include <stdio.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

typedef struct {
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t  vertexOffset;
	uint32_t firstInstance;
} gpu_draw_cmd_t;

typedef struct {
	uint32_t indexFirst;
	uint32_t indexCount;
	uint32_t vertexCount;
} gpu_mesh_bounds_t;

static int gpu_draw_cmd_valid(
	const gpu_draw_cmd_t *cmd,
	const gpu_mesh_bounds_t *mesh,
	uint32_t bufferIndexCount,
	uint32_t maxInstances )
{
	uint64_t endIndex;

	if ( !cmd || !mesh ) {
		return 0;
	}
	if ( cmd->instanceCount == 0u || cmd->indexCount == 0u ) {
		return 0;
	}
	if ( cmd->instanceCount > maxInstances ) {
		return 0;
	}
	if ( cmd->firstIndex < mesh->indexFirst ) {
		return 0;
	}
	if ( cmd->indexCount > mesh->indexCount ) {
		return 0;
	}
	endIndex = (uint64_t)cmd->firstIndex + (uint64_t)cmd->indexCount;
	if ( endIndex > (uint64_t)bufferIndexCount ) {
		return 0;
	}
	if ( cmd->vertexOffset < 0 ) {
		return 0;
	}
	if ( (uint32_t)cmd->vertexOffset >= mesh->vertexCount ) {
		return 0;
	}
	return 1;
}

int main( void )
{
	gpu_draw_cmd_t cmd = { 96u, 1u, 100u, 0, 7u };
	gpu_mesh_bounds_t mesh = { 100u, 96u, 512u };

	ASSERT( gpu_draw_cmd_valid( &cmd, &mesh, 512u, 4096u ), "valid cmd" );

	cmd.firstIndex = 500u;
	ASSERT( !gpu_draw_cmd_valid( &cmd, &mesh, 512u, 4096u ), "firstIndex+count OOB" );

	cmd.firstIndex = 100u;
	cmd.indexCount = 500u;
	ASSERT( !gpu_draw_cmd_valid( &cmd, &mesh, 512u, 4096u ), "indexCount exceeds mesh" );

	cmd.indexCount = 96u;
	cmd.vertexOffset = 512;
	ASSERT( !gpu_draw_cmd_valid( &cmd, &mesh, 512u, 4096u ), "vertexOffset OOB" );

	cmd.vertexOffset = 0;
	cmd.instanceCount = 0u;
	ASSERT( !gpu_draw_cmd_valid( &cmd, &mesh, 512u, 4096u ), "zero instances" );

	printf( "unit_indirect_cmd_bounds: PASS\n" );
	return 0;
}
