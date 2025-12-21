/*
===========================================================================
Copyright (C) 2024 id Tech 3

TIKI model rendering implementation.
Inspired by Ritual Entertainment's ÜberTools TIKI model system.
===========================================================================
*/

#include "tr_local.h"
#include "tr_tiki.h"

/*
=================
R_TIKICullModel
=================
Cull a TIKI model
=================
*/
static int R_TIKICullModel(tikiData_t *tikiData, trRefEntity_t *ent)
{
	vec3_t bounds[2];
	int i;
	
	if (!tikiData || !tikiData->header)
		return CULL_OUT;
	
	// TODO: Calculate proper bounds from bone hierarchy and frames
	// For now, use simple bounds around entity origin
	VectorCopy(ent->e.origin, bounds[0]);
	VectorCopy(ent->e.origin, bounds[1]);
	
	// Expand bounds (default model size)
	for (i = 0; i < 3; i++) {
		bounds[0][i] -= 64.0f;
		bounds[1][i] += 64.0f;
	}
	
	return R_CullLocalBox(bounds);
}

/*
=================
R_TIKIComputeFogNum
=================
Compute fog number for TIKI model
=================
*/
static int R_TIKIComputeFogNum(tikiData_t *tikiData, trRefEntity_t *ent)
{
	(void)tikiData; // Unused - kept for API compatibility
	int i, j;
	const fog_t *fog;
	vec3_t localOrigin;
	
	if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
		return 0;
	}
	
	// Use entity origin for fog calculation
	VectorCopy(ent->e.origin, localOrigin);
	
	for (i = 1; i < tr.world->numfogs; i++) {
		fog = &tr.world->fogs[i];
		for (j = 0; j < 3; j++) {
			if (localOrigin[j] - 64.0f >= fog->bounds[1][j]) {
				break;
			}
			if (localOrigin[j] + 64.0f <= fog->bounds[0][j]) {
				break;
			}
		}
		if (j == 3) {
			return i;
		}
	}
	
	return 0;
}

/*
=================
R_TIKIAddAnimSurfaces
=================
Add TIKI model surfaces for rendering
=================
*/
void R_TIKIAddAnimSurfaces(trRefEntity_t *ent)
{
	tikiData_t		*tikiData;
	tikiSurface_t	*surf;
	shader_t		*shader;
	skin_t			*skin;
	int				i, j;
	int				fogNum = 0;
	int				cull;
	qboolean		personalModel;
	
	if (!tr.currentModel || tr.currentModel->type != MOD_TIKI)
		return;
	
	tikiData = tr.currentModel->modelData.tiki;
	if (!tikiData || !tikiData->header)
		return;
	
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE);
	
	// Validate frames
	if (ent->e.frame >= tikiData->numFrames || ent->e.frame < 0) {
		ri.Printf(PRINT_DEVELOPER, "R_TIKIAddAnimSurfaces: invalid frame %d for '%s'\n",
			ent->e.frame, tr.currentModel->name);
		ent->e.frame = 0;
	}
	
	if (ent->e.oldframe >= tikiData->numFrames || ent->e.oldframe < 0) {
		ent->e.oldframe = ent->e.frame;
	}
	
	// Cull model
	cull = R_TIKICullModel(tikiData, ent);
	if (cull == CULL_OUT) {
		return;
	}
	
	// Set up lighting
	if (!personalModel || r_shadows->integer > 1) {
		R_SetupEntityLighting(&tr.refdef, ent);
	}
	
	// Get fog number
	fogNum = R_TIKIComputeFogNum(tikiData, ent);
	
	// Add surfaces
	surf = tikiData->surfaces;
	for (i = 0; i < tikiData->numSurfaces; i++) {
		// Determine shader
		if (ent->e.customShader) {
			shader = R_GetShaderByHandle(ent->e.customShader);
		} else if (ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins) {
			skin = R_GetSkinByHandle(ent->e.customSkin);
			shader = tr.defaultShader;
			
			for (j = 0; j < skin->numSurfaces; j++) {
				if (!strcmp(skin->surfaces[j].name, surf->name)) {
					shader = skin->surfaces[j].shader;
					break;
				}
			}
		} else {
			shader = tr.defaultShader;
		}
		
		// Add shadows if needed
		if (!personalModel
			&& r_shadows->integer == 2
			&& fogNum == 0
			&& !(ent->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
			&& shader->sort == SS_OPAQUE) {
			R_AddDrawSurf((void *)surf, tr.shadowShader, 0, 0);
		}
		
		// Add surface for rendering
		if (!personalModel) {
			R_AddDrawSurf((void *)surf, shader, fogNum, 0);
		}
		
		surf = (tikiSurface_t *)((byte *)surf + surf->ofsEnd);
	}
}

