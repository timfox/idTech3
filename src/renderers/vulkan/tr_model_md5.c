/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

MD5 model loader for id Tech 4 (Doom 3) skeletal mesh format.
Parses the text-based .md5mesh format including joints and
weighted vertices. The MD5 format is part of the id Tech 4
GPL2 source release.
===========================================================================
*/

#include "tr_local.h"
#include <stdlib.h>

#define MD5_MAX_JOINTS   128
#define MD5_MAX_MESHES   32
#define MD5_MAX_VERTS    65536
#define MD5_MAX_TRIS     65536
#define MD5_MAX_WEIGHTS  (MD5_MAX_VERTS * 4)

typedef struct {
	char    name[MAX_QPATH];
	int     parent;
	vec3_t  pos;
	vec4_t  orient;
} md5Joint_t;

typedef struct {
	int     jointIndex;
	float   bias;
	vec3_t  pos;
} md5Weight_t;

typedef struct {
	vec2_t  st;
	int     firstWeight;
	int     numWeights;
} md5Vertex_t;

typedef struct {
	int     indexes[3];
} md5Triangle_t;

typedef struct {
	char            shader[MAX_QPATH];
	int             numVerts;
	int             numTris;
	int             numWeights;
	md5Vertex_t    *verts;
	md5Triangle_t  *tris;
	md5Weight_t    *weights;
} md5Mesh_t;

static const char *MD5_ParseToken(const char **text) {
	return COM_Parse(text);
}

static void MD5_SkipLine(const char **text) {
	while (**text && **text != '\n') (*text)++;
	if (**text == '\n') (*text)++;
}

static void MD5_QuatComputeW(vec4_t q) {
	float t = 1.0f - q[0]*q[0] - q[1]*q[1] - q[2]*q[2];
	q[3] = (t < 0.0f) ? 0.0f : -sqrtf(t);
}

extern qhandle_t R_RegisterMD5(const char *name, model_t *mod);

