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
#include <math.h>

// Vulkan function pointers
extern PFN_vkCmdSetViewport qvkCmdSetViewport;
extern PFN_vkCmdSetScissor qvkCmdSetScissor;
extern PFN_vkCmdEndRenderPass qvkCmdEndRenderPass;

// Scene Rendering Functions for Vulkan Renderer
// Handles 3D scene rendering, entities, polygons, and world geometry

void vk_add_entity(const refEntity_t *re, int intShaderTime) {
    // Add entity to scene using standard pipeline
    // This function stores entities directly in backEndData->entities[]
    // to be processed by R_GenerateDrawSurfs() during the main render pass
    // We implement the standard pattern directly here to avoid circular dependency
    // with RE_AddRefEntityToScene in tr_rtx.c
    
    if (!vk.active || !re) {
        return;
    }
    
    extern int r_numentities;
    extern backEndData_t *backEndData;
    
    if (!tr.registered) {
        return;
    }
    if (r_numentities >= MAX_REFENTITIES) {
        ri.Printf(PRINT_DEVELOPER, "vk_add_entity: Dropping refEntity, reached MAX_REFENTITIES\n");
        return;
    }
    if (isnan(re->origin[0]) || isnan(re->origin[1]) || isnan(re->origin[2])) {
        static qboolean first_time = qtrue;
        if (first_time) {
            first_time = qfalse;
            ri.Printf(PRINT_WARNING, "vk_add_entity passed a refEntity with NaN origin\n");
        }
        return;
    }
    if ((unsigned)re->reType >= RT_MAX_REF_ENTITY_TYPE) {
        ri.Error(ERR_DROP, "vk_add_entity: bad reType %i", re->reType);
        return;
    }
    
    // Store in backEndData using standard pattern
    backEndData->entities[r_numentities].e = *re;
    backEndData->entities[r_numentities].lightingCalculated = qfalse;
    backEndData->entities[r_numentities].intShaderTime = intShaderTime ? qtrue : qfalse;
    
    r_numentities++;
}

void vk_add_polygon(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num) {
    // Add polygon to scene using standard pipeline 
    // This function stores polygons directly in backEndData->polys[]
    // to be processed by R_AddPolygonSurfaces() during the main render pass
    // We implement the standard pattern directly here to avoid circular dependency
    
    if (!vk.active || !verts || numVerts <= 0) {
        return;
    }
    
    extern int r_numpolys;
    extern int r_numpolyverts;
    extern int max_polys;
    extern int max_polyverts;
    extern backEndData_t *backEndData;
    
    if (!tr.registered) {
        return;
    }
    
    srfPoly_t *poly;
    int j;
    
    for (j = 0; j < num; j++) {
        if (r_numpolyverts + numVerts > max_polyverts || r_numpolys >= max_polys) {
            ri.Printf(PRINT_DEVELOPER, "vk_add_polygon: r_max_polys or r_max_polyverts reached\n");
            return;
        }
        
        poly = &backEndData->polys[r_numpolys];
        poly->surfaceType = SF_POLY;
        poly->hShader = hShader;
        poly->numVerts = numVerts;
        poly->verts = &backEndData->polyVerts[r_numpolyverts];
        
        Com_Memcpy(poly->verts, &verts[numVerts * j], numVerts * sizeof(*verts));
        
        r_numpolys++;
        r_numpolyverts += numVerts;
    }
}

void vk_clear_scene(void) {
    // Clear the scene for new frame using standard pipeline
    // The standard RE_ClearScene() in tr_scene.c handles clearing backEndData
    // This wrapper ensures compatibility with the standard pipeline
    extern void RE_ClearScene(void);
    
    if (vk.active) {
        RE_ClearScene();
    }
}

void vk_render_scene_vulkan(const refdef_t *fd) {
    // Standard pipeline integration
    // This function is now a no-op wrapper. The standard rendering pipeline handles everything:
    // 1. Entities/polygons are stored in backEndData via vk_add_entity()/vk_add_polygon()
    // 2. R_GenerateDrawSurfs() processes backEndData and creates draw surfaces
    // 3. The backend renders all draw surfaces together in the main render pass
    //
    // This function exists for API compatibility but does not create its own
    // command buffer or render pass - it relies on the standard pipeline.
    
    Q_UNUSED(fd);
    
    // No-op: Standard pipeline handles rendering
    // The main render pass, command buffers, and draw surface processing
    // are all handled by RE_RenderScene() -> R_RenderView() -> R_GenerateDrawSurfs()
    // in the standard idTech3 pipeline
}