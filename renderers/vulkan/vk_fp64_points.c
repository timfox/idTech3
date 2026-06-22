/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Double-precision point cloud visualization (Sözen, arXiv:2408.09699):
native GPU fp64 (dvec3/dmat4) vs Bailey-style emulated high/low vec3 split.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_cmd.h"
#include "vk_fp64_points.h"
#include "vk_pipeline_helpers.h"
#include "vk_util.h"
#include "vk_render_pass.h"
#include "vk_view_state.h"

#define VK_FP64_POINTS_MAX_VERTS_DEFAULT 1000000
#define VK_FP64_PUSH_NATIVE_SIZE 144
#define VK_FP64_PUSH_F32_SIZE 80

typedef struct {
	double x, y, z;
	double cr, cg, cb;
} vk_fp64_native_vertex_t;

typedef struct {
	float highPos[3];
	float lowPos[3];
	float highColor[3];
	float lowColor[3];
} vk_fp64_emulated_vertex_t;

typedef struct {
	float pos[3];
	float color[3];
} vk_fp64_single_vertex_t;

typedef struct {
	double mvp[16];
	float pointSize;
	float pad[3];
} vk_fp64_push_native_t;

typedef struct {
	float mvp[16];
	float pointSize;
	float pad[3];
} vk_fp64_push_f32_t;

static struct {
	qboolean loaded;
	int dimensions;
	uint32_t vertexCount;

	VkBuffer nativeBuffer;
	VkDeviceMemory nativeMemory;
	VkBuffer emulatedBuffer;
	VkDeviceMemory emulatedMemory;
	VkBuffer singleBuffer;
	VkDeviceMemory singleMemory;

	VkPipeline pipelines[VK_FP64_POINTS_MODE_COUNT];
	qboolean pipelinesValid;

	double lastBenchMs[VK_FP64_POINTS_MODE_COUNT];
} fp64;

extern cvar_t *r_fp64Points;
extern cvar_t *r_fp64PointsMode;
extern cvar_t *r_fp64PointsSize;
extern cvar_t *r_fp64PointsMaxVerts;

static void VK_FP64_SplitDouble( double value, float *high, float *low )
{
	const float h = (float)value;
	const double hd = (double)h;

	*high = h;
	*low = (float)( value - hd );
}

static void VK_FP64_ColorFromPosition( double x, double y, double z, double *cr, double *cg, double *cb )
{
	*cr = 0.5 + 0.5 * x;
	*cg = 0.5 + 0.5 * y;
	*cb = 0.5 + 0.5 * z;
}

static void VK_FP64_DestroyBuffer( VkBuffer *buf, VkDeviceMemory *mem )
{
	if ( *buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *buf, NULL );
		*buf = VK_NULL_HANDLE;
	}
	if ( *mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *mem, NULL );
		*mem = VK_NULL_HANDLE;
	}
}

static qboolean VK_FP64_UploadBuffer( const void *data, VkDeviceSize size, VkBuffer *outBuf, VkDeviceMemory *outMem, const char *name )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements memReq;
	VkMemoryAllocateInfo alloc;
	VkCommandBuffer cmd;
	VkBufferCopy region;
	VK_FP64_DestroyBuffer( outBuf, outMem );

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, outBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &memReq );
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc, NULL, outMem ) );
	qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 );

	if ( size > vk.staging_buffer.size ) {
		ri.Printf( PRINT_WARNING, "[VK][fp64] upload %s (%u bytes) exceeds staging buffer\n", name, (unsigned)size );
		return qfalse;
	}

	Com_Memcpy( vk.staging_buffer.ptr, data, (size_t)size );
	cmd = vk_begin_command_buffer();
	region.srcOffset = 0;
	region.dstOffset = 0;
	region.size = size;
	qvkCmdCopyBuffer( cmd, vk.staging_buffer.handle, *outBuf, 1, &region );
	vk_end_command_buffer( cmd, __func__ );

	SET_OBJECT_NAME( *outBuf, name, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	return qtrue;
}

