/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Wavefront OBJ model loader using tinyobjloader-c (MIT license).
Supports vertices, normals, texture coordinates, materials, and surface splitting.
===========================================================================
*/

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "../../external/include/tinyobj/tinyobj_loader_c.h"

#include <math.h>
#include "tr_local.h"

#define OBJ_MAX_VERTS   SHADER_MAX_VERTEXES
#define OBJ_MAX_TRIS    (SHADER_MAX_INDEXES / 3)
#define OBJ_VERTS_PER_SURF  OBJ_MAX_VERTS
#define OBJ_TRIS_PER_SURF   (OBJ_VERTS_PER_SURF / 3)

typedef struct {
	void *objBuf;
	void *mtlBuf;
} obj_read_ctx_t;

static void obj_file_reader(void *ctx, const char *filename, int is_mtl,
                            const char *obj_filename, char **buf, size_t *len) {
	void *data;
	int size;
	obj_read_ctx_t *readCtx = (obj_read_ctx_t *)ctx;

	(void)obj_filename;

	size = ri.FS_ReadFile(filename, &data);
	if (size <= 0 || !data) {
		*buf = NULL;
		*len = 0;
		return;
	}

	*buf = (char *)data;
	*len = (size_t)size;

	if (is_mtl) {
		readCtx->mtlBuf = data;
	} else {
		readCtx->objBuf = data;
	}
}

/*
=================
R_VectorToLatLong
=================
Encode unit normal to MD3 lat/long format (lat high byte, lng low byte).
*/
static short R_VectorToLatLong(const vec3_t n) {
	float lat, lng;
	float nz = Com_Clamp(-1.0f, 1.0f, n[2]);

	if (VectorLengthSquared(n) < 0.0001f) {
		return 0;
	}

	lng = acosf(nz);
	lat = atan2f(n[1], n[0]);
	if (lat < 0.0f) {
		lat += (float)(M_PI * 2.0);
	}

	{
		unsigned int lat8 = (unsigned int)(lat * (255.0f / (2.0f * (float)M_PI))) & 0xff;
		unsigned int lng8 = (unsigned int)(lng * (255.0f / (float)M_PI)) & 0xff;
		return (short)((lat8 << 8) | lng8);
	}
}

/*
=================
R_OBJ_ResolveTexturePath
=================
Convert MTL diffuse_texname to engine shader path (e.g. textures/common/white).
*/
static void R_OBJ_ResolveTexturePath(const char *objName, const char *texName,
                                    char *out, int outSize) {
	char dir[MAX_QPATH];
	char stripped[MAX_QPATH];
	int i, len;

	if (!texName || !texName[0]) {
		Q_strncpyz(out, "textures/common/white", outSize);
		return;
	}

	/* Strip extension from tex name */
	COM_StripExtension(texName, stripped, sizeof(stripped));

	/* If already looks like engine path, use as-is */
	if (strstr(texName, "textures/") == texName) {
		Q_strncpyz(out, stripped, outSize);
		return;
	}

	if (stripped[0] == '/' || stripped[0] == '\\') {
		Q_strncpyz(out, stripped, outSize);
		return;
	}

	/* Get directory of obj file */
	len = (int)strlen(objName);
	for (i = len - 1; i >= 0; i--) {
		if (objName[i] == '/' || objName[i] == '\\') {
			break;
		}
	}
	if (i >= 0 && (size_t)(i + 2) <= sizeof(dir)) {
		Q_strncpyz(dir, objName, (size_t)(i + 2));
		Com_sprintf(out, outSize, "%s%s", dir, stripped);
	} else {
		Q_strncpyz(out, stripped, outSize);
	}
}

extern qhandle_t R_RegisterOBJ(const char *name, model_t *mod);

