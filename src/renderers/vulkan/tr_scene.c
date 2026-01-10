/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;

static int			r_firstSceneDrawSurf;
#ifdef USE_PMLIGHT
static int			r_firstSceneLitSurf;
#endif

int			r_numdlights;
static int			r_firstSceneDlight;

static int			r_numentities;
static int			r_firstSceneEntity;

static int			r_numpolys;
static int			r_firstScenePoly;

static int			r_numpolyverts;

// CPU-side particle buffer for RE_AddParticle
#define MAX_CPU_PARTICLES 1024
typedef struct {
	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		color;
	float		size;
	float		life;
	float		maxLife; // Store original life for fade calculations
	qhandle_t	shader;
	qboolean	active;
} cpu_particle_t;

static cpu_particle_t	cpu_particles[MAX_CPU_PARTICLES];
static int				cpu_particle_count = 0;
static float			last_particle_update_time = 0.0f;

// Forward declarations for static particle functions
static void R_ProcessCPUParticles( float deltaTime );
static void R_RenderCPUParticles( void );


/*
====================
R_InitNextFrame

====================
*/
void R_InitNextFrame( void ) {
	// Process CPU particles before resetting (update positions, life, etc.)
	if (tr.refdef.floatTime > 0.0f) {
		float deltaTime = tr.refdef.floatTime - last_particle_update_time;
		if (deltaTime > 0.0f && deltaTime < 1.0f) { // Sanity check: delta should be reasonable
			R_ProcessCPUParticles(deltaTime);
		}
		last_particle_update_time = tr.refdef.floatTime;
	}
	
	// Note: We don't reset cpu_particle_count here - particles persist across frames
	// until they die. Only reset if explicitly clearing scene.
	// cpu_particle_count = 0; // Moved to RE_ClearScene if needed

	backEndData->commands.used = 0;

	r_firstSceneDrawSurf = 0;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = 0;
#endif

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;
}


/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstScenePoly = r_numpolys;
	
	// Optionally clear CPU particles when scene is cleared
	// For now, particles persist until they die naturally
	// cpu_particle_count = 0;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces( void ) {
	int			i;
	shader_t	*sh;
	const srfPoly_t	*poly;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		sh = R_GetShaderByHandle( poly->hShader );
		R_AddDrawSurf( ( void * )poly, sh, poly->fogIndex, 0 );
	}
}

/*
=====================
RE_AddPolyToScene

=====================
*/
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	srfPoly_t	*poly;
	int			i, j;
	int			fogIndex;
	const fog_t		*fog;
	vec3_t		bounds[2];

	if ( !tr.registered ) {
		return;
	}
#if 0
	if ( !hShader ) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_AddPolyToScene: NULL poly shader\n");
		return;
	}
#endif
	for ( j = 0; j < numPolys; j++ ) {
		if ( r_numpolyverts + numVerts > max_polyverts || r_numpolys >= max_polys ) {
      /*
      NOTE TTimo this was initially a PRINT_WARNING
      but it happens a lot with high fighting scenes and particles
      since we don't plan on changing the const and making for room for those effects
      simply cut this message to developer only
      */
			ri.Printf( PRINT_DEVELOPER, "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n");
			return;
		}

		poly = &backEndData->polys[r_numpolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData->polyVerts[r_numpolyverts];
		
		Com_Memcpy( poly->verts, &verts[numVerts*j], numVerts * sizeof( *verts ) );
#if 0
		if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
			poly->verts->modulate[0] = 255;
			poly->verts->modulate[1] = 255;
			poly->verts->modulate[2] = 255;
			poly->verts->modulate[3] = 255;
		}
#endif
		// done.
		r_numpolys++;
		r_numpolyverts += numVerts;

		// if no world is loaded
		if ( tr.world == NULL ) {
			fogIndex = 0;
		}
		// see if it is in a fog volume
		else if ( tr.world->numfogs == 1 ) {
			fogIndex = 0;
		} else {
			// find which fog volume the poly is in
			VectorCopy( poly->verts[0].xyz, bounds[0] );
			VectorCopy( poly->verts[0].xyz, bounds[1] );
			for ( i = 1 ; i < poly->numVerts ; i++ ) {
				AddPointToBounds( poly->verts[i].xyz, bounds[0], bounds[1] );
			}
			for ( fogIndex = 1 ; fogIndex < tr.world->numfogs ; fogIndex++ ) {
				fog = &tr.world->fogs[fogIndex]; 
				if ( bounds[1][0] >= fog->bounds[0][0]
					&& bounds[1][1] >= fog->bounds[0][1]
					&& bounds[1][2] >= fog->bounds[0][2]
					&& bounds[0][0] <= fog->bounds[1][0]
					&& bounds[0][1] <= fog->bounds[1][1]
					&& bounds[0][2] <= fog->bounds[1][2] ) {
					break;
				}
			}
			if ( fogIndex == tr.world->numfogs ) {
				fogIndex = 0;
			}
		}
		poly->fogIndex = fogIndex;
	}
}


