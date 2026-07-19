/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Tessellation vertex/index upload, descriptor and pipeline binding, draws.
Split from vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_meshlets.h"

#ifdef USE_VK_PBR
#include "vk_forward_plus.h"
#endif

#ifdef USE_VK_PBR
static VkBuffer shade_bufs[10];
#else
static VkBuffer shade_bufs[8];
#endif
static int bind_base;
static int bind_count;

static void vk_bind_index_attr( int index )
{
	if ( bind_base == -1 ) {
		bind_base = index;
		bind_count = 1;
	} else {
		bind_count = index - bind_base + 1;
	}
}


static void vk_bind_attr( int index, unsigned int item_size, const void *src ) {
	const uint32_t offset = PAD( vk.cmd->vertex_buffer_offset, 32 );
	const uint32_t size = tess.numVertexes * item_size;

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
	} else {
		vk.cmd->buf_offset[ index ] = offset;
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr( index );
}

void *vk_alloc_storage( size_t size, uint32_t *offset )
{
	const uint32_t aligned = PAD( vk.cmd->vertex_buffer_offset, (VkDeviceSize)vk.storage_alignment );
	const uint32_t size32 = (uint32_t)size;

	if ( aligned + size32 > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( aligned + size32, 1 );
		return NULL;
	}

	if ( offset ) {
		*offset = aligned;
	}

	vk.cmd->vertex_buffer_offset = (VkDeviceSize)aligned + size32;
	return vk.cmd->vertex_buffer_ptr + aligned;
}

void vk_set_iqm_storage_offsets( uint32_t skin_offset, uint32_t morph_offset, uint32_t topo_offset )
{
	if ( !vk.cmd ) {
		return;
	}

	vk.cmd->iqm_skin_offset = skin_offset;
	vk.cmd->iqm_morph_offset = morph_offset;
	vk.cmd->gltf_topo_offset = topo_offset;
}

void vk_reset_iqm_storage_offsets( void )
{
	vk_set_iqm_storage_offsets( 0, 0, 0 );
}


uint32_t vk_tess_index( uint32_t numIndexes, const void *src ) {
	const uint32_t offset = vk.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof( tess.indexes[0] );

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
		return ~0U;
	} else {
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
		return offset;
	}
}


void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset )
{
	if ( vk.cmd->curr_index_buffer != buffer || vk.cmd->curr_index_offset != offset )
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32 );

	vk.cmd->curr_index_buffer = buffer;
	vk.cmd->curr_index_offset = offset;
}


#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex )
{
	qvkCmdDrawIndexed( vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0 );
}
#endif


void vk_bind_index( void )
{
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		vk.cmd->num_indexes = 0;
		//qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext( tess.numIndexes, tess.indexes );
}


void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes )
{
	uint32_t offset	= vk_tess_index( numIndexes, indexes );
	if ( offset != ~0U ) {
		vk_bind_index_buffer( vk.cmd->vertex_buffer, offset );
		vk.cmd->num_indexes = numIndexes;
	} else {
		// overflowed
		vk.cmd->num_indexes = 0;
	}
}


void vk_bind_geometry( uint32_t flags )
{
	//unsigned int size;
	bind_base = -1;
	bind_count = 0;

	if ( ( flags & ( TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2 ) ) == 0 )
		return;

	/* glTF VBO path: bind per-primitive buffers instead of tess */
	if ( tess.gltfDrawSurface ) {
		const struct srfGLTFPrimitive_s *surf = tess.gltfDrawSurface;
		tess.gltfDrawSurface = NULL;
		if ( surf->vbo_vertex != TR_GLTF_VBO_HANDLE_INVALID && surf->vbo_index != TR_GLTF_VBO_HANDLE_INVALID &&
		     surf->numVertices > 0 && surf->numIndices > 0 ) {
			VkBuffer bufs[17];
			VkDeviceSize offs[17];
			VkBuffer vbo = (VkBuffer)(uintptr_t)surf->vbo_vertex;
			VkBuffer ibo = (VkBuffer)(uintptr_t)surf->vbo_index;
			int bi;
			for ( bi = 0; bi < 17; bi++ ) {
				bufs[bi] = vbo;
				offs[bi] = 0;
			}
			offs[0] = (VkDeviceSize)surf->vbo_vertex_offsets[0];
			offs[1] = (VkDeviceSize)surf->vbo_vertex_offsets[1];
			offs[2] = (VkDeviceSize)surf->vbo_vertex_offsets[2];
			offs[5] = (VkDeviceSize)surf->vbo_vertex_offsets[5];
			offs[15] = (VkDeviceSize)surf->vbo_vertex_offsets[6];
			offs[16] = (VkDeviceSize)surf->vbo_vertex_offsets[7];
			qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 17, bufs, offs );
			vk_bind_index_buffer( ibo, 0 );
			vk.cmd->num_indexes = surf->numIndices;
		} else {
			vk.cmd->num_indexes = 0;
		}
		return;
	}