static void VK_FP64_BuildMvpFloat( float outMvp[16] )
{
	float view[16];
	const float *projection;

	if ( backEnd.projection2D ) {
		Matrix16Identity( outMvp );
		return;
	}

	projection = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	Com_Memcpy( view, backEnd.viewParms.world.modelViewMatrix, sizeof( view ) );
	myGlMultMatrix( view, projection, outMvp );
}

static void VK_FP64_BuildMvpDouble( double outMvp[16] )
{
	float mvpF[16];
	int i;

	VK_FP64_BuildMvpFloat( mvpF );
	for ( i = 0; i < 16; i++ ) {
		outMvp[i] = (double)mvpF[i];
	}
}

static qboolean VK_FP64_BuildVertexData( int count, int dimensions, const double *coords )
{
	vk_fp64_native_vertex_t *native;
	vk_fp64_emulated_vertex_t *emulated;
	vk_fp64_single_vertex_t *single;
	VkDeviceSize nativeSize;
	VkDeviceSize emulatedSize;
	VkDeviceSize singleSize;
	int i;

	nativeSize = (VkDeviceSize)count * sizeof( *native );
	emulatedSize = (VkDeviceSize)count * sizeof( *emulated );
	singleSize = (VkDeviceSize)count * sizeof( *single );

	native = (vk_fp64_native_vertex_t *)ri.Hunk_AllocateTempMemory( (int)nativeSize );
	emulated = (vk_fp64_emulated_vertex_t *)ri.Hunk_AllocateTempMemory( (int)emulatedSize );
	single = (vk_fp64_single_vertex_t *)ri.Hunk_AllocateTempMemory( (int)singleSize );
	if ( !native || !emulated || !single ) {
		return qfalse;
	}

	for ( i = 0; i < count; i++ ) {
		const double x = coords[i * 3 + 0];
		const double y = coords[i * 3 + 1];
		const double z = ( dimensions >= 3 ) ? coords[i * 3 + 2] : 0.0;
		double cr, cg, cb;

		VK_FP64_ColorFromPosition( x, y, z, &cr, &cg, &cb );

		native[i].x = x;
		native[i].y = y;
		native[i].z = z;
		native[i].cr = cr;
		native[i].cg = cg;
		native[i].cb = cb;

		VK_FP64_SplitDouble( x, &emulated[i].highPos[0], &emulated[i].lowPos[0] );
		VK_FP64_SplitDouble( y, &emulated[i].highPos[1], &emulated[i].lowPos[1] );
		VK_FP64_SplitDouble( z, &emulated[i].highPos[2], &emulated[i].lowPos[2] );
		VK_FP64_SplitDouble( cr, &emulated[i].highColor[0], &emulated[i].lowColor[0] );
		VK_FP64_SplitDouble( cg, &emulated[i].highColor[1], &emulated[i].lowColor[1] );
		VK_FP64_SplitDouble( cb, &emulated[i].highColor[2], &emulated[i].lowColor[2] );

		single[i].pos[0] = (float)x;
		single[i].pos[1] = (float)y;
		single[i].pos[2] = (float)z;
		single[i].color[0] = (float)cr;
		single[i].color[1] = (float)cg;
		single[i].color[2] = (float)cb;
	}

	if ( !VK_FP64_UploadBuffer( native, nativeSize, &fp64.nativeBuffer, &fp64.nativeMemory, "fp64 native VBO" ) ||
		!VK_FP64_UploadBuffer( emulated, emulatedSize, &fp64.emulatedBuffer, &fp64.emulatedMemory, "fp64 emulated VBO" ) ||
		!VK_FP64_UploadBuffer( single, singleSize, &fp64.singleBuffer, &fp64.singleMemory, "fp64 single VBO" ) ) {
		ri.Hunk_FreeTempMemory( native );
		ri.Hunk_FreeTempMemory( emulated );
		ri.Hunk_FreeTempMemory( single );
		return qfalse;
	}

	ri.Hunk_FreeTempMemory( native );
	ri.Hunk_FreeTempMemory( emulated );
	ri.Hunk_FreeTempMemory( single );

	fp64.vertexCount = (uint32_t)count;
	fp64.dimensions = dimensions;
	fp64.loaded = qtrue;
	return qtrue;
}

