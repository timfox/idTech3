/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan Forward+ scaffolding: GPU light SSBO, tile index SSBO, compute tile
cull, optional PBR fragment debug overlay (r_forwardPlusDebug). See
docs/RENDERER_2026_ARCHITECTURE_PASS.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_forward_plus.h"
#include "vk_util.h"

#define VK_FP_RECORD_STRIDE (sizeof(float) * 16) /* 4 x vec4 per light */
#define VK_FP_HEADER_BYTES (sizeof(float) * 8) /* 2 x vec4: count/meta + tile grid / viewport */
#define VK_FP_TILE_DIM 16u
#define VK_FP_MAX_TILES (256u * 256u)
#define VK_FP_MAX_PER_TILE 4u
#define VK_FP_PARAM_BYTES 256u
#define VK_FP_DUMMY_LIGHT_FLOATS 32u
#define VK_FP_DUMMY_TILE_UINTS 16u

typedef struct {
	uint32_t tile_grid[2];
	uint32_t total_tiles;
	uint32_t num_lights;
} vk_fp_push_t;

static VkDescriptorSet vk_fp_graphics_descriptor = VK_NULL_HANDLE;
static VkBuffer vk_fp_dummy_light_buf = VK_NULL_HANDLE;
static VkBuffer vk_fp_dummy_tile_buf = VK_NULL_HANDLE;
static VkDeviceMemory vk_fp_dummy_mem = VK_NULL_HANDLE;

static void vk_fp_destroy_dummy_buffers( void )
{
	if ( vk_fp_dummy_light_buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk_fp_dummy_light_buf, NULL );
		vk_fp_dummy_light_buf = VK_NULL_HANDLE;
	}
	if ( vk_fp_dummy_tile_buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk_fp_dummy_tile_buf, NULL );
		vk_fp_dummy_tile_buf = VK_NULL_HANDLE;
	}
	if ( vk_fp_dummy_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk_fp_dummy_mem, NULL );
		vk_fp_dummy_mem = VK_NULL_HANDLE;
	}
}

static void vk_fp_create_dummy_buffers( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr_light, mr_tile;
	VkMemoryAllocateInfo mai;
	uint32_t mem_type;
	VkDeviceSize light_bytes, tile_bytes, tile_off, total;
	byte *ptr;

	if ( vk_fp_dummy_light_buf != VK_NULL_HANDLE ) {
		return;
	}

	light_bytes = (VkDeviceSize)VK_FP_DUMMY_LIGHT_FLOATS * sizeof( float );
	tile_bytes = (VkDeviceSize)VK_FP_DUMMY_TILE_UINTS * sizeof( uint32_t );

	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.pNext = NULL;
	bci.flags = 0;
	bci.size = light_bytes;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bci.queueFamilyIndexCount = 0;
	bci.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk_fp_dummy_light_buf ) );
	qvkGetBufferMemoryRequirements( vk.device, vk_fp_dummy_light_buf, &mr_light );

	tile_off = PAD( light_bytes, mr_light.alignment );

	bci.size = tile_bytes;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk_fp_dummy_tile_buf ) );
	qvkGetBufferMemoryRequirements( vk.device, vk_fp_dummy_tile_buf, &mr_tile );

	total = tile_off + mr_tile.size;
	mem_type = vk_find_memory_type( vk.physical_device, mr_light.memoryTypeBits & mr_tile.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.pNext = NULL;
	mai.allocationSize = total;
	mai.memoryTypeIndex = mem_type;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &vk_fp_dummy_mem ) );
	VK_CHECK( qvkMapMemory( vk.device, vk_fp_dummy_mem, 0, VK_WHOLE_SIZE, 0, (void **)&ptr ) );
	Com_Memset( ptr, 0, (size_t)total );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk_fp_dummy_light_buf, vk_fp_dummy_mem, 0 ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk_fp_dummy_tile_buf, vk_fp_dummy_mem, tile_off ) );
}

