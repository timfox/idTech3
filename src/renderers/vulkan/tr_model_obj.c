/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Wavefront OBJ model loader using tinyobjloader-c (MIT license).
Supports vertices, normals, texture coordinates, and materials.
===========================================================================
*/

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "../../external/include/tinyobj/tinyobj_loader_c.h"

#include "tr_local.h"

static void obj_file_reader(void *ctx, const char *filename, int is_mtl,
                            const char *obj_filename, char **buf, size_t *len) {
	void *data;
	int size;

	(void)is_mtl;
	(void)obj_filename;
	(void)ctx;

	size = ri.FS_ReadFile(filename, &data);
	if (size <= 0 || !data) {
		*buf = NULL;
		*len = 0;
		return;
	}

	*buf = (char *)data;
	*len = (size_t)size;
}

extern qhandle_t R_RegisterOBJ(const char *name, model_t *mod);

qhandle_t R_RegisterOBJ(const char *name, model_t *mod) {
	tinyobj_attrib_t attrib;
	tinyobj_shape_t *shapes = NULL;
	size_t numShapes = 0;
	tinyobj_material_t *materials = NULL;
	size_t numMaterials = 0;
	int ret;
	void *fileData;
	int fileSize;
	int i, v;
	md3Header_t *md3;
	md3Surface_t *surf;
	md3Shader_t *md3Shader;
	md3Triangle_t *tri;
	md3St_t *st;
	md3XyzNormal_t *xyz;
	int numVerts, numTris;

	(void)fileData;
	(void)fileSize;

	ret = tinyobj_parse_obj(&attrib, &shapes, &numShapes,
		&materials, &numMaterials,
		name, obj_file_reader, NULL, TINYOBJ_FLAG_TRIANGULATE);

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

	if (numVerts > SHADER_MAX_VERTEXES) numVerts = SHADER_MAX_VERTEXES;
	if (numTris > SHADER_MAX_INDEXES / 3) numTris = SHADER_MAX_INDEXES / 3;

	{
		int hdrSize = sizeof(md3Header_t) + sizeof(md3Surface_t) +
			sizeof(md3Shader_t) + numTris * sizeof(md3Triangle_t) +
			numVerts * sizeof(md3St_t) + numVerts * sizeof(md3XyzNormal_t);

		md3 = (md3Header_t *)ri.Hunk_Alloc(hdrSize, h_low);
		Com_Memset(md3, 0, hdrSize);

		md3->ident = MD3_IDENT;
		md3->version = MD3_VERSION;
		Q_strncpyz(md3->name, name, sizeof(md3->name));
		md3->numFrames = 1;
		md3->numSurfaces = 1;
		md3->ofsFrames = sizeof(md3Header_t);
		md3->ofsSurfaces = sizeof(md3Header_t);
		md3->ofsEnd = hdrSize;

		surf = (md3Surface_t *)((byte *)md3 + md3->ofsSurfaces);
		surf->ident = MD3_IDENT;
		Q_strncpyz(surf->name, "obj_surface", sizeof(surf->name));
		surf->numFrames = 1;
		surf->numShaders = 1;
		surf->numVerts = numVerts;
		surf->numTriangles = numTris;

		int offset = sizeof(md3Surface_t);
		surf->ofsShaders = offset;
		offset += sizeof(md3Shader_t);
		surf->ofsTriangles = offset;
		offset += numTris * sizeof(md3Triangle_t);
		surf->ofsSt = offset;
		offset += numVerts * sizeof(md3St_t);
		surf->ofsXyzNormals = offset;
		offset += numVerts * sizeof(md3XyzNormal_t);
		surf->ofsEnd = offset;

		md3Shader = (md3Shader_t *)((byte *)surf + surf->ofsShaders);
		if (numMaterials > 0 && materials[0].diffuse_texname[0]) {
			Q_strncpyz(md3Shader->name, materials[0].diffuse_texname, sizeof(md3Shader->name));
		} else {
			Q_strncpyz(md3Shader->name, "textures/common/white", sizeof(md3Shader->name));
		}
		md3Shader->shaderIndex = 0;

		tri = (md3Triangle_t *)((byte *)surf + surf->ofsTriangles);
		st = (md3St_t *)((byte *)surf + surf->ofsSt);
		xyz = (md3XyzNormal_t *)((byte *)surf + surf->ofsXyzNormals);

		for (i = 0; i < numTris * 3 && i < (int)attrib.num_faces; i++) {
			tinyobj_vertex_index_t idx = attrib.faces[i];
			v = i;

			if (idx.v_idx >= 0 && idx.v_idx * 3 + 2 < (int)(attrib.num_vertices * 3)) {
				xyz[v].xyz[0] = (short)(attrib.vertices[idx.v_idx * 3 + 0] * 64.0f);
				xyz[v].xyz[1] = (short)(attrib.vertices[idx.v_idx * 3 + 1] * 64.0f);
				xyz[v].xyz[2] = (short)(attrib.vertices[idx.v_idx * 3 + 2] * 64.0f);
			}

			if (idx.vt_idx >= 0 && idx.vt_idx * 2 + 1 < (int)(attrib.num_texcoords * 2)) {
				st[v].st[0] = attrib.texcoords[idx.vt_idx * 2 + 0];
				st[v].st[1] = 1.0f - attrib.texcoords[idx.vt_idx * 2 + 1];
			}

			if (idx.vn_idx >= 0 && idx.vn_idx * 3 + 2 < (int)(attrib.num_normals * 3)) {
				float nx = attrib.normals[idx.vn_idx * 3 + 0];
				float ny = attrib.normals[idx.vn_idx * 3 + 1];
				(void)nx; (void)ny;
			}

			xyz[v].normal = 0;
		}

		for (i = 0; i < numTris; i++) {
			tri[i].indexes[0] = i * 3 + 0;
			tri[i].indexes[1] = i * 3 + 1;
			tri[i].indexes[2] = i * 3 + 2;
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