qhandle_t R_RegisterOBJ(const char *name, model_t *mod) {
	tinyobj_attrib_t attrib;
	tinyobj_shape_t *shapes = NULL;
	size_t numShapes = 0;
	tinyobj_material_t *materials = NULL;
	size_t numMaterials = 0;
	obj_read_ctx_t readCtx;
	int ret;
	int i, s, v;
	int numVerts, numTris;
	vec3_t mins, maxs, localOrigin;
	float radius;
	md3Header_t *md3;

	Com_Memset(&readCtx, 0, sizeof(readCtx));

	ret = tinyobj_parse_obj(&attrib, &shapes, &numShapes,
		&materials, &numMaterials,
		name, obj_file_reader, &readCtx, TINYOBJ_FLAG_TRIANGULATE);

	if (readCtx.objBuf) {
		ri.FS_FreeFile(readCtx.objBuf);
	}
	if (readCtx.mtlBuf) {
		ri.FS_FreeFile(readCtx.mtlBuf);
	}

	if (ret != TINYOBJ_SUCCESS) {
		ri.Printf(PRINT_WARNING, "OBJ: parse error in %s\n", name);
		return 0;
	}

	numVerts = (int)attrib.num_face_num_verts;
	numTris = (int)(attrib.num_faces / 3);

	if (numVerts == 0 || numTris == 0) {
		tinyobj_attrib_free(&attrib);
		tinyobj_shapes_free(shapes, numShapes);
		tinyobj_materials_free(materials, numMaterials);
		ri.Printf(PRINT_WARNING, "OBJ: no geometry in %s\n", name);
		return 0;
	}

	/* Compute bounds from vertex data */
	if (attrib.num_vertices > 0) {
		mins[0] = maxs[0] = attrib.vertices[0];
		mins[1] = maxs[1] = attrib.vertices[1];
		mins[2] = maxs[2] = attrib.vertices[2];
		for (i = 1; i < (int)attrib.num_vertices; i++) {
			float *vert = &attrib.vertices[i * 3];
			if (vert[0] < mins[0]) mins[0] = vert[0];
			if (vert[1] < mins[1]) mins[1] = vert[1];
			if (vert[2] < mins[2]) mins[2] = vert[2];
			if (vert[0] > maxs[0]) maxs[0] = vert[0];
			if (vert[1] > maxs[1]) maxs[1] = vert[1];
			if (vert[2] > maxs[2]) maxs[2] = vert[2];
		}
		localOrigin[0] = (mins[0] + maxs[0]) * 0.5f;
		localOrigin[1] = (mins[1] + maxs[1]) * 0.5f;
		localOrigin[2] = (mins[2] + maxs[2]) * 0.5f;
		radius = 0.5f * sqrtf(
			(maxs[0] - mins[0]) * (maxs[0] - mins[0]) +
			(maxs[1] - mins[1]) * (maxs[1] - mins[1]) +
			(maxs[2] - mins[2]) * (maxs[2] - mins[2]));
	} else {
		VectorClear(mins);
		VectorClear(maxs);
		VectorClear(localOrigin);
		radius = 1.0f;
	}

	{
		int numSurfaces;
		int vertOffset;
		md3Frame_t *frame;
		md3Surface_t *surf;
		int surfSize;

		numSurfaces = (numVerts + OBJ_VERTS_PER_SURF - 1) / OBJ_VERTS_PER_SURF;
		if (numSurfaces < 1) numSurfaces = 1;

		surfSize = sizeof(md3Frame_t);
		for (s = 0; s < numSurfaces; s++) {
			int surfVerts = numVerts - s * OBJ_VERTS_PER_SURF;
			int surfTris;
			if (surfVerts > OBJ_VERTS_PER_SURF) surfVerts = OBJ_VERTS_PER_SURF;
			surfTris = surfVerts / 3;
			surfSize += sizeof(md3Surface_t) + sizeof(md3Shader_t) +
				surfTris * sizeof(md3Triangle_t) +
				surfVerts * (sizeof(md3St_t) + sizeof(md3XyzNormal_t));
		}

		md3 = (md3Header_t *)ri.Hunk_Alloc(sizeof(md3Header_t) + surfSize, h_low);
		Com_Memset(md3, 0, sizeof(md3Header_t) + surfSize);

		md3->ident = MD3_IDENT;
		md3->version = MD3_VERSION;
		Q_strncpyz(md3->name, name, sizeof(md3->name));
		md3->numFrames = 1;
		md3->numSurfaces = numSurfaces;
		md3->ofsFrames = sizeof(md3Header_t);
		md3->ofsTags = sizeof(md3Header_t) + sizeof(md3Frame_t);
		md3->ofsSurfaces = sizeof(md3Header_t) + sizeof(md3Frame_t);
		md3->ofsEnd = sizeof(md3Header_t) + surfSize;

		frame = (md3Frame_t *)((byte *)md3 + md3->ofsFrames);
		VectorCopy(mins, frame->bounds[0]);
		VectorCopy(maxs, frame->bounds[1]);
		VectorCopy(localOrigin, frame->localOrigin);
		frame->radius = radius;
		Q_strncpyz(frame->name, "default", sizeof(frame->name));

		surf = (md3Surface_t *)((byte *)md3 + md3->ofsSurfaces);
		vertOffset = 0;

		for (s = 0; s < numSurfaces; s++) {
			int surfVerts = numVerts - vertOffset;
			int surfTris;
			md3Shader_t *md3Shader;
			md3Triangle_t *tri;
			md3St_t *st;
			md3XyzNormal_t *xyz;
			char shaderName[MAX_QPATH];

			if (surfVerts > OBJ_VERTS_PER_SURF) surfVerts = OBJ_VERTS_PER_SURF;
			surfTris = surfVerts / 3;

			surf->ident = SF_MD3;
			Com_sprintf(surf->name, sizeof(surf->name), "obj_surface%i", s);
			surf->name[sizeof(surf->name) - 1] = '\0';
			Q_strlwr(surf->name);
			surf->numFrames = 1;
			surf->numShaders = 1;
			surf->numVerts = surfVerts;
			surf->numTriangles = surfTris;

			{
				int offset = sizeof(md3Surface_t);
				surf->ofsShaders = offset;
				offset += sizeof(md3Shader_t);
				surf->ofsTriangles = offset;
				offset += surfTris * sizeof(md3Triangle_t);
				surf->ofsSt = offset;
				offset += surfVerts * sizeof(md3St_t);
				surf->ofsXyzNormals = offset;
				offset += surfVerts * sizeof(md3XyzNormal_t);
				surf->ofsEnd = offset;
			}

			md3Shader = (md3Shader_t *)((byte *)surf + surf->ofsShaders);
			if (numMaterials > 0 && materials[0].diffuse_texname && materials[0].diffuse_texname[0]) {
				R_OBJ_ResolveTexturePath(name, materials[0].diffuse_texname, shaderName, sizeof(shaderName));
			} else {
				Q_strncpyz(shaderName, "textures/common/white", sizeof(shaderName));
			}
			Q_strncpyz(md3Shader->name, shaderName, sizeof(md3Shader->name));
			{
				shader_t *sh = R_FindShader(md3Shader->name, LIGHTMAP_NONE, qtrue);
				md3Shader->shaderIndex = sh->defaultShader ? 0 : sh->index;
			}

			tri = (md3Triangle_t *)((byte *)surf + surf->ofsTriangles);
			st = (md3St_t *)((byte *)surf + surf->ofsSt);
			xyz = (md3XyzNormal_t *)((byte *)surf + surf->ofsXyzNormals);

			for (i = 0; i < surfVerts && (vertOffset + i) < (int)attrib.num_faces; i++) {
				tinyobj_vertex_index_t idx = attrib.faces[vertOffset + i];
				vec3_t faceNormal;
				int triBase;

				v = i;

				if (idx.v_idx >= 0 && idx.v_idx * 3 + 2 < (int)(attrib.num_vertices * 3)) {
					xyz[v].xyz[0] = (short)(attrib.vertices[idx.v_idx * 3 + 0] * 64.0f);
					xyz[v].xyz[1] = (short)(attrib.vertices[idx.v_idx * 3 + 1] * 64.0f);
					xyz[v].xyz[2] = (short)(attrib.vertices[idx.v_idx * 3 + 2] * 64.0f);
				}

				if (idx.vt_idx >= 0 && idx.vt_idx * 2 + 1 < (int)(attrib.num_texcoords * 2)) {
					st[v].st[0] = attrib.texcoords[idx.vt_idx * 2 + 0];
					st[v].st[1] = 1.0f - attrib.texcoords[idx.vt_idx * 2 + 1];
				} else {
					st[v].st[0] = 0.0f;
					st[v].st[1] = 0.0f;
				}

				if (idx.vn_idx >= 0 && idx.vn_idx * 3 + 2 < (int)(attrib.num_normals * 3)) {
					vec3_t n;
					n[0] = attrib.normals[idx.vn_idx * 3 + 0];
					n[1] = attrib.normals[idx.vn_idx * 3 + 1];
					n[2] = attrib.normals[idx.vn_idx * 3 + 2];
					xyz[v].normal = R_VectorToLatLong(n);
				} else {
					/* Face-normal fallback: compute from triangle */
					triBase = (vertOffset + i) / 3;
					if (triBase * 3 + 2 < (int)attrib.num_faces) {
						tinyobj_vertex_index_t i0 = attrib.faces[triBase * 3 + 0];
						tinyobj_vertex_index_t i1 = attrib.faces[triBase * 3 + 1];
						tinyobj_vertex_index_t i2 = attrib.faces[triBase * 3 + 2];
						if (i0.v_idx >= 0 && i1.v_idx >= 0 && i2.v_idx >= 0 &&
							i0.v_idx * 3 + 2 < (int)(attrib.num_vertices * 3) &&
							i1.v_idx * 3 + 2 < (int)(attrib.num_vertices * 3) &&
							i2.v_idx * 3 + 2 < (int)(attrib.num_vertices * 3)) {
							vec3_t v0, v1, v2, e1, e2;
							v0[0] = attrib.vertices[i0.v_idx * 3 + 0];
							v0[1] = attrib.vertices[i0.v_idx * 3 + 1];
							v0[2] = attrib.vertices[i0.v_idx * 3 + 2];
							v1[0] = attrib.vertices[i1.v_idx * 3 + 0];
							v1[1] = attrib.vertices[i1.v_idx * 3 + 1];
							v1[2] = attrib.vertices[i1.v_idx * 3 + 2];
							v2[0] = attrib.vertices[i2.v_idx * 3 + 0];
							v2[1] = attrib.vertices[i2.v_idx * 3 + 1];
							v2[2] = attrib.vertices[i2.v_idx * 3 + 2];
							VectorSubtract(v1, v0, e1);
							VectorSubtract(v2, v0, e2);
							CrossProduct(e1, e2, faceNormal);
							if (VectorNormalize(faceNormal) > 0.0001f) {
								xyz[v].normal = R_VectorToLatLong(faceNormal);
							} else {
								xyz[v].normal = 0;
							}
						} else {
							xyz[v].normal = 0;
						}
					} else {
						xyz[v].normal = 0;
					}
				}
			}

			for (i = 0; i < surfTris; i++) {
				tri[i].indexes[0] = i * 3 + 0;
				tri[i].indexes[1] = i * 3 + 1;
				tri[i].indexes[2] = i * 3 + 2;
			}

			vertOffset += surfVerts;
			surf = (md3Surface_t *)((byte *)surf + surf->ofsEnd);
		}
	}

	mod->type = MOD_MESH;
	mod->dataSize = 0;
	mod->md3[0] = md3;

	tinyobj_attrib_free(&attrib);
	tinyobj_shapes_free(shapes, numShapes);
	tinyobj_materials_free(materials, numMaterials);

	ri.Printf(PRINT_ALL, "OBJ: loaded %s (%d verts, %d tris)\n", name, numVerts, numTris);
	return mod->index;
}