static VkPipeline VK_FP64_CreatePointPipeline(
	VkShaderModule vs,
	VkShaderModule fs,
	const VkVertexInputBindingDescription *bindings,
	uint32_t bindingCount,
	const VkVertexInputAttributeDescription *attribs,
	uint32_t attribCount,
	uint32_t pushSize )
{
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAsm;
	VkPipelineRasterizationStateCreateInfo raster;
	VkPipelineMultisampleStateCreateInfo msaa;
	VkPipelineDepthStencilStateCreateInfo depth;
	VkPipelineColorBlendAttachmentState blendAttach;
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineViewportStateCreateInfo viewportState;
	VkDynamicState dynamicStates[2];
	VkPipelineDynamicStateCreateInfo dynamic;
	VkGraphicsPipelineCreateInfo pipe;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo layoutDesc;
	VkPipelineLayout layout;
	VkPipeline result;

	if ( vk.pipeline_layout_fp64_points == VK_NULL_HANDLE ) {
		Com_Memset( &pushRange, 0, sizeof( pushRange ) );
		pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushRange.offset = 0;
		pushRange.size = VK_FP64_PUSH_NATIVE_SIZE;

		Com_Memset( &layoutDesc, 0, sizeof( layoutDesc ) );
		layoutDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutDesc.pushConstantRangeCount = 1;
		layoutDesc.pPushConstantRanges = &pushRange;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &layoutDesc, NULL, &vk.pipeline_layout_fp64_points ) );
		SET_OBJECT_NAME( vk.pipeline_layout_fp64_points, "pipeline layout - fp64 points", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
	}

	Com_Memset( stages, 0, sizeof( stages ) );
	vk_set_shader_stage_desc( stages + 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main" );
	vk_set_shader_stage_desc( stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main" );

	Com_Memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = bindingCount;
	vertexInput.pVertexBindingDescriptions = bindings;
	vertexInput.vertexAttributeDescriptionCount = attribCount;
	vertexInput.pVertexAttributeDescriptions = attribs;

	Com_Memset( &inputAsm, 0, sizeof( inputAsm ) );
	inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

	Com_Memset( &raster, 0, sizeof( raster ) );
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_POINT;
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster.lineWidth = 1.0f;

	Com_Memset( &msaa, 0, sizeof( msaa ) );
	msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msaa.rasterizationSamples = vk_get_main_rasterization_samples();

	Com_Memset( &depth, 0, sizeof( depth ) );
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthTestEnable = VK_TRUE;
	depth.depthWriteEnable = VK_TRUE;
	depth.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

	Com_Memset( &blendAttach, 0, sizeof( blendAttach ) );
	blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blendAttach;

	Com_Memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	Com_Memset( &dynamic, 0, sizeof( dynamic ) );
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = ARRAY_LEN( dynamicStates );
	dynamic.pDynamicStates = dynamicStates;

	Com_Memset( &pipe, 0, sizeof( pipe ) );
	pipe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipe.stageCount = 2;
	pipe.pStages = stages;
	pipe.pVertexInputState = &vertexInput;
	pipe.pInputAssemblyState = &inputAsm;
	pipe.pViewportState = &viewportState;
	pipe.pRasterizationState = &raster;
	pipe.pMultisampleState = &msaa;
	pipe.pDepthStencilState = &depth;
	pipe.pColorBlendState = &blend;
	pipe.pDynamicState = &dynamic;
	pipe.layout = vk.pipeline_layout_fp64_points;
	pipe.renderPass = vk.render_pass.main;
	pipe.subpass = 0;

	(void)pushSize;
	layout = vk.pipeline_layout_fp64_points;
	pipe.layout = layout;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &pipe, NULL, &result ) );
	return result;
}