qhandle_t R_RegisterMD5(const char *name, model_t *mod) {
	void *fileData;
	int fileSize;
	const char *text;
	const char *token;
	int version, numJoints, numMeshes;
	md5Joint_t joints[MD5_MAX_JOINTS];
	md5Mesh_t meshes[MD5_MAX_MESHES];
	int i, j, m;

	fileSize = ri.FS_ReadFile(name, &fileData);
	if (fileSize <= 0 || !fileData) {
		return 0;
	}

	text = (const char *)fileData;

	token = MD5_ParseToken(&text);
	if (Q_stricmp(token, "MD5Version")) {
		ri.Printf(PRINT_WARNING, "MD5: %s is not an MD5 file\n", name);
		ri.FS_FreeFile(fileData);
		return 0;
	}

	token = MD5_ParseToken(&text);
	version = atoi(token);
	if (version != 10) {
		ri.Printf(PRINT_WARNING, "MD5: %s has version %d, expected 10\n", name, version);
		ri.FS_FreeFile(fileData);
		return 0;
	}

	numJoints = 0;
	numMeshes = 0;
	Com_Memset(joints, 0, sizeof(joints));
	Com_Memset(meshes, 0, sizeof(meshes));

	while (1) {
		token = MD5_ParseToken(&text);
		if (!token[0]) break;

		if (!Q_stricmp(token, "commandline")) {
			MD5_SkipLine(&text);
		} else if (!Q_stricmp(token, "numJoints")) {
			token = MD5_ParseToken(&text);
			numJoints = atoi(token);
			if (numJoints > MD5_MAX_JOINTS) numJoints = MD5_MAX_JOINTS;
		} else if (!Q_stricmp(token, "numMeshes")) {
			token = MD5_ParseToken(&text);
			numMeshes = atoi(token);
			if (numMeshes > MD5_MAX_MESHES) numMeshes = MD5_MAX_MESHES;
		} else if (!Q_stricmp(token, "joints")) {
			MD5_ParseToken(&text);
			for (i = 0; i < numJoints; i++) {
				token = MD5_ParseToken(&text);
				Q_strncpyz(joints[i].name, token, sizeof(joints[i].name));

				token = MD5_ParseToken(&text);
				joints[i].parent = atoi(token);

				MD5_ParseToken(&text);
				joints[i].pos[0] = (float)atof(MD5_ParseToken(&text));
				joints[i].pos[1] = (float)atof(MD5_ParseToken(&text));
				joints[i].pos[2] = (float)atof(MD5_ParseToken(&text));
				MD5_ParseToken(&text);

				MD5_ParseToken(&text);
				joints[i].orient[0] = (float)atof(MD5_ParseToken(&text));
				joints[i].orient[1] = (float)atof(MD5_ParseToken(&text));
				joints[i].orient[2] = (float)atof(MD5_ParseToken(&text));
				MD5_ParseToken(&text);

				MD5_QuatComputeW(joints[i].orient);
			}
			MD5_ParseToken(&text);
		} else if (!Q_stricmp(token, "mesh")) {
			md5Mesh_t *mesh = &meshes[m = 0];
			int meshIdx = -1;

			for (i = 0; i < numMeshes; i++) {
				if (meshes[i].numVerts == 0) {
					mesh = &meshes[i];
					meshIdx = i;
					break;
				}
			}
			if (meshIdx < 0) { MD5_SkipLine(&text); continue; }

			MD5_ParseToken(&text);

			while (1) {
				token = MD5_ParseToken(&text);
				if (!token[0] || token[0] == '}') break;

				if (!Q_stricmp(token, "shader")) {
					token = MD5_ParseToken(&text);
					Q_strncpyz(mesh->shader, token, sizeof(mesh->shader));
				} else if (!Q_stricmp(token, "numverts")) {
					token = MD5_ParseToken(&text);
					mesh->numVerts = atoi(token);
					if (mesh->numVerts > MD5_MAX_VERTS) mesh->numVerts = MD5_MAX_VERTS;
					mesh->verts = (md5Vertex_t *)ri.Hunk_Alloc(mesh->numVerts * sizeof(md5Vertex_t), h_low);
				} else if (!Q_stricmp(token, "vert")) {
					int idx = atoi(MD5_ParseToken(&text));
					if (idx >= 0 && idx < mesh->numVerts) {
						MD5_ParseToken(&text);
						mesh->verts[idx].st[0] = (float)atof(MD5_ParseToken(&text));
						mesh->verts[idx].st[1] = (float)atof(MD5_ParseToken(&text));
						MD5_ParseToken(&text);
						mesh->verts[idx].firstWeight = atoi(MD5_ParseToken(&text));
						mesh->verts[idx].numWeights = atoi(MD5_ParseToken(&text));
					}
				} else if (!Q_stricmp(token, "numtris")) {
					token = MD5_ParseToken(&text);
					mesh->numTris = atoi(token);
					if (mesh->numTris > MD5_MAX_TRIS) mesh->numTris = MD5_MAX_TRIS;
					mesh->tris = (md5Triangle_t *)ri.Hunk_Alloc(mesh->numTris * sizeof(md5Triangle_t), h_low);
				} else if (!Q_stricmp(token, "tri")) {
					int idx = atoi(MD5_ParseToken(&text));
					if (idx >= 0 && idx < mesh->numTris) {
						mesh->tris[idx].indexes[0] = atoi(MD5_ParseToken(&text));
						mesh->tris[idx].indexes[1] = atoi(MD5_ParseToken(&text));
						mesh->tris[idx].indexes[2] = atoi(MD5_ParseToken(&text));
					}
				} else if (!Q_stricmp(token, "numweights")) {
					token = MD5_ParseToken(&text);
					mesh->numWeights = atoi(token);
					if (mesh->numWeights > MD5_MAX_WEIGHTS) mesh->numWeights = MD5_MAX_WEIGHTS;
					mesh->weights = (md5Weight_t *)ri.Hunk_Alloc(mesh->numWeights * sizeof(md5Weight_t), h_low);
				} else if (!Q_stricmp(token, "weight")) {
					int idx = atoi(MD5_ParseToken(&text));
					if (idx >= 0 && idx < mesh->numWeights) {
						mesh->weights[idx].jointIndex = atoi(MD5_ParseToken(&text));
						mesh->weights[idx].bias = (float)atof(MD5_ParseToken(&text));
						MD5_ParseToken(&text);
						mesh->weights[idx].pos[0] = (float)atof(MD5_ParseToken(&text));
						mesh->weights[idx].pos[1] = (float)atof(MD5_ParseToken(&text));
						mesh->weights[idx].pos[2] = (float)atof(MD5_ParseToken(&text));
						MD5_ParseToken(&text);
					}
				}
			}
		}
	}

	{
		md3Header_t *md3;
		md3Surface_t *surf;
		md3Shader_t *md3Shader;
		md3Triangle_t *md3Tri;
		md3St_t *md3St;
		md3XyzNormal_t *md3Xyz;

		md5Mesh_t *mesh = &meshes[0];
		if (!mesh->numVerts || !mesh->numTris) {
			ri.FS_FreeFile(fileData);
			ri.Printf(PRINT_WARNING, "MD5: %s has no geometry\n", name);
			return 0;
		}

		int totalVerts = mesh->numVerts;
		int totalTris = mesh->numTris;

		int hdrSize = sizeof(md3Header_t) + sizeof(md3Surface_t) +
			sizeof(md3Shader_t) + totalTris * sizeof(md3Triangle_t) +
			totalVerts * sizeof(md3St_t) + totalVerts * sizeof(md3XyzNormal_t);

		md3 = (md3Header_t *)ri.Hunk_Alloc(hdrSize, h_low);
		Com_Memset(md3, 0, hdrSize);

		md3->ident = MD3_IDENT;
		md3->version = MD3_VERSION;
		Q_strncpyz(md3->name, name, sizeof(md3->name));
		md3->numFrames = 1;
		md3->numSurfaces = 1;
		md3->ofsSurfaces = sizeof(md3Header_t);
		md3->ofsEnd = hdrSize;

		surf = (md3Surface_t *)((byte *)md3 + md3->ofsSurfaces);
		surf->ident = MD3_IDENT;
		Q_strncpyz(surf->name, "md5_mesh", sizeof(surf->name));
		surf->numFrames = 1;
		surf->numShaders = 1;
		surf->numVerts = totalVerts;
		surf->numTriangles = totalTris;

		int offset = sizeof(md3Surface_t);
		surf->ofsShaders = offset; offset += sizeof(md3Shader_t);
		surf->ofsTriangles = offset; offset += totalTris * sizeof(md3Triangle_t);
		surf->ofsSt = offset; offset += totalVerts * sizeof(md3St_t);
		surf->ofsXyzNormals = offset; offset += totalVerts * sizeof(md3XyzNormal_t);
		surf->ofsEnd = offset;

		md3Shader = (md3Shader_t *)((byte *)surf + surf->ofsShaders);
		Q_strncpyz(md3Shader->name, mesh->shader[0] ? mesh->shader : "textures/common/white", sizeof(md3Shader->name));

		md3Tri = (md3Triangle_t *)((byte *)surf + surf->ofsTriangles);
		md3St = (md3St_t *)((byte *)surf + surf->ofsSt);
		md3Xyz = (md3XyzNormal_t *)((byte *)surf + surf->ofsXyzNormals);

		for (i = 0; i < totalVerts; i++) {
			vec3_t finalPos;
			VectorClear(finalPos);

			for (j = 0; j < mesh->verts[i].numWeights; j++) {
				int wIdx = mesh->verts[i].firstWeight + j;
				if (wIdx < 0 || wIdx >= mesh->numWeights) continue;
				md5Weight_t *w = &mesh->weights[wIdx];
				md5Joint_t *joint = &joints[w->jointIndex];

				vec3_t wPos;
				VectorCopy(w->pos, wPos);

				float qx = joint->orient[0], qy = joint->orient[1], qz = joint->orient[2], qw = joint->orient[3];
				float ix = qw*wPos[0] + qy*wPos[2] - qz*wPos[1];
				float iy = qw*wPos[1] + qz*wPos[0] - qx*wPos[2];
				float iz = qw*wPos[2] + qx*wPos[1] - qy*wPos[0];
				float iw = -qx*wPos[0] - qy*wPos[1] - qz*wPos[2];

				vec3_t rotated;
				rotated[0] = ix*qw + iw*(-qx) + iy*(-qz) - iz*(-qy);
				rotated[1] = iy*qw + iw*(-qy) + iz*(-qx) - ix*(-qz);
				rotated[2] = iz*qw + iw*(-qz) + ix*(-qy) - iy*(-qx);

				finalPos[0] += (joint->pos[0] + rotated[0]) * w->bias;
				finalPos[1] += (joint->pos[1] + rotated[1]) * w->bias;
				finalPos[2] += (joint->pos[2] + rotated[2]) * w->bias;
			}

			md3Xyz[i].xyz[0] = (short)(finalPos[0] * 64.0f);
			md3Xyz[i].xyz[1] = (short)(finalPos[1] * 64.0f);
			md3Xyz[i].xyz[2] = (short)(finalPos[2] * 64.0f);
			md3Xyz[i].normal = 0;

			md3St[i].st[0] = mesh->verts[i].st[0];
			md3St[i].st[1] = mesh->verts[i].st[1];
		}

		for (i = 0; i < totalTris; i++) {
			md3Tri[i].indexes[0] = mesh->tris[i].indexes[0];
			md3Tri[i].indexes[1] = mesh->tris[i].indexes[1];
			md3Tri[i].indexes[2] = mesh->tris[i].indexes[2];
		}

		mod->type = MOD_MESH;
		mod->md3[0] = md3;
	}

	ri.FS_FreeFile(fileData);
	ri.Printf(PRINT_ALL, "MD5: loaded %s (%d joints, %d meshes)\n", name, numJoints, numMeshes);
	return mod->index;
}
