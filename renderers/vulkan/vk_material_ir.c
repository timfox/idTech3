/*
===========================================================================
Raster Ultra 1.8 — material IR translation from classic / PBR shaders.
===========================================================================
*/

#include "tr_local.h"
#include "vk_material_ir.h"
#include "vk_raster_ultra.h"

static cvar_t *r_materialIR;
static qboolean s_cmds;
static uint32_t s_translateCount;
static uint32_t s_classicCount;
static uint32_t s_pbrCount;

void vk_material_ir_register_cvars( void )
{
	if ( r_materialIR ) {
		return;
	}
	r_materialIR = ri.Cvar_Get( "r_materialIR", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_materialIR, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialIR,
		"Raster Ultra 1.8 material IR translation (latched).\n"
		" 0 off (classic shaderStage path only)\n"
		" 1 enable IR bookkeeping + status (does not replace Q3 shaders)" );
	ri.Cvar_SetGroup( r_materialIR, CVG_RENDERER );
}

void vk_material_ir_init( void )
{
	vk_material_ir_register_cvars();
	s_translateCount = 0;
	s_classicCount = 0;
	s_pbrCount = 0;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "material_ir_status", vk_material_ir_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][MaterialIR] %s (r_materialIR %d) cacheVersion=%d\n",
		( r_materialIR && r_materialIR->integer ) ? "enabled" : "off",
		r_materialIR ? r_materialIR->integer : 0,
		VK_MAT_IR_CACHE_VERSION );
}

void vk_material_ir_shutdown( void )
{
}

void vk_material_ir_reset( vkMaterialIR_t *ir )
{
	if ( !ir ) {
		return;
	}
	Com_Memset( ir, 0, sizeof( *ir ) );
	ir->domain = VK_MAT_DOMAIN_OPAQUE;
	ir->blendMode = VK_MAT_BLEND_OPAQUE;
	ir->shadeModel = VK_MAT_SHADE_CLASSIC;
	ir->cacheVersion = VK_MAT_IR_CACHE_VERSION;
	ir->sourceShaderIndex = ~0u;
	ir->baseColor[0] = ir->baseColor[1] = ir->baseColor[2] = 1.0f;
	ir->baseColor[3] = 1.0f;
	ir->roughness = 0.72f;
	ir->opacity = 1.0f;
	ir->uvScale = 1.0f;
	ir->heightBlendSharpness = 8.0f;
	ir->expectedTexelDensity = 1.0f;
	ir->patternPeriod = 0.0f;
	ir->proceduralMaxFreq = 0.0f;
	ir->preferredAnisotropy = 8.0f;
	ir->detailFrequencyBand = 1; /* meso */
	ir->alphaCoveragePolicy = 0;
	ir->antiMoireImportance = 0;
	ir->stochasticEligible = 0;
	ir->fromClassic = qtrue;
}

uint32_t vk_material_ir_permutation_key( const vkMaterialIR_t *ir )
{
	uint32_t key;

	if ( !ir || !ir->valid ) {
		return 0;
	}
	/* Bounded static feature groups only — no unrestricted graph hash. */
	key = ir->staticFeatures & 0xffffu;
	key ^= ( (uint32_t)ir->domain << 16 );
	key ^= ( (uint32_t)ir->shadeModel << 24 );
	if ( ir->layerCount > 1 ) {
		key ^= ( (uint32_t)Com_Clamp( 2, VK_MAT_MAX_LAYERS, ir->layerCount ) << 8 );
	}
	return key;
}