void VK_FP64_PointsCreatePipelines( void )
{
	VkVertexInputBindingDescription nativeBinding;
	VkVertexInputAttributeDescription nativeAttribs[2];
	VkVertexInputBindingDescription emulatedBinding;
	VkVertexInputAttributeDescription emulatedAttribs[4];
	VkVertexInputBindingDescription singleBinding;
	VkVertexInputAttributeDescription singleAttribs[2];
	int m;

	if ( !vk.shaderFloat64 || vk.modules.fp64_points_native_vs == VK_NULL_HANDLE ) {
		fp64.pipelinesValid = qfalse;
		return;
	}

	for ( m = 0; m < VK_FP64_POINTS_MODE_COUNT; m++ ) {
		if ( fp64.pipelines[m] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, fp64.pipelines[m], NULL );
			fp64.pipelines[m] = VK_NULL_HANDLE;
		}
	}

	nativeBinding.binding = 0;
	nativeBinding.stride = sizeof( vk_fp64_native_vertex_t );
	nativeBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	nativeAttribs[0].location = 0;
	nativeAttribs[0].binding = 0;
	nativeAttribs[0].format = VK_FORMAT_R64G64B64_SFLOAT;
	nativeAttribs[0].offset = offsetof( vk_fp64_native_vertex_t, x );
	nativeAttribs[1].location = 1;
	nativeAttribs[1].binding = 0;
	nativeAttribs[1].format = VK_FORMAT_R64G64B64_SFLOAT;
	nativeAttribs[1].offset = offsetof( vk_fp64_native_vertex_t, cr );

	fp64.pipelines[VK_FP64_POINTS_MODE_NATIVE] = VK_FP64_CreatePointPipeline(
		vk.modules.fp64_points_native_vs, vk.modules.fp64_points_native_fs,
		&nativeBinding, 1, nativeAttribs, 2, VK_FP64_PUSH_NATIVE_SIZE );

	emulatedBinding.binding = 0;
	emulatedBinding.stride = sizeof( vk_fp64_emulated_vertex_t );
	emulatedBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	emulatedAttribs[0] = (VkVertexInputAttributeDescription){ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( vk_fp64_emulated_vertex_t, highPos ) };
	emulatedAttribs[1] = (VkVertexInputAttributeDescription){ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( vk_fp64_emulated_vertex_t, lowPos ) };
	emulatedAttribs[2] = (VkVertexInputAttributeDescription){ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( vk_fp64_emulated_vertex_t, highColor ) };
	emulatedAttribs[3] = (VkVertexInputAttributeDescription){ 3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( vk_fp64_emulated_vertex_t, lowColor ) };

	fp64.pipelines[VK_FP64_POINTS_MODE_EMULATED] = VK_FP64_CreatePointPipeline(
		vk.modules.fp64_points_emulated_vs, vk.modules.fp64_points_emulated_fs,
		&emulatedBinding, 1, emulatedAttribs, 4, VK_FP64_PUSH_F32_SIZE );

	singleBinding.binding = 0;
	singleBinding.stride = sizeof( vk_fp64_single_vertex_t );
	singleBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	singleAttribs[0] = (VkVertexInputAttributeDescription){ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( vk_fp64_single_vertex_t, pos ) };
	singleAttribs[1] = (VkVertexInputAttributeDescription){ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( vk_fp64_single_vertex_t, color ) };

	fp64.pipelines[VK_FP64_POINTS_MODE_SINGLE] = VK_FP64_CreatePointPipeline(
		vk.modules.fp64_points_single_vs, vk.modules.fp64_points_single_fs,
		&singleBinding, 1, singleAttribs, 2, VK_FP64_PUSH_F32_SIZE );

	fp64.pipelinesValid = qtrue;
	ri.Printf( PRINT_ALL, "[VK][fp64] point pipelines ready (native/emulated/single)\n" );
}