#ifdef USE_VBO
	if ( tess.vboIndex ) {
		if ( tess.vboStreamItem && vk.vbo.stream_vertex_buffer ) {
			const stream_vbo_item_t *item = tess.vboStreamItem;
			const stream_vbo_stage_t *stage = &item->stages[tess.vboStage];

			shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] =
				shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.stream_vertex_buffer;
#ifdef USE_VK_PBR
			shade_bufs[8] = vk.vbo.stream_vertex_buffer;
			shade_bufs[9] = vk.vbo.stream_vertex_buffer;
#endif

			if ( flags & TESS_XYZ ) {
				vk.cmd->vbo_offset[0] = item->vboOffset;
				vk_bind_index_attr( 0 );
			}
			if ( flags & TESS_RGBA0 ) {
				vk.cmd->vbo_offset[1] = stage->rgb_offset[0];
				vk_bind_index_attr( 1 );
			}
			if ( flags & TESS_ST0 ) {
				vk.cmd->vbo_offset[2] = stage->tex_offset[0];
				vk_bind_index_attr( 2 );
			}
			if ( flags & TESS_ST1 ) {
				vk.cmd->vbo_offset[3] = stage->tex_offset[1];
				vk_bind_index_attr( 3 );
			}
			if ( flags & TESS_ST2 ) {
				vk.cmd->vbo_offset[4] = stage->tex_offset[2];
				vk_bind_index_attr( 4 );
			}
			if ( flags & TESS_NNN ) {
				vk.cmd->vbo_offset[5] = item->normalOffset;
				vk_bind_index_attr( 5 );
			}
			if ( flags & TESS_RGBA1 ) {
				vk.cmd->vbo_offset[6] = stage->rgb_offset[1];
				vk_bind_index_attr( 6 );
			}
			if ( flags & TESS_RGBA2 ) {
				vk.cmd->vbo_offset[7] = stage->rgb_offset[2];
				vk_bind_index_attr( 7 );
			}
#ifdef USE_VK_PBR
			if ( flags & TESS_PBR ) {
				vk.cmd->vbo_offset[5] = item->normalOffset;
				vk_bind_index_attr( 5 );
				vk.cmd->vbo_offset[8] = item->qtangentOffset;
				vk_bind_index_attr( 8 );
				vk.cmd->vbo_offset[9] = item->lightdirOffset;
				vk_bind_index_attr( 9 );
			}
#endif

			qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );
			return;
		}

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.vertex_buffer;
#ifdef USE_VK_PBR
		shade_bufs[8] = vk.vbo.vertex_buffer;
		shade_bufs[9] = vk.vbo.vertex_buffer;
#endif

		if ( flags & TESS_XYZ ) {  // 0
			vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr( 0 );
		}

		if ( flags & TESS_RGBA0 ) { // 1
			vk.cmd->vbo_offset[1] = tess.shader->stages[ tess.vboStage ]->rgb_offset[0];
			vk_bind_index_attr( 1 );
		}

		if ( flags & TESS_ST0 ) {  // 2
			vk.cmd->vbo_offset[2] = tess.shader->stages[ tess.vboStage ]->tex_offset[0];
			vk_bind_index_attr( 2 );
		}

		if ( flags & TESS_ST1 ) {  // 3
			vk.cmd->vbo_offset[3] = tess.shader->stages[ tess.vboStage ]->tex_offset[1];
			vk_bind_index_attr( 3 );
		}

		if ( flags & TESS_ST2 ) {  // 4
			vk.cmd->vbo_offset[4] = tess.shader->stages[ tess.vboStage ]->tex_offset[2];
			vk_bind_index_attr( 4 );
		}

		if ( flags & TESS_NNN ) { // 5
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );
		}

		if ( flags & TESS_RGBA1 ) { // 6
			vk.cmd->vbo_offset[6] = tess.shader->stages[ tess.vboStage ]->rgb_offset[1];
			vk_bind_index_attr( 6 );
		}

		if ( flags & TESS_RGBA2 ) { // 7
			vk.cmd->vbo_offset[7] = tess.shader->stages[ tess.vboStage ]->rgb_offset[2];
			vk_bind_index_attr( 7 );
		}