//=================================================================================

static int isnan_fp( const float *f )
{
	uint32_t u = *( (uint32_t*) f );
	u = 0x7F800000 - ( u & 0x7FFFFFFF );
	return (int)( u >> 31 );
}


/*
=====================
RE_AddRefEntityToScene
=====================
*/
void RE_AddRefEntityToScene( const refEntity_t *ent, qboolean intShaderTime ) {
	if ( !tr.registered ) {
		return;
	}
	if ( r_numentities >= MAX_REFENTITIES ) {
		ri.Printf( PRINT_DEVELOPER, "RE_AddRefEntityToScene: Dropping refEntity, reached MAX_REFENTITIES\n" );
		return;
	}
	if ( isnan_fp( &ent->origin[0] ) || isnan_fp( &ent->origin[1] ) || isnan_fp( &ent->origin[2] ) ) {
		static qboolean first_time = qtrue;
		if ( first_time ) {
			first_time = qfalse;
			ri.Printf( PRINT_WARNING, "RE_AddRefEntityToScene passed a refEntity which has an origin with a NaN component\n" );
		}
		return;
	}
	if ( (unsigned)ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		ri.Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData->entities[r_numentities].e = *ent;
	backEndData->entities[r_numentities].lightingCalculated = qfalse;
	backEndData->entities[r_numentities].intShaderTime = intShaderTime;

	r_numentities++;
}


/*
=====================
RE_AddDynamicLightToScene
=====================
*/
static void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int additive ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	if ( (size_t)r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
#ifndef USE_VULKAN
	// these cards don't have the correct blend mode
	if ( glConfig.hardwareType == GLHW_RIVA128 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		return;
	}
#endif
#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer )
#endif
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if ( r_dlightSaturation->value != 1.0 )
	{
		float luminance = LUMA( r, g, b );
		r = LERP( luminance, r, r_dlightSaturation->value );
		g = LERP( luminance, g, r_dlightSaturation->value );
		b = LERP( luminance, b, r_dlightSaturation->value );
	}

	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy( org, dl->origin );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = additive;
	dl->linear = qfalse;
}


/*
=====================
RE_AddLinearLightToScene
=====================
*/
void RE_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b  ) {
	dlight_t	*dl;
	if ( VectorCompare( start, end ) ) {
		RE_AddDynamicLightToScene( start, intensity, r, g, b, 0 );
		return;
	}
	if ( !tr.registered ) {
		return;
	}
	if ( (size_t)r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer )
#endif
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if ( r_dlightSaturation->value != 1.0 )
	{
		float luminance = LUMA( r, g, b );
		r = LERP( luminance, r, r_dlightSaturation->value );
		g = LERP( luminance, g, r_dlightSaturation->value );
		b = LERP( luminance, b, r_dlightSaturation->value );
	}

	dl = &backEndData->dlights[ r_numdlights++ ];
	VectorCopy( start, dl->origin );
	VectorCopy( end, dl->origin2 );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = 0;
	dl->linear = qtrue;
}



/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qfalse );
}


/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}


void *R_GetCommandBuffer( int bytes );