void VK_FP64_PointsInit( void )
{
	Com_Memset( &fp64, 0, sizeof( fp64 ) );
}

void VK_FP64_PointsShutdown( void )
{
	int m;

	VK_FP64_PointsClear();

	for ( m = 0; m < VK_FP64_POINTS_MODE_COUNT; m++ ) {
		if ( fp64.pipelines[m] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, fp64.pipelines[m], NULL );
			fp64.pipelines[m] = VK_NULL_HANDLE;
		}
	}

	if ( vk.pipeline_layout_fp64_points != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_fp64_points, NULL );
		vk.pipeline_layout_fp64_points = VK_NULL_HANDLE;
	}

	fp64.pipelinesValid = qfalse;
}

void VK_FP64_PointsClear( void )
{
	VK_FP64_DestroyBuffer( &fp64.nativeBuffer, &fp64.nativeMemory );
	VK_FP64_DestroyBuffer( &fp64.emulatedBuffer, &fp64.emulatedMemory );
	VK_FP64_DestroyBuffer( &fp64.singleBuffer, &fp64.singleMemory );
	fp64.loaded = qfalse;
	fp64.vertexCount = 0;
}

qboolean VK_FP64_PointsReady( void )
{
	return fp64.loaded && fp64.vertexCount > 0;
}

uint32_t VK_FP64_PointsVertexCount( void )
{
	return fp64.vertexCount;
}

static qboolean VK_FP64_ParseCsvLine( const char *line, double *x, double *y, double *z, int dimensions )
{
	int n;

	*x = *y = *z = 0.0;
	if ( dimensions >= 3 ) {
		n = sscanf( line, " %lf , %lf , %lf", x, y, z );
		if ( n < 3 ) {
			n = sscanf( line, " %lf %lf %lf", x, y, z );
		}
		if ( n < 3 ) {
			return qfalse;
		}
	} else {
		n = sscanf( line, " %lf , %lf", x, y );
		if ( n < 2 ) {
			n = sscanf( line, " %lf %lf", x, y );
		}
		if ( n < 2 ) {
			return qfalse;
		}
		*z = 0.0;
	}
	return qtrue;
}

