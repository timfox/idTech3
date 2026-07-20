/*
===========================================================================
Raster Ultra 1.8 — material instances (parameter overrides).
===========================================================================
*/

#include "tr_local.h"
#include "vk_material_ir.h"
#include "vk_material_instance.h"

static cvar_t *r_materialInstance;
static qboolean s_cmds;
static vkMaterialInstance_t s_pool[VK_MAT_INSTANCE_MAX];
static int s_count;

void vk_material_instance_register_cvars( void )
{
	if ( r_materialInstance ) {
		return;
	}
	r_materialInstance = ri.Cvar_Get( "r_materialInstance", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_materialInstance, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialInstance,
		"Raster Ultra 1.8 material instances (latched).\n"
		"Parameter overrides without new SPIR-V / pipeline clones." );
	ri.Cvar_SetGroup( r_materialInstance, CVG_RENDERER );
}

void vk_material_instance_init( void )
{
	vk_material_instance_register_cvars();
	Com_Memset( s_pool, 0, sizeof( s_pool ) );
	s_count = 0;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "material_instance_status", vk_material_instance_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][MaterialInstance] %s pool=%d\n",
		( r_materialInstance && r_materialInstance->integer ) ? "enabled" : "off",
		VK_MAT_INSTANCE_MAX );
}

void vk_material_instance_shutdown( void )
{
	Com_Memset( s_pool, 0, sizeof( s_pool ) );
	s_count = 0;
}

int vk_material_instance_create( const char *name, uint32_t baseShaderIndex )
{
	int i;

	if ( !r_materialInstance || !r_materialInstance->integer ) {
		return -1;
	}
	for ( i = 0; i < VK_MAT_INSTANCE_MAX; i++ ) {
		if ( !s_pool[i].active ) {
			vkMaterialInstance_t *inst = &s_pool[i];
			Com_Memset( inst, 0, sizeof( *inst ) );
			inst->active = qtrue;
			Q_strncpyz( inst->name, name ? name : va( "inst_%d", i ), sizeof( inst->name ) );
			inst->baseShaderIndex = baseShaderIndex;
			inst->color[0] = inst->color[1] = inst->color[2] = inst->color[3] = 1.0f;
			inst->roughness = -1.0f; /* <0 = inherit */
			inst->metallic = -1.0f;
			inst->opacity = 1.0f;
			inst->uvScale = 1.0f;
			s_count++;
			return i;
		}
	}
	return -1;
}

qboolean vk_material_instance_set_param( int handle, const char *param, float value )
{
	vkMaterialInstance_t *inst;

	if ( handle < 0 || handle >= VK_MAT_INSTANCE_MAX || !param ) {
		return qfalse;
	}
	inst = &s_pool[handle];
	if ( !inst->active ) {
		return qfalse;
	}
	if ( !Q_stricmp( param, "roughness" ) ) {
		inst->roughness = value;
	} else if ( !Q_stricmp( param, "metallic" ) ) {
		inst->metallic = value;
	} else if ( !Q_stricmp( param, "wetness" ) ) {
		inst->wetness = value;
	} else if ( !Q_stricmp( param, "dirt" ) ) {
		inst->dirt = value;
	} else if ( !Q_stricmp( param, "damage" ) ) {
		inst->damage = value;
	} else if ( !Q_stricmp( param, "snow" ) ) {
		inst->snow = value;
	} else if ( !Q_stricmp( param, "dust" ) ) {
		inst->dust = value;
	} else if ( !Q_stricmp( param, "rust" ) ) {
		inst->rust = value;
	} else if ( !Q_stricmp( param, "soot" ) ) {
		inst->soot = value;
	} else if ( !Q_stricmp( param, "moss" ) ) {
		inst->moss = value;
	} else if ( !Q_stricmp( param, "uvScale" ) ) {
		inst->uvScale = value;
	} else if ( !Q_stricmp( param, "opacity" ) ) {
		inst->opacity = value;
	} else if ( !Q_stricmp( param, "transmission" ) ) {
		inst->transmission = value;
	} else if ( !Q_stricmp( param, "animationRate" ) ) {
		inst->animationRate = value;
	} else {
		return qfalse;
	}
	return qtrue;
}

const vkMaterialInstance_t *vk_material_instance_get( int handle )
{
	if ( handle < 0 || handle >= VK_MAT_INSTANCE_MAX ) {
		return NULL;
	}
	if ( !s_pool[handle].active ) {
		return NULL;
	}
	return &s_pool[handle];
}

void vk_material_instance_apply( const vkMaterialInstance_t *inst, vkMaterialIR_t *ir )
{
	if ( !inst || !ir || !inst->active ) {
		return;
	}
	if ( inst->roughness >= 0.0f ) {
		ir->roughness = Com_Clamp( 0.02f, 1.0f, inst->roughness );
	}
	if ( inst->metallic >= 0.0f ) {
		ir->metallic = Com_Clamp( 0.0f, 1.0f, inst->metallic );
	}
	ir->wetness = Com_Clamp( 0.0f, 1.0f, inst->wetness );
	ir->snow = Com_Clamp( 0.0f, 1.0f, inst->snow );
	ir->dust = Com_Clamp( 0.0f, 1.0f, ( inst->dust > inst->dirt ) ? inst->dust : inst->dirt );
	ir->rust = Com_Clamp( 0.0f, 1.0f, inst->rust );
	ir->soot = Com_Clamp( 0.0f, 1.0f, inst->soot );
	ir->moss = Com_Clamp( 0.0f, 1.0f, inst->moss );
	ir->damage = Com_Clamp( 0.0f, 1.0f, inst->damage );
	ir->opacity = Com_Clamp( 0.0f, 1.0f, inst->opacity );
	ir->uvScale = inst->uvScale > 0.0f ? inst->uvScale : 1.0f;
	if ( ir->wetness > 0.0f ) {
		ir->dynamicFeatures |= VK_MAT_DYN_WETNESS;
	}
	if ( ir->snow > 0.0f ) {
		ir->dynamicFeatures |= VK_MAT_DYN_SNOW;
	}
	if ( ir->dust > 0.0f ) {
		ir->dynamicFeatures |= VK_MAT_DYN_DUST;
	}
	if ( ir->rust > 0.0f ) {
		ir->dynamicFeatures |= VK_MAT_DYN_RUST;
	}
	if ( ir->damage > 0.0f ) {
		ir->dynamicFeatures |= VK_MAT_DYN_DAMAGE;
	}
	ir->staticFeatures |= VK_MAT_FEAT_EVOLUTION;
	ir->permutationKey = vk_material_ir_permutation_key( ir );
}

void vk_material_instance_status_f( void )
{
	int i, active = 0;

	for ( i = 0; i < VK_MAT_INSTANCE_MAX; i++ ) {
		if ( s_pool[i].active ) {
			active++;
		}
	}
	ri.Printf( PRINT_ALL, "=== Material Instance (Raster Ultra 1.8) ===\n" );
	ri.Printf( PRINT_ALL, "r_materialInstance : %d\n",
		r_materialInstance ? r_materialInstance->integer : 0 );
	ri.Printf( PRINT_ALL, "active             : %d / %d\n", active, VK_MAT_INSTANCE_MAX );
	ri.Printf( PRINT_ALL, "policy             : param overrides; no pipeline duplication\n" );
}