#ifdef USE_VK_PBR
		if (flags & TESS_PBR) {
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );

			vk.cmd->vbo_offset[8] = tess.shader->qtangentOffset;
			vk_bind_index_attr(8);

			vk.cmd->vbo_offset[9] = tess.shader->lightdirOffset;
			vk_bind_index_attr(9);
		}
#endif

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );

	} else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.cmd->vertex_buffer;
#ifdef USE_VK_PBR
		shade_bufs[8] = vk.cmd->vertex_buffer;
		shade_bufs[9] = vk.cmd->vertex_buffer;
#endif

		if ( flags & TESS_XYZ ) {
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if ( flags & TESS_RGBA0 ) {
			vk_bind_attr(1, sizeof( color4ub_t ), tess.svars.colors[0][0].rgba);
		}

		if ( flags & TESS_ST0 ) {
			vk_bind_attr(2, sizeof( vec2_t ), tess.svars.texcoordPtr[0]);
		}

		if ( flags & TESS_ST1 ) {
			vk_bind_attr(3, sizeof( vec2_t ), tess.svars.texcoordPtr[1]);
		}

		if ( flags & TESS_ST2 ) {
			vk_bind_attr(4, sizeof( vec2_t ), tess.svars.texcoordPtr[2]);
		}

		if ( flags & TESS_NNN ) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if ( flags & TESS_RGBA1 ) {
			vk_bind_attr(6, sizeof( color4ub_t ), tess.svars.colors[1][0].rgba);
		}

		if ( flags & TESS_RGBA2 ) {
			vk_bind_attr(7, sizeof( color4ub_t ), tess.svars.colors[2][0].rgba);
		}
#ifdef USE_VK_PBR
		if (flags & TESS_PBR) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
			vk_bind_attr(8, sizeof(tess.qtangent[0]), tess.qtangent);
			vk_bind_attr(9, sizeof(tess.lightdir[0]), tess.lightdir);
		}
#endif

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_bind_lighting( int stage, int bundle )
{
	bind_base = -1;
	bind_count = 0;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.vbo.vertex_buffer;

		vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk.cmd->vbo_offset[1] = tess.shader->stages[ stage ]->tex_offset[ bundle ];
		vk.cmd->vbo_offset[2] = tess.shader->normalOffset;

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 3, shade_bufs, vk.cmd->vbo_offset + 0 );

	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.cmd->vertex_buffer;

		vk_bind_attr( 0, sizeof( tess.xyz[0] ), &tess.xyz[0] );
		vk_bind_attr( 1, sizeof( vec2_t ), tess.svars.texcoordPtr[ bundle ] );
		vk_bind_attr( 2, sizeof( tess.normal[0] ), tess.normal );

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_reset_descriptor( int index )
{
	vk.cmd->descriptor_set.current[ index ] = VK_NULL_HANDLE;
	vk.cmd->descriptor_set.image[ index ] = NULL;
}


void vk_update_descriptor( int index, VkDescriptorSet descriptor )
{
	if ( vk.cmd->descriptor_set.current[ index ] != descriptor ) {
		vk.cmd->descriptor_set.start = ( (uint32_t)index < vk.cmd->descriptor_set.start ) ? (uint32_t)index : vk.cmd->descriptor_set.start;
		vk.cmd->descriptor_set.end = ( (uint32_t)index > vk.cmd->descriptor_set.end ) ? (uint32_t)index : vk.cmd->descriptor_set.end;
	}
	vk.cmd->descriptor_set.current[ index ] = descriptor;
}

void vk_update_descriptor_offset( int index, uint32_t offset )
{
	vk.cmd->descriptor_set.offset[ index ] = offset;
}