qboolean VK_FP64_PointsLoadCsv( const char *path, int dimensions )
{
	int len;
	char *buf;
	char *cursor;
	int maxVerts;
	int count;
	double *coords;
	void *fileBuf;

	if ( !path || !path[0] ) {
		return qfalse;
	}
	if ( dimensions != 2 && dimensions != 3 ) {
		dimensions = 3;
	}

	len = ri.FS_ReadFile( path, &fileBuf );
	if ( len <= 0 || !fileBuf ) {
		ri.Printf( PRINT_WARNING, "[VK][fp64] could not read '%s'\n", path );
		return qfalse;
	}

	buf = (char *)ri.Hunk_AllocateTempMemory( len + 1 );
	if ( !buf ) {
		ri.FS_FreeFile( fileBuf );
		return qfalse;
	}
	Com_Memcpy( buf, fileBuf, len );
	ri.FS_FreeFile( fileBuf );
	buf[len] = '\0';

	maxVerts = r_fp64PointsMaxVerts ? r_fp64PointsMaxVerts->integer : VK_FP64_POINTS_MAX_VERTS_DEFAULT;
	if ( maxVerts < 1000 ) {
		maxVerts = 1000;
	}

	coords = (double *)ri.Hunk_AllocateTempMemory( maxVerts * 3 * (int)sizeof( double ) );
	if ( !coords ) {
		ri.Hunk_FreeTempMemory( buf );
		return qfalse;
	}

	count = 0;
	cursor = buf;
	while ( cursor < buf + len && count < maxVerts ) {
		char lineBuf[MAX_STRING_CHARS];
		char *eol;
		int lineLen;

		while ( cursor < buf + len && ( *cursor == '\r' || *cursor == '\n' || *cursor == ' ' || *cursor == '\t' ) ) {
			cursor++;
		}
		if ( cursor >= buf + len ) {
			break;
		}

		eol = strchr( cursor, '\n' );
		lineLen = eol ? (int)( eol - cursor ) : (int)( ( buf + len ) - cursor );
		if ( lineLen >= (int)sizeof( lineBuf ) ) {
			lineLen = (int)sizeof( lineBuf ) - 1;
		}
		Q_strncpyz( lineBuf, cursor, lineLen + 1 );
		cursor += lineLen;
		if ( eol ) {
			cursor++;
		}

		if ( !lineBuf[0] || lineBuf[0] == '#' ) {
			continue;
		}
		if ( Q_stricmpn( lineBuf, "x", 1 ) == 0 || Q_stricmpn( lineBuf, "X", 1 ) == 0 ) {
			continue;
		}
		{
			double x, y, z;

			if ( VK_FP64_ParseCsvLine( lineBuf, &x, &y, &z, dimensions ) ) {
				coords[count * 3 + 0] = x;
				coords[count * 3 + 1] = y;
				coords[count * 3 + 2] = z;
				count++;
			}
		}
	}

	ri.Hunk_FreeTempMemory( buf );

	if ( count < 1 ) {
		ri.Hunk_FreeTempMemory( coords );
		ri.Printf( PRINT_WARNING, "[VK][fp64] no vertices in '%s'\n", path );
		return qfalse;
	}

	VK_FP64_PointsClear();
	if ( !VK_FP64_BuildVertexData( count, dimensions, coords ) ) {
		ri.Hunk_FreeTempMemory( coords );
		return qfalse;
	}

	ri.Hunk_FreeTempMemory( coords );
	ri.Printf( PRINT_ALL, "[VK][fp64] loaded %d points (%dD) from %s\n", count, dimensions, path );
	return qtrue;
}

qboolean VK_FP64_PointsGenerate( int count, int dimensions )
{
	double *coords;
	int i;
	int maxVerts;

	if ( dimensions != 2 && dimensions != 3 ) {
		dimensions = 2;
	}
	maxVerts = r_fp64PointsMaxVerts ? r_fp64PointsMaxVerts->integer : VK_FP64_POINTS_MAX_VERTS_DEFAULT;
	if ( count < 1 || count > maxVerts ) {
		ri.Printf( PRINT_WARNING, "[VK][fp64] count %d out of range (1..%d)\n", count, maxVerts );
		return qfalse;
	}

	coords = (double *)ri.Hunk_AllocateTempMemory( count * 3 * (int)sizeof( double ) );
	if ( !coords ) {
		return qfalse;
	}

	for ( i = 0; i < count; i++ ) {
		coords[i * 3 + 0] = -1.0 + 2.0 * ( (double)rand() / (double)RAND_MAX );
		coords[i * 3 + 1] = -1.0 + 2.0 * ( (double)rand() / (double)RAND_MAX );
		coords[i * 3 + 2] = ( dimensions >= 3 ) ? ( -1.0 + 2.0 * ( (double)rand() / (double)RAND_MAX ) ) : 0.0;
	}

	VK_FP64_PointsClear();
	if ( !VK_FP64_BuildVertexData( count, dimensions, coords ) ) {
		ri.Hunk_FreeTempMemory( coords );
		return qfalse;
	}

	ri.Hunk_FreeTempMemory( coords );
	ri.Printf( PRINT_ALL, "[VK][fp64] generated %d random %dD points\n", count, dimensions );
	return qtrue;
}