qboolean vk_material_ir_from_shader( const shader_t *shader, vkMaterialIR_t *out )
{
	const shaderStage_t *stage;
	uint32_t feat = 0;
	int i;

	if ( !out ) {
		return qfalse;
	}
	vk_material_ir_reset( out );
	if ( !shader ) {
		return qfalse;
	}

	Q_strncpyz( out->name, shader->name, sizeof( out->name ) );
	out->sourceShaderIndex = (uint32_t)shader->index;
	out->fromClassic = qtrue;

	if ( shader->isSky ) {
		out->domain = VK_MAT_DOMAIN_SKY;
	} else if ( shader->sort >= SS_BLEND0 ) {
		out->domain = VK_MAT_DOMAIN_TRANSPARENT;
		out->blendMode = VK_MAT_BLEND_ALPHA;
	} else if ( shader->contentFlags & CONTENTS_WATER ) {
		out->domain = VK_MAT_DOMAIN_WATER;
		feat |= VK_MAT_FEAT_WATER | VK_MAT_FEAT_FREQUENCY;
		out->detailFrequencyBand = 2; /* micro waves */
		out->antiMoireImportance = 2;
	}

	stage = NULL;
	for ( i = 0; i < MAX_SHADER_STAGES && shader->stages[i]; i++ ) {
		if ( shader->stages[i]->active ) {
			stage = shader->stages[i];
			break;
		}
	}
	if ( !stage ) {
		out->valid = qtrue;
		s_translateCount++;
		s_classicCount++;
		out->permutationKey = vk_material_ir_permutation_key( out );
		return qtrue;
	}

	if ( stage->stateBits & GLS_ATEST_BITS ) {
		out->domain = VK_MAT_DOMAIN_ALPHA_TEST;
		feat |= VK_MAT_FEAT_ALPHA_TEST | VK_MAT_FEAT_FREQUENCY;
		out->alphaCoveragePolicy = 1;
		out->antiMoireImportance = 2;
	}

	if ( stage->vk_pbr_flags ) {
		out->shadeModel = VK_MAT_SHADE_PBR_METAL_ROUGH;
		out->fromClassic = qfalse;
		s_pbrCount++;
		if ( stage->vk_pbr_flags & PBR_HAS_CLEARCOAT ) {
			feat |= VK_MAT_FEAT_CLEARCOAT;
		}
		if ( stage->vk_pbr_flags & PBR_HAS_SHEEN ) {
			feat |= VK_MAT_FEAT_SHEEN;
		}
		if ( stage->vk_pbr_flags & PBR_HAS_ANISOTROPY ) {
			feat |= VK_MAT_FEAT_ANISOTROPY;
		}
		if ( stage->vk_pbr_flags & PBR_HAS_TRANSMISSION ) {
			feat |= VK_MAT_FEAT_TRANSMISSION;
			if ( out->domain == VK_MAT_DOMAIN_OPAQUE ) {
				out->domain = VK_MAT_DOMAIN_GLASS;
			}
		}
		if ( ( stage->vk_pbr_flags & PBR_HAS_MATERIAL_BLEND ) || stage->materialBlend ) {
			feat |= VK_MAT_FEAT_HEIGHT_BLEND;
			out->heightBlendSharpness = stage->blendSharpness > 0.0f ? stage->blendSharpness : 8.0f;
			out->layerCount = stage->materialLayerCount > 0 ? stage->materialLayerCount : 2;
			if ( out->layerCount < 2 ) {
				out->layerCount = 2;
			}
			if ( out->layerCount > VK_MAT_MAX_LAYERS ) {
				out->layerCount = VK_MAT_MAX_LAYERS;
			}
			for ( i = 0; i < out->layerCount; i++ ) {
				out->layers[i].weight = 1.0f;
				out->layers[i].opacity = 1.0f;
				out->layers[i].roughness = 0.72f;
			}
		}
		if ( stage->normalMapType == PHYS_NORMALHEIGHT ||
			( stage->normalScale[3] > 0.0f && r_pom && r_pom->integer ) ) {
			feat |= VK_MAT_FEAT_POM;
		}
	} else {
		s_classicCount++;
	}

	out->staticFeatures = feat;
	out->valid = qtrue;
	out->permutationKey = vk_material_ir_permutation_key( out );
	s_translateCount++;
	return qtrue;
}

void vk_material_ir_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== Material IR (Raster Ultra 1.8) ===\n" );
	ri.Printf( PRINT_ALL, "r_materialIR     : %d\n", r_materialIR ? r_materialIR->integer : 0 );
	ri.Printf( PRINT_ALL, "cacheVersion     : %d\n", VK_MAT_IR_CACHE_VERSION );
	ri.Printf( PRINT_ALL, "translates       : %u (classic=%u pbr=%u)\n",
		s_translateCount, s_classicCount, s_pbrCount );
	ri.Printf( PRINT_ALL, "domains          : opaque..terrain (%d)\n", VK_MAT_DOMAIN_COUNT );
	ri.Printf( PRINT_ALL, "note             : IR books PBR/classic; does not replace Q3 stages\n" );
}