void RE_BeginScene( const refdef_t *fd ) {
	Com_Memcpy( tr.refdef.text, fd->text, sizeof( tr.refdef.text ) );

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.rdflags = fd->rdflags;

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if ( ! (tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int		areaDiff;
		int		i;

		// compare the area bits
		areaDiff = 0;
		for ( i = 0; i < (int)(MAX_MAP_AREA_BYTES/sizeof(int)); i++ ) {
			areaDiff |= ((int *)tr.refdef.areamask)[i] ^ ((int *)fd->areamask)[i];
			((int *)tr.refdef.areamask)[i] = ((int *)fd->areamask)[i];
		}

		if ( areaDiff ) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}


	// derived info

	tr.refdef.floatTime = (double)tr.refdef.time * 0.001; // -EC-: cast to double

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

#ifdef USE_PMLIGHT
	tr.refdef.numLitSurfs = r_firstSceneLitSurf;
	tr.refdef.litSurfs = backEndData->litSurfs;
#endif

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	// turn off dynamic lighting globally by clearing all the
	// dlights if it needs to be disabled
	if ( r_dynamiclight->integer == 0 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		tr.refdef.num_dlights = 0;
	}

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;
}

void RE_EndScene( void ) {
	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = tr.refdef.numLitSurfs;
#endif

	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;
}

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderScene( const refdef_t *fd ) {
#ifdef USE_VULKAN
	renderCommand_t	lastRenderCommand __attribute__((unused));

	// Check Vulkan safety before rendering
	extern qboolean vk_is_safe_state(void);
	if (!vk_is_safe_state()) {
		ri.Printf(PRINT_DEVELOPER, "Vulkan: Skipping scene render - renderer not in safe state\n");
		return;
	}

	// Periodic memory corruption check for cache structures
	extern void vk_memory_corruption_check(void);
	vk_memory_corruption_check();
#endif
	viewParms_t		parms;
	int				startTime;

	if ( !tr.registered ) {
		return;
	}

	if ( r_norefresh->integer ) {
		return;
	}

	startTime = ri.Milliseconds();

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Error (ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

	RE_BeginScene( fd );

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	Com_Memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - ( tr.refdef.y + tr.refdef.height );
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;

	parms.scissorX = parms.viewportX;
	parms.scissorY = parms.viewportY;
	parms.scissorWidth = parms.viewportWidth;
	parms.scissorHeight = parms.viewportHeight;

	parms.portalView = PV_NONE;

#ifdef USE_PMLIGHT
	parms.dlights = tr.refdef.dlights;
	parms.num_dlights = tr.refdef.num_dlights;
#endif

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;
	
	parms.stereoFrame = tr.refdef.stereoFrame;

	VectorCopy( fd->vieworg, parms.or.origin );
	VectorCopy( fd->viewaxis[0], parms.or.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.or.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.or.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

#ifdef USE_VULKAN
	lastRenderCommand = tr.lastRenderCommand;
	tr.drawSurfCmd = NULL;
	tr.numDrawSurfCmds = 0;
#endif

	R_RenderView( &parms );

#ifdef USE_VULKAN
	if ( tr.needScreenMap )
	{
		// Duplicate draw surfaces so we can capture after the first pass and
		// immediately redraw the subset that samples $currentRender.
		drawSurfsCommand_t *cmd, *src = NULL;
		int i;

		for ( i = 0; i < tr.numDrawSurfCmds; i++ )
		{
			cmd = R_GetCommandBuffer( sizeof( *cmd ) );
			if ( cmd )
			{
				src = tr.drawSurfCmd + i;
				*cmd = *src;
			}
			else
			{
				break;
			}
		}

		if ( src )
		{
			tr.drawSurfCmd[0].refdef.needScreenMap = qtrue;
			src->refdef.switchRenderPass = qtrue;
		}

		tr.needScreenMap = 0;
	}
#endif

	// Ray tracing moved to RTX renderer only

	// Vulkan: Record rendering commands
	if (vk.device != (VkDevice)0x20000000 && vk.active) {
		vk_render_scene_vulkan(fd);
		
		// Render CPU particles after main scene
		R_RenderCPUParticles();
	}

	RE_EndScene();

	tr.frontEndMsec += ri.Milliseconds() - startTime;
}

/*
=====================
RE_AddParticle
=====================
*/
void RE_AddParticle( const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life, qhandle_t shader ) {
	// CPU particle implementation - add particle to CPU-side buffer
	// Particles are stored and can be rendered later or uploaded to GPU particle system
	
	if (cpu_particle_count >= MAX_CPU_PARTICLES) {
		// Buffer full, skip this particle
		return;
	}
	
	cpu_particle_t *particle = &cpu_particles[cpu_particle_count];
	VectorCopy(origin, particle->origin);
	VectorCopy(velocity, particle->velocity);
	VectorCopy(color, particle->color);
	particle->size = size;
	particle->life = life;
	particle->maxLife = life; // Store original life for fade calculations
	particle->shader = shader;
	particle->active = qtrue;
	
	cpu_particle_count++;
}

/*
=====================
R_ProcessCPUParticles

Update CPU-side particles: apply velocity, decrease life, remove dead particles
=====================
*/
static void R_ProcessCPUParticles( float deltaTime ) {
	if (cpu_particle_count == 0) {
		return;
	}
	
	// Process each particle
	int writeIndex = 0;
	for (int i = 0; i < cpu_particle_count; i++) {
		cpu_particle_t *particle = &cpu_particles[i];
		
		if (!particle->active) {
			continue; // Skip inactive particles
		}
		
		// Update position based on velocity
		vec3_t delta;
		VectorScale(particle->velocity, deltaTime, delta);
		VectorAdd(particle->origin, delta, particle->origin);
		
		// Decrease life
		particle->life -= deltaTime;
		
		// Remove dead particles
		if (particle->life <= 0.0f) {
			particle->active = qfalse;
			continue; // Don't copy to writeIndex
		}
		
		// Keep alive particles, compact array
		if (writeIndex != i) {
			cpu_particles[writeIndex] = cpu_particles[i];
		}
		writeIndex++;
	}
	
	// Update count to reflect removed particles
	cpu_particle_count = writeIndex;
}

/*
=====================
R_RenderCPUParticles

Render CPU-side particles as billboarded sprites
=====================
*/
static void R_RenderCPUParticles( void ) {
	if (cpu_particle_count == 0 || !vk.active) {
		return;
	}
	
	// Check if GPU particle system is enabled and should be used
	extern cvar_t *r_particles_gpu;
	extern void vk_particles_upload_cpu_particles(const cpu_particle_t *particles, int count);
	extern void vk_particles_render(void);
	
	if (r_particles_gpu && r_particles_gpu->integer) {
		// Upload particles to GPU particle buffer for GPU-based rendering
		vk_particles_upload_cpu_particles(cpu_particles, cpu_particle_count);
		vk_particles_render();
		return;
	}
	
	// CPU-based rendering: render particles as billboarded quads
	// Group particles by shader to minimize state changes
	qhandle_t currentShader = 0;
	qboolean shaderChanged = qfalse;
	shader_t *shader = NULL;
	
	// Sort particles by shader for batching (simple approach: render in order)
	// For better performance, could sort particles by shader handle
	
	for (int i = 0; i < cpu_particle_count; i++) {
		cpu_particle_t *p = &cpu_particles[i];
		
		if (!p->active || p->life <= 0.0f) {
			continue;
		}
		
		// Check if we need to change shader
		if (currentShader != p->shader) {
			if (shaderChanged) {
				RB_EndSurface();
			}
			
			shader = R_GetShaderByHandle(p->shader);
			if (shader) {
				RB_BeginSurface(shader, -1);
				currentShader = p->shader;
				shaderChanged = qtrue;
			} else {
				// Invalid shader, skip this particle
				continue;
			}
		}
		
		// Calculate billboard vectors
		// Use view axis vectors to create billboarded quad
		vec3_t left, up;
		float radius = p->size;
		
		// Scale view axis vectors by particle size
		VectorScale(backEnd.viewParms.or.axis[1], radius, left);
		VectorScale(backEnd.viewParms.or.axis[2], radius, up);
		
		// Handle mirror views
		if (backEnd.viewParms.portalView == PV_MIRROR) {
			VectorSubtract(vec3_origin, left, left);
		}
		
		// Create color with alpha fade based on remaining life
		color4ub_t color;
		float lifeRatio = p->maxLife > 0.0f ? (p->life / p->maxLife) : 1.0f;
		if (lifeRatio < 0.0f) lifeRatio = 0.0f;
		if (lifeRatio > 1.0f) lifeRatio = 1.0f;
		
		byte alpha = (byte)(255.0f * lifeRatio);
		color.rgba[0] = (byte)(p->color[0] * 255.0f);
		color.rgba[1] = (byte)(p->color[1] * 255.0f);
		color.rgba[2] = (byte)(p->color[2] * 255.0f);
		color.rgba[3] = alpha;
		
		// Add quad to tessellator
		RB_AddQuadStamp(p->origin, left, up, color);
	}
	
	// End the last surface
	if (shaderChanged) {
		RB_EndSurface();
	}
}