void vk_bind_descriptor_sets( void )
{
	uint32_t offsets[VK_DESC_UNIFORM_COUNT], offset_count;
	uint32_t start, end, count, i;
	uint32_t bound_set_count = MIN( (uint32_t)VK_DESC_COUNT, vk.maxBoundDescriptorSets );

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U && !backEnd.oitAccumPass && !backEnd.oitMomentsPass ) {
		if ( vk.cmd->last_pipeline == VK_NULL_HANDLE ) {
			if ( vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] == VK_NULL_HANDLE &&
				vk.cmd->uniform_descriptor != VK_NULL_HANDLE )
			{
				vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] = vk.cmd->uniform_descriptor;
			}

			if ( bound_set_count == 0 || vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] == VK_NULL_HANDLE ) {
				return;
			}

			start = 0;
			end = bound_set_count - 1;
		} else {
			return;
		}
	}

	end = ( backEnd.oitAccumPass || backEnd.oitMomentsPass ) ? 0 : vk.cmd->descriptor_set.end;
	if ( backEnd.oitAccumPass || backEnd.oitMomentsPass )
		start = 0;

	offset_count = 0;
	if ( start == VK_DESC_UNIFORM ) {
		for ( i = 0; i < VK_DESC_UNIFORM_COUNT; i++ ) {
			offsets[offset_count++] = vk.cmd->descriptor_set.offset[i];
		}
	}

	count = end - start + 1;

#ifdef USE_VK_PBR
	if ( vk.maxBoundDescriptorSets >= VK_DESC_COUNT ) {
		VkDescriptorSet fp_set = vk_forward_plus_get_graphics_descriptor_set();
		if ( fp_set != VK_NULL_HANDLE ) {
			vk.cmd->descriptor_set.current[VK_DESC_FORWARD_PLUS] = fp_set;
		}
	}
#endif

	// fill NULL descriptor gaps
	if ( tr.whiteImage ) {
		for ( i = start + 1; i <= end; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
				vk.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
				vk.cmd->descriptor_set.image[i] = tr.whiteImage;
			}
		}
	}

#ifdef USE_VK_PBR
	if ( r_vk_pipeline_debug && r_vk_pipeline_debug->integer && vk.cmd ) {
		struct {
			int index;
			const char *name;
		} pbr_descs[] = {
			{ VK_DESC_PBR_NORMAL, "normal" },
			{ VK_DESC_PBR_PHYSICAL, "physical" },
			{ VK_DESC_PBR_CUBEMAP, "env" },
			{ VK_DESC_PBR_IRRADIANCE, "irradiance" }
		};

		ri.Printf( PRINT_DEVELOPER, "vk bind descriptors PBR\n" );
		for ( int desc_index = 0; desc_index < (int)ARRAY_LEN(pbr_descs); desc_index++ ) {
			int index = pbr_descs[desc_index].index;
			const char *name = pbr_descs[desc_index].name;
			const image_t *img = vk.cmd->descriptor_set.image[index];
			const char *source = img ? img->imgName : "none";
			const char *tag = "missing";
			if ( img == tr.whiteImage ) {
				tag = "fallback";
			} else if ( img != NULL ) {
				tag = "source";
			}
			ri.Printf( PRINT_DEVELOPER, "  %s desc=%p view=%p sampler=%p %s(%s)\n",
				name,
				(void*)vk.cmd->descriptor_set.current[index],
				(void*)(img ? img->view : VK_NULL_HANDLE),
				(void*)(img ? img->vk_sampler : VK_NULL_HANDLE),
				tag,
				source );
		}
	}
#endif

	if ( backEnd.oitMomentsPass && vk.pipeline_layout_oit_moments != VK_NULL_HANDLE ) {
		if ( vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0] != VK_NULL_HANDLE ) {
			VkDescriptorSet sets[2] = {
				vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0],
				vk.oit_depth_descriptor
			};
			uint32_t set_count = ( vk.oit_depth_descriptor != VK_NULL_HANDLE ) ? 2u : 1u;
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				vk.pipeline_layout_oit_moments, 0, set_count, sets, 0, NULL );
		}
	} else if ( backEnd.oitAccumPass && r_oit && r_oit->integer == 2 &&
		backEnd.oitBucketFilter != 2 &&
		vk.pipeline_layout_oit_accum_mboit != VK_NULL_HANDLE ) {
		/* MBOIT accum: set 0 = tex0; 1 = depth; 2 = moments; 3 = b0; 4 = Forward+ (when lit). */
		if ( vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0] != VK_NULL_HANDLE ) {
			VkDescriptorSet sets[5] = {
				vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0],
				vk.oit_depth_descriptor,
				vk.oit_moments_descriptor,
				vk.oit_b0_descriptor,
				VK_NULL_HANDLE
			};
			uint32_t set_count = 1u;
			if ( vk.oit_depth_descriptor != VK_NULL_HANDLE ) {
				set_count = 2u;
			}
			if ( vk.oit_moments_descriptor != VK_NULL_HANDLE && vk.oit_b0_descriptor != VK_NULL_HANDLE ) {
				set_count = 4u;
			}