static void vk_fp_write_graphics_descriptor( VkBuffer light_buf, VkBuffer tile_buf )
{
	VkDescriptorBufferInfo infos[2];
	VkWriteDescriptorSet writes[2];

	if ( vk_fp_graphics_descriptor == VK_NULL_HANDLE || light_buf == VK_NULL_HANDLE || tile_buf == VK_NULL_HANDLE ) {
		return;
	}

	infos[0].buffer = light_buf;
	infos[0].offset = 0;
	infos[0].range = VK_WHOLE_SIZE;
	infos[1].buffer = tile_buf;
	infos[1].offset = 0;
	infos[1].range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk_fp_graphics_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &infos[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk_fp_graphics_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &infos[1];

	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
}

void vk_forward_plus_create_set_layout( void )
{
	VkDescriptorSetLayoutBinding binds[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;

#ifdef USE_VK_PBR
	if ( vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[2].binding = 2;
	binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.pNext = NULL;
	layout_ci.flags = 0;
	layout_ci.bindingCount = 3;
	layout_ci.pBindings = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.set_layout_forward_plus ) );
	SET_OBJECT_NAME( vk.set_layout_forward_plus, "descriptor set layout - forward+", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT );
#endif
}

void vk_forward_plus_destroy_graphics_layout( void )
{
#ifdef USE_VK_PBR
	if ( vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.set_layout_forward_plus, NULL );
		vk.set_layout_forward_plus = VK_NULL_HANDLE;
	}
#endif
}

void vk_forward_plus_init_graphics_descriptors( void )
{
#ifdef USE_VK_PBR
	VkDescriptorSetAllocateInfo alloc_ci;

	if ( vk.set_layout_forward_plus == VK_NULL_HANDLE || vk.descriptor_pool == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk_fp_graphics_descriptor == VK_NULL_HANDLE ) {
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.pNext = NULL;
		alloc_ci.descriptorPool = vk.descriptor_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.set_layout_forward_plus;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk_fp_graphics_descriptor ) );
	}

	vk_fp_create_dummy_buffers();

	if ( r_forwardPlus && r_forwardPlus->integer && vk.forward_plus.buffer != VK_NULL_HANDLE && vk.forward_plus.tile_buffer != VK_NULL_HANDLE ) {
		vk_fp_write_graphics_descriptor( vk.forward_plus.buffer, vk.forward_plus.tile_buffer );
	} else {
		vk_fp_write_graphics_descriptor( vk_fp_dummy_light_buf, vk_fp_dummy_tile_buf );
	}
#endif
}

static void vk_fp_destroy_buffers( void )
{
	if ( vk.forward_plus.tile_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.tile_buffer, NULL );
		vk.forward_plus.tile_buffer = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.tile_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.forward_plus.tile_memory, NULL );
		vk.forward_plus.tile_memory = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.param_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.param_buffer, NULL );
		vk.forward_plus.param_buffer = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.param_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.forward_plus.param_memory, NULL );
		vk.forward_plus.param_memory = VK_NULL_HANDLE;
	}
	vk.forward_plus.param_mapped = NULL;
	vk.forward_plus.param_buffer_size = 0u;
	vk.forward_plus.tile_capacity_tiles = 0u;
	vk.forward_plus.descriptor = VK_NULL_HANDLE;
}

static void vk_fp_destroy_compute_pipeline( void )
{
	if ( vk.forward_plus.tile_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.forward_plus.tile_pipeline, NULL );
		vk.forward_plus.tile_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.forward_plus.pipeline_layout, NULL );
		vk.forward_plus.pipeline_layout = VK_NULL_HANDLE;
	}
}

void vk_forward_plus_destroy_compute_pipeline( void )
{
	vk_fp_destroy_compute_pipeline();
}

void vk_forward_plus_on_descriptor_pool_destroyed( void )
{
	vk_fp_graphics_descriptor = VK_NULL_HANDLE;
	vk.forward_plus.descriptor = VK_NULL_HANDLE;
}

static void vk_fp_destroy_light_buffer( void )
{
	if ( vk.forward_plus.buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.buffer, NULL );
		vk.forward_plus.buffer = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.forward_plus.memory, NULL );
		vk.forward_plus.memory = VK_NULL_HANDLE;
	}
	vk.forward_plus.mapped = NULL;
	vk.forward_plus.capacity_bytes = 0u;
}