/*
=================
RB_TIKISurfaceAnim
=================
Render a TIKI surface with skeletal animation
=================
*/
void RB_TIKISurfaceAnim(tikiSurface_t *surface)
{
	int				i;
	float			frontlerp, backlerp;
	tikiData_t		*tikiData;
	tikiFrame_t		*frame, *oldFrame;
	tikiVertex_t	*vert;
	tikiTriangle_t	*tri;
	// TODO: Bone animation variables (for future implementation)
	// tikiBone_t *bone, *bonePtr;
	tikiBoneWeight_t *weight; // Used for bone weight data access
	// vec3_t tempVert, tempNormal;
	// vec3_t bonePos, boneScale;
	// quat_t boneRot;
	(void)frontlerp; (void)backlerp; (void)frame; (void)oldFrame; (void)weight;
	
	if (!surface || !tr.currentModel || tr.currentModel->type != MOD_TIKI)
		return;
	
	tikiData = tr.currentModel->modelData.tiki;
	if (!tikiData || !tikiData->header)
		return;
	
#ifdef USE_VBO
	VBO_Flush();
#endif

	tess.surfType = SF_TIKI;
	
	// Calculate lerp
	if (backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame) {
		backlerp = 0.0f;
		frontlerp = 1.0f;
	} else {
		backlerp = backEnd.currentEntity->e.backlerp;
		frontlerp = 1.0f - backlerp;
	}
	
	// Get frames
	if (backEnd.currentEntity->e.frame >= tikiData->numFrames ||
		backEnd.currentEntity->e.oldframe >= tikiData->numFrames) {
		return;
	}
	
	frame = &tikiData->frames[backEnd.currentEntity->e.frame];
	oldFrame = &tikiData->frames[backEnd.currentEntity->e.oldframe];
	
	RB_CHECKOVERFLOW(surface->numVerts, surface->numTris * 3);
	
	// Add triangles
	tri = (tikiTriangle_t *)((byte *)surface + surface->ofsTris);
	for (i = 0; i < surface->numTris; i++) {
		tess.indexes[tess.numIndexes++] = tess.numVertexes + tri[i].indices[0];
		tess.indexes[tess.numIndexes++] = tess.numVertexes + tri[i].indices[1];
		tess.indexes[tess.numIndexes++] = tess.numVertexes + tri[i].indices[2];
	}
	
	// Deform vertices by bones
	vert = (tikiVertex_t *)((byte *)surface + surface->ofsVerts);
	weight = (tikiBoneWeight_t *)((byte *)surface + surface->ofsBoneWeights);
	
	// For now, render vertices without bone deformation
	// TODO: Implement proper skeletal animation with bone weights
	for (i = 0; i < surface->numVerts; i++) {
		// Simple transform - just use vertex position directly
		// Full bone deformation will be implemented later
		VectorCopy(vert[i].position, tess.xyz[tess.numVertexes]);
		VectorCopy(vert[i].normal, tess.normal[tess.numVertexes]);
		tess.texCoords[0][tess.numVertexes][0] = vert[i].st[0];
		tess.texCoords[0][tess.numVertexes][1] = vert[i].st[1];
		
		// Copy vertex color (color4ub_t is a union with byte rgba[4])
		if (vert[i].color[0] || vert[i].color[1] || vert[i].color[2] || vert[i].color[3]) {
			tess.vertexColors[tess.numVertexes].rgba[0] = vert[i].color[0];
			tess.vertexColors[tess.numVertexes].rgba[1] = vert[i].color[1];
			tess.vertexColors[tess.numVertexes].rgba[2] = vert[i].color[2];
			tess.vertexColors[tess.numVertexes].rgba[3] = vert[i].color[3];
		} else {
			// Default white
			tess.vertexColors[tess.numVertexes].rgba[0] = 255;
			tess.vertexColors[tess.numVertexes].rgba[1] = 255;
			tess.vertexColors[tess.numVertexes].rgba[2] = 255;
			tess.vertexColors[tess.numVertexes].rgba[3] = 255;
		}
		
		tess.numVertexes++;
	}
}

