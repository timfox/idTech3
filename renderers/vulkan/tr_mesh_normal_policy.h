/*
===========================================================================
Experimental authored / hard-edge mesh normal policy.

Default policy 0 preserves certified legacy behavior.
Applies first to glTF/GLB (DCC-authored split normals). OBJ already expands
corners by (v,vt,vn). MD3/BSP remain untouched unless future opt-in.
===========================================================================
*/

#ifndef TR_MESH_NORMAL_POLICY_H
#define TR_MESH_NORMAL_POLICY_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gltfPrimitive_s;
struct gltfModel_s;

typedef enum {
	MESH_NORMAL_POLICY_LEGACY = 0,           /* certified default: do nothing */
	MESH_NORMAL_POLICY_PRESERVE = 1,         /* keep authored; fill missing smooth */
	MESH_NORMAL_POLICY_PRESERVE_OR_ANGLE = 2,/* keep authored; else crease-angle + splits */
	MESH_NORMAL_POLICY_FORCE_ANGLE = 3,      /* regenerate crease-angle + splits */
	MESH_NORMAL_POLICY_DEBUG = 4             /* preserve + log comparison stats */
} meshNormalPolicy_t;

typedef struct {
	int vertsBefore;
	int vertsAfter;
	int authoredNormals;
	int invalidNormals;
	int hardEdges;
	int softEdges;
	int normalSplits;
	int tangentGenerated;
	float maxAuthoredVsGeneratedDeg;
	meshNormalPolicy_t effectivePolicy;
	float creaseAngleDeg;
	char sourceHint[32];
} meshNormalStats_t;

void R_MeshNormalPolicy_Init( void );
void R_MeshNormalPolicy_Shutdown( void );

meshNormalPolicy_t R_MeshNormalPolicy_Get( void );
qboolean R_MeshNormalPolicy_PreserveAuthored( void );
float R_MeshNormalPolicy_HardEdgeAngleDeg( void );
qboolean R_MeshNormalPolicy_SplitTangentsAtHardEdges( void );
qboolean R_MeshNormalPolicy_Active( void ); /* policy != legacy */

/* Process one glTF primitive in-place (may expand verts/indices on hunk). */
void R_MeshNormalPolicy_ProcessGLTFPrimitive( struct gltfPrimitive_s *prim,
	const char *modelName, meshNormalStats_t *outStats );

void R_MeshNormalPolicy_ProcessGLTFModel( struct gltfModel_s *model, const char *modelName );

/* Console: mesh_normal_status [modelname] */
void R_MeshNormalStatus_f( void );

#ifdef __cplusplus
}
#endif

#endif /* TR_MESH_NORMAL_POLICY_H */
