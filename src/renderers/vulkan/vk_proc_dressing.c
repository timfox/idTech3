/*
=============================================================================
Procedural Dressing System

Creates instancing directives from paint/spline/volume rules using simple
noise sampling. Outputs precomputed transforms each frame to the GPU culling
pipeline (if enabled).
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_proc_dressing.h"

#ifdef USE_VULKAN

// CVars (externs defined in tr_init.c)
extern cvar_t *r_procDressing;
extern cvar_t *r_procDressingDensity;
extern cvar_t *r_procDressingDebug;
extern cvar_t *r_foliageWindStrength;
extern cvar_t *r_foliageWindFrequency;

static proc_instance_t *procInstances = NULL;

static uint32_t hash_u32( uint32_t x )
{
	x ^= x >> 17;
	x *= 0xed5ad4bbU;
	x ^= x >> 11;
	x *= 0xac4c1b51U;
	x ^= x >> 15;
	x *= 0x31848babU;
	x ^= x >> 14;
	return x;
}

static float hash_float01( uint32_t seed )
{
	return ( hash_u32( seed ) & 0xFFFFFF ) / (float)0x1000000;
}

static float lerpf( float a, float b, float t )
{
	return a + ( b - a ) * t;
}

static void build_transform( const vec3_t pos, float uniformScale, mat4_t out )
{
	Matrix16Identity( out );
	out[0] = uniformScale;
	out[5] = uniformScale;
	out[10] = uniformScale;
	out[12] = pos[0];
	out[13] = pos[1];
	out[14] = pos[2];
}

static void seed_biomes( void )
{
	vk.procDressing.biomeCount = 0;

	proc_biome_t forest = { 0 };
	Q_strncpyz( forest.name, "forest", sizeof( forest.name ) );
	VectorSet( forest.tint, 0.35f, 0.5f, 0.35f );
	forest.scaleRange[0] = 0.8f;
	forest.scaleRange[1] = 1.6f;
	forest.densityMultiplier = 1.0f;
	forest.materialIndex = 0;
	vk.procDressing.biomes[vk.procDressing.biomeCount++] = forest;

	proc_biome_t rocks = { 0 };
	Q_strncpyz( rocks.name, "rocks", sizeof( rocks.name ) );
	VectorSet( rocks.tint, 0.4f, 0.4f, 0.4f );
	rocks.scaleRange[0] = 0.6f;
	rocks.scaleRange[1] = 1.2f;
	rocks.densityMultiplier = 0.5f;
	rocks.materialIndex = 1;
	vk.procDressing.biomes[vk.procDressing.biomeCount++] = rocks;
}

static void seed_rules( void )
{
	vk.procDressing.ruleCount = 0;

	// Paint rule around origin (hero space)
	proc_rule_t paint = { 0 };
	paint.type = PROC_RULE_PAINT;
	VectorClear( paint.a );
	paint.radius = 25.0f;
	paint.density = 0.08f;
	paint.jitter = 0.5f;
	paint.biomeId = 0;
	paint.maxInstances = 2048;
	vk.procDressing.rules[vk.procDressing.ruleCount++] = paint;

	// Volume rule for backdrop dressing
	proc_rule_t volume = { 0 };
	volume.type = PROC_RULE_VOLUME;
	VectorSet( volume.mins, -40.0f, -40.0f, -2.0f );
	VectorSet( volume.maxs, 40.0f, 40.0f, 8.0f );
	volume.density = 0.015f;
	volume.jitter = 0.25f;
	volume.biomeId = 1;
	volume.maxInstances = 4096;
	vk.procDressing.rules[vk.procDressing.ruleCount++] = volume;
}

static proc_biome_t *get_biome( uint32_t biomeId )
{
	if ( biomeId >= vk.procDressing.biomeCount ) {
		return &vk.procDressing.biomes[0];
	}
	return &vk.procDressing.biomes[biomeId];
}

static void add_instance( const vec3_t pos, float scale, uint32_t biomeId, const vec3_t tint )
{
	if ( vk.procDressing.instanceCount >= VK_MAX_PROC_INSTANCES ) {
		return;
	}
	proc_instance_t *inst = &procInstances[vk.procDressing.instanceCount++];
	build_transform( pos, scale, inst->transform );
	inst->biomeId = biomeId;
	inst->color[0] = tint[0];
	inst->color[1] = tint[1];
	inst->color[2] = tint[2];
	inst->color[3] = 1.0f;
}

static void generate_from_paint( const proc_rule_t *rule, uint32_t seed )
{
	const float area = M_PI * rule->radius * rule->radius;
	const float density = rule->density * ( r_procDressingDensity ? r_procDressingDensity->value : 1.0f );
	const float targetF = Com_Clamp( 0.0f, (float)rule->maxInstances, area * density );
	const uint32_t target = (uint32_t)targetF;
	const proc_biome_t *biome = get_biome( rule->biomeId );

	for ( uint32_t i = 0; i < target && vk.procDressing.instanceCount < VK_MAX_PROC_INSTANCES; ++i ) {
		const float r = rule->radius * sqrtf( hash_float01( seed + i ) );
		const float theta = hash_float01( seed + i * 3 + 7 ) * (float)( 2.0 * M_PI );
		vec3_t pos;
		pos[0] = rule->a[0] + r * cosf( theta );
		pos[1] = rule->a[1] + r * sinf( theta );
		pos[2] = rule->a[2];

		const float jitter = ( hash_float01( seed + i * 5 + 13 ) - 0.5f ) * rule->jitter;
		pos[2] += jitter;

		const float scaleRand = hash_float01( seed + i * 11 + 21 );
		const float scale = lerpf( biome->scaleRange[0], biome->scaleRange[1], scaleRand );
		add_instance( pos, scale, rule->biomeId, biome->tint );
	}
}

static void generate_from_volume( const proc_rule_t *rule, uint32_t seed )
{
	vec3_t extents;
	VectorSubtract( rule->maxs, rule->mins, extents );
	const float area = extents[0] * extents[1];
	const float density = rule->density * ( r_procDressingDensity ? r_procDressingDensity->value : 1.0f );
	const float targetF = Com_Clamp( 0.0f, (float)rule->maxInstances, area * density );
	const uint32_t target = (uint32_t)targetF;
	const proc_biome_t *biome = get_biome( rule->biomeId );

	for ( uint32_t i = 0; i < target && vk.procDressing.instanceCount < VK_MAX_PROC_INSTANCES; ++i ) {
		vec3_t pos;
		pos[0] = rule->mins[0] + hash_float01( seed + i * 3 + 1 ) * extents[0];
		pos[1] = rule->mins[1] + hash_float01( seed + i * 5 + 3 ) * extents[1];
		pos[2] = rule->mins[2] + hash_float01( seed + i * 7 + 5 ) * extents[2];

		const float scaleRand = hash_float01( seed + i * 9 + 11 );
		const float scale = lerpf( biome->scaleRange[0], biome->scaleRange[1], scaleRand );
		add_instance( pos, scale, rule->biomeId, biome->tint );
	}
}

static void generate_from_spline( const proc_rule_t *rule, uint32_t seed )
{
	vec3_t delta;
	VectorSubtract( rule->b, rule->a, delta );
	const float length = VectorLength( delta );
	const float density = rule->density * ( r_procDressingDensity ? r_procDressingDensity->value : 1.0f );
	const float targetF = Com_Clamp( 0.0f, (float)rule->maxInstances, length * rule->radius * 2.0f * density );
	const uint32_t target = (uint32_t)targetF;
	const proc_biome_t *biome = get_biome( rule->biomeId );

	for ( uint32_t i = 0; i < target && vk.procDressing.instanceCount < VK_MAX_PROC_INSTANCES; ++i ) {
		const float t = hash_float01( seed + i * 13 + 3 );
		vec3_t pos;
		VectorMA( rule->a, t, delta, pos );

		// Scatter within radius
		const float angle = hash_float01( seed + i * 17 + 9 ) * (float)( 2.0 * M_PI );
		const float offset = rule->radius * ( hash_float01( seed + i * 19 + 11 ) - 0.5f );
		pos[0] += cosf( angle ) * offset;
		pos[1] += sinf( angle ) * offset;

		const float scaleRand = hash_float01( seed + i * 23 + 15 );
		const float scale = lerpf( biome->scaleRange[0], biome->scaleRange[1], scaleRand );
		add_instance( pos, scale, rule->biomeId, biome->tint );
	}
}

static void apply_wind_offset( const proc_instance_t *inst, mat4_t out )
{
	Matrix16Copy( inst->transform, out );

	if ( !r_foliageWindStrength || r_foliageWindStrength->value <= 0.0f ) {
		return;
	}

	// Treat biome 0 as foliage for wind; others remain static
	if ( inst->biomeId != 0 ) {
		return;
	}

	const float strength = r_foliageWindStrength->value;
	const float freq = r_foliageWindFrequency ? r_foliageWindFrequency->value : 0.6f;
	const float t = tr.refdef.floatTime * freq + inst->biomeId * 0.37f;
	const float sway = sinf( t ) * strength;

	out[12] += sway * 0.5f; // x
	out[14] += sway * 0.35f; // z
}

static void build_instances( void )
{
	vk.procDressing.instanceCount = 0;

	for ( uint32_t i = 0; i < vk.procDressing.ruleCount; ++i ) {
		const proc_rule_t *rule = &vk.procDressing.rules[i];
		uint32_t seed = (uint32_t)Com_BlockChecksum( rule, sizeof( proc_rule_t ) ) + (uint32_t)vk.frame_count;
		switch ( rule->type ) {
			case PROC_RULE_PAINT:  generate_from_paint( rule, seed ); break;
			case PROC_RULE_VOLUME: generate_from_volume( rule, seed ); break;
			case PROC_RULE_SPLINE: generate_from_spline( rule, seed ); break;
			default: break;
		}
	}

	vk.procDressing.dirty = qfalse;

	if ( r_procDressingDebug && r_procDressingDebug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "ProcDressing: generated %u instances from %u rules\n",
			vk.procDressing.instanceCount, vk.procDressing.ruleCount );
	}
}

void vk_proc_dressing_init( void )
{
	if ( vk.procDressing.initialized ) {
		return;
	}

	vk.procDressing.enabled = ( r_procDressing && r_procDressing->integer );
	if ( !vk.procDressing.enabled ) {
		return;
	}

	Com_Memset( &vk.procDressing, 0, sizeof( vk.procDressing ) );
	vk.procDressing.enabled = qtrue;

	procInstances = (proc_instance_t *)ri.Malloc( sizeof( proc_instance_t ) * VK_MAX_PROC_INSTANCES );
	seed_biomes();
	seed_rules();
	vk.procDressing.dirty = qtrue;
	vk.procDressing.initialized = qtrue;
	ri.Printf( PRINT_ALL, "Procedural dressing: initialized (%u max instances)\n", VK_MAX_PROC_INSTANCES );
}

void vk_proc_dressing_shutdown( void )
{
	if ( procInstances ) {
		ri.Free( procInstances );
		procInstances = NULL;
	}
	Com_Memset( &vk.procDressing, 0, sizeof( vk.procDressing ) );
}

void vk_proc_dressing_mark_dirty( void )
{
	vk.procDressing.dirty = qtrue;
}

void vk_proc_dressing_tick( void )
{
	if ( r_procDressing && !r_procDressing->integer ) {
		return;
	}

	if ( !vk.procDressing.initialized ) {
		vk_proc_dressing_init();
	}

	if ( !vk.procDressing.initialized ) {
		return;
	}

	if ( vk.procDressing.dirty ) {
		build_instances();
	}

	// Push instances into GPU culling each frame (instance buffer resets per frame)
	if ( vk_gpu_culling_is_enabled() ) {
		for ( uint32_t i = 0; i < vk.procDressing.instanceCount; ++i ) {
			const proc_instance_t *inst = &procInstances[i];
			mat4_t windAdjusted;
			apply_wind_offset( inst, windAdjusted );
			vk_gpu_culling_add_instance( windAdjusted, inst->biomeId, inst->color );
		}
	}
}

#endif // USE_VULKAN