#ifdef USE_VK_PBR
			if ( set_count >= 4u && vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
				VkDescriptorSet fp_set = vk_forward_plus_get_graphics_descriptor_set();
				if ( fp_set != VK_NULL_HANDLE ) {
					sets[4] = fp_set;
					set_count = 5u;
				}
			}
#endif
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				vk.pipeline_layout_oit_accum_mboit, 0, set_count, sets, 0, NULL );
		}
	} else if ( backEnd.oitAccumPass && vk.pipeline_layout_oit_accum != VK_NULL_HANDLE ) {
		/* OIT accum set 0 = tex0; set 1 = opaque depth; set 2 = Forward+ (when lit). */
		if ( vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0] != VK_NULL_HANDLE ) {
			VkDescriptorSet sets[3];
			uint32_t set_count = 1u;
			sets[0] = vk.cmd->descriptor_set.current[VK_DESC_TEXTURE0];
			sets[1] = vk.oit_depth_descriptor;
			if ( vk.oit_depth_descriptor != VK_NULL_HANDLE ) {
				set_count = 2u;
			}
#ifdef USE_VK_PBR
			if ( vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
				VkDescriptorSet fp_set = vk_forward_plus_get_graphics_descriptor_set();
				if ( fp_set != VK_NULL_HANDLE && set_count >= 2u ) {
					sets[2] = fp_set;
					set_count = 3u;
				}
			}
#endif
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				vk.pipeline_layout_oit_accum, 0, set_count, sets, 0, NULL );
		}
	} else {
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );
	}

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}

static void vk_force_descriptor_rebind( void )
{
	uint32_t bound_set_count = MIN( (uint32_t)VK_DESC_COUNT, vk.maxBoundDescriptorSets );

	if ( !vk.cmd || backEnd.oitAccumPass || backEnd.oitMomentsPass ) {
		return;
	}

	if ( vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] == VK_NULL_HANDLE &&
		vk.cmd->uniform_descriptor != VK_NULL_HANDLE ) {
		vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] = vk.cmd->uniform_descriptor;
	}

	if ( bound_set_count == 0 || vk.cmd->descriptor_set.current[VK_DESC_UNIFORM] == VK_NULL_HANDLE ) {
		return;
	}

	vk.cmd->descriptor_set.start = 0;
	vk.cmd->descriptor_set.end = bound_set_count - 1;
}


void vk_bind_pipeline( uint32_t pipeline ) {
	VkPipeline vkpipe;

	if ( backEnd.oitMomentsPass && vk.oit_moments_pipeline != VK_NULL_HANDLE ) {
		vkpipe = vk.oit_moments_pipeline;
	} else if ( backEnd.oitAccumPass && r_oit && r_oit->integer == 2 &&
		backEnd.oitBucketFilter != 2 &&
		vk.oit_accum_mboit_pipeline != VK_NULL_HANDLE ) {
		vkpipe = vk.oit_accum_mboit_pipeline;
	} else if ( backEnd.oitAccumPass && vk.oit_accum_pipeline != VK_NULL_HANDLE ) {
		vkpipe = vk.oit_accum_pipeline;
	} else {
		vkpipe = vk_gen_pipeline( pipeline );
	}

	if ( vkpipe != vk.cmd->last_pipeline ) {
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe );
		vk.cmd->last_pipeline = vkpipe;
		vk_force_descriptor_rebind();
	}

	if ( !backEnd.oitAccumPass && !backEnd.oitMomentsPass ) {
		vk_world.dirty_depth_attachment |= ( vk.pipelines[ pipeline ].def.state_bits & GLS_DEPTHMASK_TRUE );
	}
}

void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )
		return;

	/* MVP push may have run before IQM/glTF GPU skin data was committed to the geometry buffer;
	 * refresh so prev_mvp matches the SSBO binding for this indexed draw. */
	if ( indexed && vk.cmd->iqm_skin_offset != 0 ) {
		vk_update_mvp( NULL );
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboStreamItem )
		VBO_RenderStreamItem();
	else if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if ( indexed ) {
		if ( !R_Meshlets_TryDrawIndirect() ) {
			qvkCmdDrawIndexed( vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0 );
		}
	} else {
		qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
	}
}


void vk_draw_dot( uint32_t storage_offset )
{
	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )
		return;

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
}