void vk_forward_plus_shutdown( void )
{
	vk_fp_destroy_buffers();
	vk_fp_destroy_light_buffer();
	vk.forward_plus.last_packed_count = 0u;
	vk.forward_plus.tiles_x = 0u;
	vk.forward_plus.tiles_y = 0u;
	vk_fp_destroy_dummy_buffers();
}

static void vk_fp_create_buffers_and_compute( void )
{
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkComputePipelineCreateInfo pipe_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkDescriptorSetAllocateInfo alloc_ci;
	VkDescriptorBufferInfo buf_infos[3];
	VkWriteDescriptorSet writes[3];
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo mai;
	uint32_t mem_type;
	uint32_t vw, vh, tiles_x, tiles_y, total_tiles;
	VkDeviceSize tile_bytes;
	const uint32_t max_lights = (uint32_t)MAX_REAL_DLIGHTS;
	const VkDeviceSize light_buf_size = (VkDeviceSize)VK_FP_HEADER_BYTES + (VkDeviceSize)max_lights * (VkDeviceSize)VK_FP_RECORD_STRIDE;

	vw = (uint32_t)glConfig.vidWidth;
	vh = (uint32_t)glConfig.vidHeight;
	if ( vw < 16u ) {
		vw = 1280u;
	}
	if ( vh < 16u ) {
		vh = 720u;
	}
	tiles_x = ( vw + VK_FP_TILE_DIM - 1u ) / VK_FP_TILE_DIM;
	tiles_y = ( vh + VK_FP_TILE_DIM - 1u ) / VK_FP_TILE_DIM;
	total_tiles = tiles_x * tiles_y;
	if ( total_tiles > VK_FP_MAX_TILES ) {
		total_tiles = VK_FP_MAX_TILES;
		tiles_y = total_tiles / tiles_x;
	}
	tile_bytes = (VkDeviceSize)total_tiles * (VkDeviceSize)VK_FP_MAX_PER_TILE * sizeof( uint32_t );

	vk.forward_plus.tiles_x = tiles_x;
	vk.forward_plus.tiles_y = tiles_y;
	vk.forward_plus.tile_capacity_tiles = total_tiles;

	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.pNext = NULL;
	bci.flags = 0;
	bci.size = light_buf_size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bci.queueFamilyIndexCount = 0;
	bci.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk.forward_plus.buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.forward_plus.buffer, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.pNext = NULL;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &vk.forward_plus.memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.forward_plus.memory, 0, VK_WHOLE_SIZE, 0, &vk.forward_plus.mapped ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.forward_plus.buffer, vk.forward_plus.memory, 0 ) );
	vk.forward_plus.capacity_bytes = (uint32_t)mr.size;
	Com_Memset( vk.forward_plus.mapped, 0, (size_t)mr.size );
	SET_OBJECT_NAME( vk.forward_plus.buffer, "forward+ light records", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	if ( vk.modules.forward_plus_tile_cull_cs == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] forward_plus_tile_cull compute shader missing; tile SSBO disabled\n" S_COLOR_WHITE );
		return;
	}

	bci.size = tile_bytes;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk.forward_plus.tile_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.forward_plus.tile_buffer, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &vk.forward_plus.tile_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.forward_plus.tile_buffer, vk.forward_plus.tile_memory, 0 ) );
	SET_OBJECT_NAME( vk.forward_plus.tile_buffer, "forward+ tile indices", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	bci.size = VK_FP_PARAM_BYTES;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk.forward_plus.param_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.forward_plus.param_buffer, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &vk.forward_plus.param_memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.forward_plus.param_memory, 0, VK_WHOLE_SIZE, 0, &vk.forward_plus.param_mapped ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.forward_plus.param_buffer, vk.forward_plus.param_memory, 0 ) );
	vk.forward_plus.param_buffer_size = (uint32_t)mr.size;
	Com_Memset( vk.forward_plus.param_mapped, 0, (size_t)mr.size );
	SET_OBJECT_NAME( vk.forward_plus.param_buffer, "forward+ tile cull params", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_fp_push_t );

	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.pNext = NULL;
	pl_ci.flags = 0;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.set_layout_forward_plus;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.forward_plus.pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.forward_plus_tile_cull_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.forward_plus.pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.forward_plus.tile_pipeline ) );
	SET_OBJECT_NAME( vk.forward_plus.tile_pipeline, "pipeline - forward+ tile cull", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_ci.pNext = NULL;
	alloc_ci.descriptorPool = vk.descriptor_pool;
	alloc_ci.descriptorSetCount = 1;
	alloc_ci.pSetLayouts = &vk.set_layout_forward_plus;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.forward_plus.descriptor ) );

	Com_Memset( buf_infos, 0, sizeof( buf_infos ) );
	buf_infos[0].buffer = vk.forward_plus.buffer;
	buf_infos[0].offset = 0;
	buf_infos[0].range = VK_WHOLE_SIZE;
	buf_infos[1].buffer = vk.forward_plus.tile_buffer;
	buf_infos[1].offset = 0;
	buf_infos[1].range = VK_WHOLE_SIZE;
	buf_infos[2].buffer = vk.forward_plus.param_buffer;
	buf_infos[2].offset = 0;
	buf_infos[2].range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( int i = 0; i < 3; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk.forward_plus.descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &buf_infos[i];
	}
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	vk_forward_plus_init_graphics_descriptors();

	ri.Printf( PRINT_ALL, "[VK][Forward+] tile cull: %ux%u tiles (%u total), %u bytes/tile list\n",
		(unsigned)tiles_x, (unsigned)tiles_y, (unsigned)total_tiles, (unsigned)tile_bytes );
}