static void VK_FP64_DrawMode( vk_fp64_points_mode_t mode )
{
	VkBuffer buffer;
	VkDeviceSize offset = 0;
	vk_fp64_push_native_t pushNative;
	vk_fp64_push_f32_t pushF32;
	uint32_t width;
	uint32_t height;
	float pointSize;

	if ( !fp64.pipelinesValid || fp64.pipelines[mode] == VK_NULL_HANDLE || !fp64.loaded ) {
		return;
	}

	if ( mode == VK_FP64_POINTS_MODE_NATIVE && !vk.shaderFloat64 ) {
		return;
	}

	switch ( mode ) {
	case VK_FP64_POINTS_MODE_NATIVE:
		buffer = fp64.nativeBuffer;
		break;
	case VK_FP64_POINTS_MODE_EMULATED:
		buffer = fp64.emulatedBuffer;
		break;
	default:
		buffer = fp64.singleBuffer;
		break;
	}

	pointSize = r_fp64PointsSize ? r_fp64PointsSize->value : 2.0f;
	width = vk_get_render_target_width();
	height = vk_get_render_target_height();

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fp64.pipelines[mode] );

	if ( mode == VK_FP64_POINTS_MODE_NATIVE ) {
		VK_FP64_BuildMvpDouble( pushNative.mvp );
		pushNative.pointSize = pointSize;
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_fp64_points,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushNative ), &pushNative );
	} else {
		VK_FP64_BuildMvpFloat( pushF32.mvp );
		pushF32.pointSize = pointSize;
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_fp64_points,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushF32 ), &pushF32 );
	}

	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 1, &buffer, &offset );
	qvkCmdDraw( vk.cmd->command_buffer, fp64.vertexCount, 1, 0, 0 );
}

void VK_FP64_PointsDraw( void )
{
	int mode;

	if ( !r_fp64Points || !r_fp64Points->integer ) {
		return;
	}
	if ( !VK_FP64_PointsReady() || !fp64.pipelinesValid ) {
		return;
	}
	if ( vk.renderPassIndex != RENDER_PASS_MAIN || backEnd.projection2D ) {
		return;
	}
	if ( vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	mode = r_fp64PointsMode ? r_fp64PointsMode->integer : 0;
	if ( mode < 0 || mode >= VK_FP64_POINTS_MODE_COUNT ) {
		mode = 0;
	}

	VK_FP64_DrawMode( (vk_fp64_points_mode_t)mode );
}

void VK_FP64_PointsBenchmark( int frames )
{
	int f;
	int mode;
	int start;
	int end;

	if ( frames < 1 ) {
		frames = 60;
	}
	if ( !VK_FP64_PointsReady() ) {
		ri.Printf( PRINT_WARNING, "[VK][fp64] benchmark: load or generate points first\n" );
		return;
	}
	if ( !fp64.pipelinesValid ) {
		ri.Printf( PRINT_WARNING, "[VK][fp64] benchmark: pipelines not ready (shaderFloat64=%s)\n",
			vk.shaderFloat64 ? "yes" : "no" );
		return;
	}

	ri.Printf( PRINT_ALL, "[VK][fp64] benchmark %d frames, %u vertices\n", frames, fp64.vertexCount );

	for ( mode = 0; mode < VK_FP64_POINTS_MODE_COUNT; mode++ ) {
		if ( mode == VK_FP64_POINTS_MODE_NATIVE && !vk.shaderFloat64 ) {
			fp64.lastBenchMs[mode] = -1.0;
			ri.Printf( PRINT_ALL, "  mode %d (native): skipped (no shaderFloat64)\n", mode );
			continue;
		}

		start = ri.Milliseconds();
		for ( f = 0; f < frames; f++ ) {
			VK_FP64_DrawMode( (vk_fp64_points_mode_t)mode );
		}
		end = ri.Milliseconds();
		fp64.lastBenchMs[mode] = (double)( end - start ) / (double)frames;
		ri.Printf( PRINT_ALL, "  mode %d: %.3f ms/frame (%.1f fps @ this draw cost only)\n",
			mode, fp64.lastBenchMs[mode],
			fp64.lastBenchMs[mode] > 0.0 ? 1000.0 / fp64.lastBenchMs[mode] : 0.0 );
	}
}