void vk_forward_plus_init( void )
{
	vk_fp_destroy_compute_pipeline();
	vk_fp_destroy_buffers();
	vk_fp_destroy_light_buffer();

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlus=0 (Forward+ scaffolding disabled)\n" );
		vk_forward_plus_init_graphics_descriptors();
		return;
	}

	vk_fp_create_buffers_and_compute();

	ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlus=1 GPU light record buffer %u bytes\n",
		(unsigned)vk.forward_plus.capacity_bytes );
}

void vk_forward_plus_update_for_refdef( void )
{
	float *base;
	uint32_t n;
	unsigned int i;
	uint32_t max_pack;
	const dlight_t *dl;
	float dbg;

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.mapped == NULL ) {
		return;
	}

	base = (float *)vk.forward_plus.mapped;
	n = backEnd.refdef.num_dlights;
	if ( n > (uint32_t)MAX_REAL_DLIGHTS ) {
		n = (uint32_t)MAX_REAL_DLIGHTS;
	}

	max_pack = ( vk.forward_plus.capacity_bytes - (uint32_t)VK_FP_HEADER_BYTES ) / (uint32_t)VK_FP_RECORD_STRIDE;
	if ( n > max_pack ) {
		n = max_pack;
	}

	dbg = ( r_forwardPlusDebug && r_forwardPlusDebug->value > 0.0f ) ? r_forwardPlusDebug->value : 0.0f;

	/* Header vec4: x=count, y=refdef time (ms), z=reserved, w=debug overlay scale */
	base[0] = (float)n;
	base[1] = (float)backEnd.refdef.time;
	base[2] = 0.0f;
	base[3] = dbg;

	/* Tile grid + viewport (for fragment overlay; compute still uses push + param SSBO) */
	base[4] = (float)vk.forward_plus.tiles_x;
	base[5] = (float)vk.forward_plus.tiles_y;
	base[6] = (float)glConfig.vidWidth;
	base[7] = (float)glConfig.vidHeight;

	dl = backEnd.refdef.dlights;
	if ( !dl ) {
		n = 0u;
		base[0] = 0.0f;
	}

	for ( i = 0; i < n; i++ ) {
		const dlight_t *L = dl + i;
		float *rec = base + (uint32_t)( VK_FP_HEADER_BYTES / sizeof( float ) ) + (uint32_t)i * (uint32_t)( VK_FP_RECORD_STRIDE / sizeof( float ) );
		vec3_t dir;
		float len;

		rec[0] = L->origin[0];
		rec[1] = L->origin[1];
		rec[2] = L->origin[2];
		rec[3] = L->radius;

		rec[4] = MAX( L->color[0], 0.0f );
		rec[5] = MAX( L->color[1], 0.0f );
		rec[6] = MAX( L->color[2], 0.0f );
		rec[7] = L->linear ? 1.0f : 0.0f;

		if ( L->linear ) {
			VectorSubtract( L->origin2, L->origin, dir );
			len = VectorNormalize( dir );
			if ( len <= 0.001f ) {
				VectorSet( dir, 0.0f, 0.0f, -1.0f );
			}
			rec[8] = dir[0];
			rec[9] = dir[1];
			rec[10] = dir[2];
			rec[11] = cosf( DEG2RAD( 35.0f ) );
			rec[12] = cosf( DEG2RAD( 20.0f ) );
			rec[13] = len;
			rec[14] = L->additive ? 1.0f : 0.0f;
			rec[15] = 0.0f;
		} else {
			rec[8] = 0.0f;
			rec[9] = 0.0f;
			rec[10] = 0.0f;
			rec[11] = -1.0f;
			rec[12] = -1.0f;
			rec[13] = L->radius;
			rec[14] = L->additive ? 1.0f : 0.0f;
			rec[15] = 0.0f;
		}
	}

	vk.forward_plus.last_packed_count = n;
}

VkDescriptorSet vk_forward_plus_get_graphics_descriptor_set( void )
{
#ifdef USE_VK_PBR
	return vk_fp_graphics_descriptor;
#else
	return VK_NULL_HANDLE;
#endif
}

void vk_forward_plus_dispatch_tile_cull( void )
{
	VkBufferMemoryBarrier barriers[3];
	vk_fp_push_t push;
	float *param_f;
	uint32_t *param_u;
	float clip_from_world[16];

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}
	if ( vk.forward_plus.tile_pipeline == VK_NULL_HANDLE || vk.forward_plus.descriptor == VK_NULL_HANDLE ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass ) {
		return;
	}
	if ( vk.forward_plus.param_mapped == NULL || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ) {
		return;
	}

	myGlMultMatrix( backEnd.viewParms.projectionMatrix, backEnd.viewParms.world.modelViewMatrix, clip_from_world );

	param_f = (float *)vk.forward_plus.param_mapped;
	param_u = (uint32_t *)vk.forward_plus.param_mapped;
	Com_Memcpy( param_f, clip_from_world, sizeof( clip_from_world ) );
	param_u[16] = vk.forward_plus.tiles_x;
	param_u[17] = vk.forward_plus.tiles_y;
	param_u[18] = (uint32_t)glConfig.vidWidth;
	param_u[19] = (uint32_t)glConfig.vidHeight;

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].buffer = vk.forward_plus.buffer;
	barriers[0].offset = 0;
	barriers[0].size = VK_WHOLE_SIZE;

	barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[1].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].buffer = vk.forward_plus.param_buffer;
	barriers[1].offset = 0;
	barriers[1].size = VK_WHOLE_SIZE;

	barriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[2].srcAccessMask = 0;
	barriers[2].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[2].buffer = vk.forward_plus.tile_buffer;
	barriers[2].offset = 0;
	barriers[2].size = VK_WHOLE_SIZE;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 3, barriers, 0, NULL );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.forward_plus.tile_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.forward_plus.pipeline_layout, 0, 1, &vk.forward_plus.descriptor, 0, NULL );

	push.tile_grid[0] = vk.forward_plus.tiles_x;
	push.tile_grid[1] = vk.forward_plus.tiles_y;
	push.total_tiles = vk.forward_plus.tiles_x * vk.forward_plus.tiles_y;
	push.num_lights = vk.forward_plus.last_packed_count;

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.forward_plus.pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

	qvkCmdDispatch( vk.cmd->command_buffer, ( push.total_tiles + 63u ) / 64u, 1, 1 );

	barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].buffer = vk.forward_plus.tile_buffer;
	barriers[0].offset = 0;
	barriers[0].size = VK_WHOLE_SIZE;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 1, barriers, 0, NULL );
}
