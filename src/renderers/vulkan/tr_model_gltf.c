/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

glTF 2.0 model loader implementation using cgltf.
Parses glTF/GLB files and converts geometry, materials,
skeleton, and animations into engine-native structures.
===========================================================================
*/

#define CGLTF_IMPLEMENTATION
#include "../../external/include/cgltf/cgltf.h"

#include "tr_local.h"
#include "tr_model_gltf.h"
#include "vk.h"

static qboolean gltf_load_materials(const cgltf_data *data, gltfModel_t *model);
static qboolean gltf_load_meshes(const cgltf_data *data, gltfModel_t *model);
static qboolean gltf_load_skeleton(const cgltf_data *data, gltfModel_t *model);
static qboolean gltf_load_animations(const cgltf_data *data, gltfModel_t *model);

typedef struct {
	gltfModel_t model;
	int numSurfaces;
	image_t *materialNormal[GLTF_MAX_MATERIALS];   /* PBR normal map per material */
	image_t *materialPhysical[GLTF_MAX_MATERIALS]; /* PBR metallic-roughness per material */
	srfGLTFPrimitive_t surfaces[1];
} gltfRenderData_t;

static void gltf_extract_texture_name(const cgltf_texture_view *tv, char *out, int outSize) {
	if (!tv || !tv->texture || !tv->texture->image || !tv->texture->image->uri) {
		out[0] = '\0';
		return;
	}
	Q_strncpyz(out, tv->texture->image->uri, outSize);
}

/*
===============
R_LoadGLTF
===============
*/
qboolean R_LoadGLTF(const char *filename, gltfModel_t *model) {
	cgltf_options options;
	cgltf_data *data = NULL;
	cgltf_result result;
	void *fileData;
	int fileSize;

	Com_Memset(model, 0, sizeof(*model));

	fileSize = ri.FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) {
		ri.Printf(PRINT_WARNING, "glTF: Could not read %s\n", filename);
		return qfalse;
	}

	Com_Memset(&options, 0, sizeof(options));
	result = cgltf_parse(&options, fileData, (cgltf_size)fileSize, &data);
	if (result != cgltf_result_success) {
		ri.Printf(PRINT_WARNING, "glTF: Parse error in %s (code %d)\n", filename, (int)result);
		ri.FS_FreeFile(fileData);
		return qfalse;
	}

	result = cgltf_load_buffers(&options, data, filename);
	if (result != cgltf_result_success) {
		ri.Printf(PRINT_WARNING, "glTF: Buffer load error in %s (code %d)\n", filename, (int)result);
		cgltf_free(data);
		ri.FS_FreeFile(fileData);
		return qfalse;
	}

	result = cgltf_validate(data);
	if (result != cgltf_result_success) {
		ri.Printf(PRINT_WARNING, "glTF: Validation error in %s (code %d)\n", filename, (int)result);
	}

	ri.Printf(PRINT_DEVELOPER, "glTF: Loading %s (%d meshes, %d materials, %d animations)\n",
		filename, (int)data->meshes_count, (int)data->materials_count, (int)data->animations_count);

	gltf_load_materials(data, model);
	gltf_load_meshes(data, model);
	gltf_load_skeleton(data, model);
	gltf_load_animations(data, model);

	/* Compute bounds from mesh vertices */
	{
		int mi, pi, vi;
		qboolean first = qtrue;
		for (mi = 0; mi < model->numMeshes; mi++) {
			gltfMesh_t *mesh = &model->meshes[mi];
			for (pi = 0; pi < mesh->numPrimitives; pi++) {
				gltfPrimitive_t *prim = &mesh->primitives[pi];
				for (vi = 0; vi < prim->numVertices; vi++) {
					float *p = prim->vertices[vi].position;
					if (first) {
						VectorCopy(p, model->boundsMin);
						VectorCopy(p, model->boundsMax);
						first = qfalse;
					} else {
						if (p[0] < model->boundsMin[0]) model->boundsMin[0] = p[0];
						if (p[1] < model->boundsMin[1]) model->boundsMin[1] = p[1];
						if (p[2] < model->boundsMin[2]) model->boundsMin[2] = p[2];
						if (p[0] > model->boundsMax[0]) model->boundsMax[0] = p[0];
						if (p[1] > model->boundsMax[1]) model->boundsMax[1] = p[1];
						if (p[2] > model->boundsMax[2]) model->boundsMax[2] = p[2];
					}
				}
			}
		}
		if (first) {
			VectorSet(model->boundsMin, -1, -1, -1);
			VectorSet(model->boundsMax, 1, 1, 1);
		}
	}

	cgltf_free(data);
	ri.FS_FreeFile(fileData);

	ri.Printf(PRINT_ALL, "glTF: Loaded %s (%d meshes, %d materials, %d joints, %d animations)\n",
		filename, model->numMeshes, model->numMaterials,
		model->skeleton.numJoints, model->numAnimations);

	return qtrue;
}

/*
===============
gltf_load_materials
===============
*/
static qboolean gltf_load_materials(const cgltf_data *data, gltfModel_t *model) {
	int i;
	int count = (int)data->materials_count;

	if (count > GLTF_MAX_MATERIALS) {
		count = GLTF_MAX_MATERIALS;
	}

	for (i = 0; i < count; i++) {
		const cgltf_material *src = &data->materials[i];
		gltfMaterial_t *dst = &model->materials[i];

		if (src->name) {
			Q_strncpyz(dst->name, src->name, sizeof(dst->name));
		}

		if (src->has_pbr_metallic_roughness) {
			const cgltf_pbr_metallic_roughness *pbr = &src->pbr_metallic_roughness;
			dst->baseColorFactor[0] = pbr->base_color_factor[0];
			dst->baseColorFactor[1] = pbr->base_color_factor[1];
			dst->baseColorFactor[2] = pbr->base_color_factor[2];
			dst->baseColorFactor[3] = pbr->base_color_factor[3];
			dst->metallicFactor = pbr->metallic_factor;
			dst->roughnessFactor = pbr->roughness_factor;

			gltf_extract_texture_name(&pbr->base_color_texture, dst->baseColorTexture, sizeof(dst->baseColorTexture));
			gltf_extract_texture_name(&pbr->metallic_roughness_texture, dst->metallicRoughnessTexture, sizeof(dst->metallicRoughnessTexture));
		} else {
			Vector4Set(dst->baseColorFactor, 1.0f, 1.0f, 1.0f, 1.0f);
			dst->metallicFactor = 1.0f;
			dst->roughnessFactor = 1.0f;
		}

		dst->emissiveFactor[0] = src->emissive_factor[0];
		dst->emissiveFactor[1] = src->emissive_factor[1];
		dst->emissiveFactor[2] = src->emissive_factor[2];
		dst->emissiveStrength = src->has_emissive_strength ? src->emissive_strength.emissive_strength : 1.0f;

		gltf_extract_texture_name(&src->normal_texture, dst->normalTexture, sizeof(dst->normalTexture));
		gltf_extract_texture_name(&src->emissive_texture, dst->emissiveTexture, sizeof(dst->emissiveTexture));
		gltf_extract_texture_name(&src->occlusion_texture, dst->occlusionTexture, sizeof(dst->occlusionTexture));

		dst->alphaCutoff = src->alpha_cutoff;
		dst->alphaMode = (int)src->alpha_mode;
		dst->doubleSided = src->double_sided ? qtrue : qfalse;

		if (src->has_clearcoat) {
			dst->clearcoatFactor = src->clearcoat.clearcoat_factor;
		}
		if (src->has_sheen) {
			dst->sheenColorFactor[0] = src->sheen.sheen_color_factor[0];
			dst->sheenColorFactor[1] = src->sheen.sheen_color_factor[1];
			dst->sheenColorFactor[2] = src->sheen.sheen_color_factor[2];
			dst->sheenRoughnessFactor = src->sheen.sheen_roughness_factor;
		}
		if (src->has_transmission) {
			dst->transmissionFactor = src->transmission.transmission_factor;
		}
		if (src->has_ior) {
			dst->ior = src->ior.ior;
		} else {
			dst->ior = 1.5f;
		}
	}

	model->numMaterials = count;
	return qtrue;
}

/*
===============
gltf_load_meshes
===============
*/
static qboolean gltf_load_meshes(const cgltf_data *data, gltfModel_t *model) {
	int mi;
	int meshCount = (int)data->meshes_count;

	if (meshCount > GLTF_MAX_MESHES) {
		meshCount = GLTF_MAX_MESHES;
	}

	for (mi = 0; mi < meshCount; mi++) {
		const cgltf_mesh *srcMesh = &data->meshes[mi];
		gltfMesh_t *dstMesh = &model->meshes[mi];
		int pi;
		int primCount = (int)srcMesh->primitives_count;

		if (srcMesh->name) {
			Q_strncpyz(dstMesh->name, srcMesh->name, sizeof(dstMesh->name));
		}

		dstMesh->primitives = (gltfPrimitive_t *)ri.Hunk_Alloc(primCount * sizeof(gltfPrimitive_t), h_low);
		dstMesh->numPrimitives = primCount;

		for (pi = 0; pi < primCount; pi++) {
			const cgltf_primitive *srcPrim = &srcMesh->primitives[pi];
			gltfPrimitive_t *dstPrim = &dstMesh->primitives[pi];
			int ai, vi;

			if (srcPrim->material) {
				dstPrim->materialIndex = (int)(srcPrim->material - data->materials);
			} else {
				dstPrim->materialIndex = -1;
			}

			int vertCount = 0;
			for (ai = 0; ai < (int)srcPrim->attributes_count; ai++) {
				if (srcPrim->attributes[ai].type == cgltf_attribute_type_position) {
					vertCount = (int)srcPrim->attributes[ai].data->count;
					break;
				}
			}

			dstPrim->numVertices = vertCount;
			dstPrim->vertices = (gltfVertex_t *)ri.Hunk_Alloc(vertCount * sizeof(gltfVertex_t), h_low);
			Com_Memset(dstPrim->vertices, 0, vertCount * sizeof(gltfVertex_t));

			for (ai = 0; ai < (int)srcPrim->attributes_count; ai++) {
				const cgltf_attribute *attr = &srcPrim->attributes[ai];
				const cgltf_accessor *acc = attr->data;

				for (vi = 0; vi < vertCount && vi < (int)acc->count; vi++) {
					float tmp[4] = {0};
					cgltf_accessor_read_float(acc, vi, tmp, 4);

					switch (attr->type) {
						case cgltf_attribute_type_position:
							VectorCopy(tmp, dstPrim->vertices[vi].position);
							break;
						case cgltf_attribute_type_normal:
							VectorCopy(tmp, dstPrim->vertices[vi].normal);
							break;
						case cgltf_attribute_type_tangent:
							Vector4Copy(tmp, dstPrim->vertices[vi].tangent);
							break;
						case cgltf_attribute_type_texcoord:
							if (attr->index == 0) {
								dstPrim->vertices[vi].texCoord0[0] = tmp[0];
								dstPrim->vertices[vi].texCoord0[1] = tmp[1];
							} else {
								dstPrim->vertices[vi].texCoord1[0] = tmp[0];
								dstPrim->vertices[vi].texCoord1[1] = tmp[1];
							}
							break;
						case cgltf_attribute_type_color:
							Vector4Copy(tmp, dstPrim->vertices[vi].color);
							break;
						case cgltf_attribute_type_joints:
							dstPrim->vertices[vi].joints[0] = (byte)tmp[0];
							dstPrim->vertices[vi].joints[1] = (byte)tmp[1];
							dstPrim->vertices[vi].joints[2] = (byte)tmp[2];
							dstPrim->vertices[vi].joints[3] = (byte)tmp[3];
							break;
						case cgltf_attribute_type_weights:
							Vector4Copy(tmp, dstPrim->vertices[vi].weights);
							break;
						default:
							break;
					}
				}
			}

			if (srcPrim->indices) {
				int idxCount = (int)srcPrim->indices->count;
				dstPrim->numIndices = idxCount;
				dstPrim->indices = (uint32_t *)ri.Hunk_Alloc(idxCount * sizeof(uint32_t), h_low);
				for (vi = 0; vi < idxCount; vi++) {
					dstPrim->indices[vi] = (uint32_t)cgltf_accessor_read_index(srcPrim->indices, vi);
				}
			}

			/* Morph targets (blend shapes) */
			dstPrim->numMorphTargets = 0;
			dstPrim->morphTargets = NULL;
			if (srcPrim->targets_count > 0 && srcPrim->targets_count <= GLTF_MAX_MORPH_TARGETS) {
				int ti, attri;
				dstPrim->numMorphTargets = (int)srcPrim->targets_count;
				dstPrim->morphTargets = (gltfMorphTarget_t *)ri.Hunk_Alloc(
					dstPrim->numMorphTargets * sizeof(gltfMorphTarget_t), h_low);
				Com_Memset(dstPrim->morphTargets, 0, dstPrim->numMorphTargets * sizeof(gltfMorphTarget_t));
				for (ti = 0; ti < dstPrim->numMorphTargets; ti++) {
					const cgltf_morph_target *tgt = &srcPrim->targets[ti];
					gltfMorphTarget_t *dst = &dstPrim->morphTargets[ti];
					for (attri = 0; attri < (int)tgt->attributes_count; attri++) {
						const cgltf_attribute *attr = &tgt->attributes[attri];
						const cgltf_accessor *acc = attr->data;
						float *dstArr = NULL;
						if (attr->name && !Q_stricmp(attr->name, "POSITION")) {
							dstArr = (float *)ri.Hunk_Alloc(vertCount * 3 * sizeof(float), h_low);
							dst->deltaPosition = dstArr;
						} else if (attr->name && !Q_stricmp(attr->name, "NORMAL")) {
							dstArr = (float *)ri.Hunk_Alloc(vertCount * 3 * sizeof(float), h_low);
							dst->deltaNormal = dstArr;
						} else if (attr->name && !Q_stricmp(attr->name, "TANGENT")) {
							dstArr = (float *)ri.Hunk_Alloc(vertCount * 3 * sizeof(float), h_low);
							dst->deltaTangent = dstArr;
						}
						if (dstArr) {
							for (vi = 0; vi < vertCount && vi < (int)acc->count; vi++) {
								float tmp[4] = {0};
								cgltf_accessor_read_float(acc, vi, tmp, 4);
								dstArr[vi * 3 + 0] = tmp[0];
								dstArr[vi * 3 + 1] = tmp[1];
								dstArr[vi * 3 + 2] = tmp[2];
							}
						}
					}
				}
			}
		}
	}

	model->numMeshes = meshCount;
	return qtrue;
}

/*
===============
gltf_load_skeleton
===============
*/
static qboolean gltf_load_skeleton(const cgltf_data *data, gltfModel_t *model) {
	int si, ji;

	if (data->skins_count == 0) {
		return qtrue;
	}

	const cgltf_skin *skin = &data->skins[0];
	int jointCount = (int)skin->joints_count;

	if (jointCount > GLTF_MAX_JOINTS) {
		jointCount = GLTF_MAX_JOINTS;
	}

	for (ji = 0; ji < jointCount; ji++) {
		const cgltf_node *node = skin->joints[ji];
		gltfJoint_t *joint = &model->skeleton.joints[ji];

		if (node->name) {
			Q_strncpyz(joint->name, node->name, sizeof(joint->name));
		}

		joint->parent = -1;
		if (node->parent) {
			for (si = 0; si < jointCount; si++) {
				if (skin->joints[si] == node->parent) {
					joint->parent = si;
					break;
				}
			}
		}

		if (node->has_translation) {
			VectorCopy(node->translation, joint->translation);
		}
		if (node->has_rotation) {
			Vector4Copy(node->rotation, joint->rotation);
		} else {
			Vector4Set(joint->rotation, 0, 0, 0, 1);
		}
		if (node->has_scale) {
			VectorCopy(node->scale, joint->scale);
		} else {
			VectorSet(joint->scale, 1, 1, 1);
		}

		if (skin->inverse_bind_matrices) {
			float ibm[16];
			cgltf_accessor_read_float(skin->inverse_bind_matrices, ji, ibm, 16);
			Com_Memcpy(joint->inverseBindMatrix, ibm, sizeof(ibm));
		} else {
			/* Identity when not specified */
			Com_Memset(joint->inverseBindMatrix, 0, sizeof(joint->inverseBindMatrix));
			joint->inverseBindMatrix[0] = joint->inverseBindMatrix[5] = joint->inverseBindMatrix[10] = joint->inverseBindMatrix[15] = 1.0f;
		}
	}

	model->skeleton.numJoints = jointCount;
	return qtrue;
}

/*
===============
gltf_load_animations
===============
*/
static qboolean gltf_load_animations(const cgltf_data *data, gltfModel_t *model) {
	int ai, ci;
	int animCount = (int)data->animations_count;

	if (animCount == 0) {
		return qtrue;
	}

	model->animations = (gltfAnimation_t *)ri.Hunk_Alloc(animCount * sizeof(gltfAnimation_t), h_low);

	for (ai = 0; ai < animCount; ai++) {
		const cgltf_animation *srcAnim = &data->animations[ai];
		gltfAnimation_t *dstAnim = &model->animations[ai];
		int channelCount = (int)srcAnim->channels_count;

		if (srcAnim->name) {
			Q_strncpyz(dstAnim->name, srcAnim->name, sizeof(dstAnim->name));
		}

		dstAnim->channels = (gltfAnimChannel_t *)ri.Hunk_Alloc(channelCount * sizeof(gltfAnimChannel_t), h_low);
		dstAnim->numChannels = channelCount;
		dstAnim->duration = 0;

		for (ci = 0; ci < channelCount; ci++) {
			const cgltf_animation_channel *srcCh = &srcAnim->channels[ci];
			gltfAnimChannel_t *dstCh = &dstAnim->channels[ci];
			const cgltf_animation_sampler *sampler = srcCh->sampler;
			int ki;

			dstCh->jointIndex = -1;
			if (srcCh->target_node && data->skins_count > 0) {
				const cgltf_skin *skin = &data->skins[0];
				int ji;
				for (ji = 0; ji < (int)skin->joints_count; ji++) {
					if (skin->joints[ji] == srcCh->target_node) {
						dstCh->jointIndex = ji;
						break;
					}
				}
			}

			dstCh->type = (int)srcCh->target_path;
			dstCh->numKeyframes = (int)sampler->input->count;

			dstCh->times = (float *)ri.Hunk_Alloc(dstCh->numKeyframes * sizeof(float), h_low);
			for (ki = 0; ki < dstCh->numKeyframes; ki++) {
				cgltf_accessor_read_float(sampler->input, ki, &dstCh->times[ki], 1);
				if (dstCh->times[ki] > dstAnim->duration) {
					dstAnim->duration = dstCh->times[ki];
				}
			}

			int compCount = (srcCh->target_path == cgltf_animation_path_type_rotation) ? 4 : 3;
			dstCh->values = (float *)ri.Hunk_Alloc(dstCh->numKeyframes * compCount * sizeof(float), h_low);
			for (ki = 0; ki < dstCh->numKeyframes; ki++) {
				cgltf_accessor_read_float(sampler->output, ki, &dstCh->values[ki * compCount], compCount);
			}
		}
	}

	model->numAnimations = animCount;
	return qtrue;
}

/*
===============
gltf_joint_to_matrix
===============
Build 3x4 matrix from TRS (glTF joint local transform).
*/
static void gltf_joint_to_matrix(const vec3_t trans, const vec4_t rot, const vec3_t scale, float *mat) {
	float xx = 2.0f * rot[0] * rot[0];
	float yy = 2.0f * rot[1] * rot[1];
	float zz = 2.0f * rot[2] * rot[2];
	float xy = 2.0f * rot[0] * rot[1];
	float xz = 2.0f * rot[0] * rot[2];
	float yz = 2.0f * rot[1] * rot[2];
	float wx = 2.0f * rot[3] * rot[0];
	float wy = 2.0f * rot[3] * rot[1];
	float wz = 2.0f * rot[3] * rot[2];
	mat[0] = scale[0] * (1.0f - (yy + zz));
	mat[1] = scale[0] * (xy - wz);
	mat[2] = scale[0] * (xz + wy);
	mat[3] = trans[0];
	mat[4] = scale[1] * (xy + wz);
	mat[5] = scale[1] * (1.0f - (xx + zz));
	mat[6] = scale[1] * (yz - wx);
	mat[7] = trans[1];
	mat[8] = scale[2] * (xz - wy);
	mat[9] = scale[2] * (yz + wx);
	mat[10] = scale[2] * (1.0f - (xx + yy));
	mat[11] = trans[2];
}

/* Multiply 3x4 * 4x4 -> 3x4. a34 row-major, b44 column-major (glTF). */
static void gltf_mat34_mul_44(const float *a34, const float *b44, float *out34) {
	out34[0] = a34[0]*b44[0] + a34[1]*b44[1] + a34[2]*b44[2] + a34[3]*b44[3];
	out34[1] = a34[0]*b44[4] + a34[1]*b44[5] + a34[2]*b44[6] + a34[3]*b44[7];
	out34[2] = a34[0]*b44[8] + a34[1]*b44[9] + a34[2]*b44[10] + a34[3]*b44[11];
	out34[3] = a34[0]*b44[12] + a34[1]*b44[13] + a34[2]*b44[14] + a34[3]*b44[15];
	out34[4] = a34[4]*b44[0] + a34[5]*b44[1] + a34[6]*b44[2] + a34[7]*b44[3];
	out34[5] = a34[4]*b44[4] + a34[5]*b44[5] + a34[6]*b44[6] + a34[7]*b44[7];
	out34[6] = a34[4]*b44[8] + a34[5]*b44[9] + a34[6]*b44[10] + a34[7]*b44[11];
	out34[7] = a34[4]*b44[12] + a34[5]*b44[13] + a34[6]*b44[14] + a34[7]*b44[15];
	out34[8] = a34[8]*b44[0] + a34[9]*b44[1] + a34[10]*b44[2] + a34[11]*b44[3];
	out34[9] = a34[8]*b44[4] + a34[9]*b44[5] + a34[10]*b44[6] + a34[11]*b44[7];
	out34[10] = a34[8]*b44[8] + a34[9]*b44[9] + a34[10]*b44[10] + a34[11]*b44[11];
	out34[11] = a34[8]*b44[12] + a34[9]*b44[13] + a34[10]*b44[14] + a34[11]*b44[15];
}

/* Compute bind-pose joint matrices: skinMat[j] = world[j] * inverseBindMatrix[j], 3x4 per joint */
void R_ComputeGLTFJointMatrices(const gltfModel_t *model, float *outMatrices) {
	float world[GLTF_MAX_JOINTS][12];
	int ji;
	for (ji = 0; ji < model->skeleton.numJoints; ji++) {
		const gltfJoint_t *j = &model->skeleton.joints[ji];
		float local[12];
		float local44[16];
		gltf_joint_to_matrix(j->translation, j->rotation, j->scale, local);
		local44[0] = local[0]; local44[1] = local[1]; local44[2] = local[2]; local44[3] = local[3];
		local44[4] = local[4]; local44[5] = local[5]; local44[6] = local[6]; local44[7] = local[7];
		local44[8] = local[8]; local44[9] = local[9]; local44[10] = local[10]; local44[11] = local[11];
		local44[12] = 0; local44[13] = 0; local44[14] = 0; local44[15] = 1;
		if (j->parent < 0) {
			Com_Memcpy(world[ji], local, sizeof(local));
		} else {
			gltf_mat34_mul_44(world[j->parent], local44, world[ji]);
		}
	}
	for (ji = 0; ji < model->skeleton.numJoints; ji++) {
		gltf_mat34_mul_44(world[ji], model->skeleton.joints[ji].inverseBindMatrix, &outMatrices[ji * 12]);
	}
}

static int R_ComputeGLTFFogNum(const gltfModel_t *model, const trRefEntity_t *ent) {
	int i, j;
	const fog_t *fog;
	vec3_t localOrigin;
	float radius;

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}
	if (!tr.world || !tr.world->numfogs) {
		return 0;
	}

	VectorCopy(ent->e.origin, localOrigin);
	radius = 0;
	if (model->boundsMin[0] != model->boundsMax[0] || model->boundsMin[1] != model->boundsMax[1] ||
	    model->boundsMin[2] != model->boundsMax[2]) {
		vec3_t diff;
		VectorSubtract(model->boundsMax, model->boundsMin, diff);
		radius = VectorLength(diff) * 0.5f;
	}

	for (i = 1; i < tr.world->numfogs; i++) {
		fog = &tr.world->fogs[i];
		for (j = 0; j < 3; j++) {
			if (localOrigin[j] - radius >= fog->bounds[1][j]) break;
			if (localOrigin[j] + radius <= fog->bounds[0][j]) break;
		}
		if (j == 3) return i;
	}
	return 0;
}

/*
===============
R_AddGLTFSurfaces
===============
*/
void R_AddGLTFSurfaces(trRefEntity_t *ent) {
	gltfRenderData_t *rdata;
	srfGLTFPrimitive_t *surf;
	shader_t *shader;
	int i, fogNum;

	rdata = (gltfRenderData_t *)tr.currentModel->modelData;
	if (!rdata || tr.currentModel->type != MOD_GLTF) {
		return;
	}

	fogNum = R_ComputeGLTFFogNum(&rdata->model, ent);
	R_SetupEntityLighting(&tr.refdef, ent);

	for (i = 0; i < rdata->numSurfaces; i++) {
		surf = &rdata->surfaces[i];
		if (surf->numVertices == 0 || surf->numIndices == 0) {
			continue;
		}
		shader = ent->e.customShader
			? R_GetShaderByHandle(ent->e.customShader)
			: surf->shader;
		R_AddDrawSurf((void *)surf, shader, fogNum, 0);
	}
}

void R_GLTFModelBounds(const void *modelData, vec3_t mins, vec3_t maxs) {
	const gltfModel_t *model = R_GetGLTFModelFromModelData(modelData);
	if (model) {
		VectorCopy(model->boundsMin, mins);
		VectorCopy(model->boundsMax, maxs);
	} else {
		VectorClear(mins);
		VectorClear(maxs);
	}
}

const gltfModel_t *R_GetGLTFModelFromModelData(const void *modelData) {
	const gltfRenderData_t *rdata = (const gltfRenderData_t *)modelData;
	return rdata ? &rdata->model : NULL;
}

/*
===============
R_FreeGLTF
===============
*/
void R_FreeGLTF(gltfModel_t *model) {
	Com_Memset(model, 0, sizeof(*model));
}

/*
===============
R_RegisterGLTF
===============
*/
qboolean R_RegisterGLTF(const char *name, model_t *mod) {
	gltfModel_t gltfModel;
	gltfRenderData_t *rdata;
	int numSurfaces = 0;
	int i, j, idx;

	if (!R_LoadGLTF(name, &gltfModel)) {
		return qfalse;
	}

	for (i = 0; i < gltfModel.numMeshes; i++) {
		numSurfaces += gltfModel.meshes[i].numPrimitives;
	}

	rdata = (gltfRenderData_t *)ri.Hunk_Alloc(
		sizeof(gltfRenderData_t) + (numSurfaces - 1) * sizeof(srfGLTFPrimitive_t), h_low);
	Com_Memcpy(&rdata->model, &gltfModel, sizeof(gltfModel_t));
	rdata->numSurfaces = numSurfaces;
	Com_Memset(rdata->materialNormal, 0, sizeof(rdata->materialNormal));
	Com_Memset(rdata->materialPhysical, 0, sizeof(rdata->materialPhysical));

	/* Load PBR textures (normal, metallic-roughness) for each material */
	for (i = 0; i < gltfModel.numMaterials; i++) {
		gltfMaterial_t *mat = &rdata->model.materials[i];
		if (mat->normalTexture[0]) {
			rdata->materialNormal[i] = R_FindImageFile(mat->normalTexture, IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE, 0);
		}
		if (mat->metallicRoughnessTexture[0]) {
			rdata->materialPhysical[i] = R_FindImageFile(mat->metallicRoughnessTexture, IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE, 0);
		}
	}

	idx = 0;
	for (i = 0; i < gltfModel.numMeshes; i++) {
		gltfMesh_t *mesh = &rdata->model.meshes[i];
		for (j = 0; j < mesh->numPrimitives; j++) {
			gltfPrimitive_t *prim = &mesh->primitives[j];
			srfGLTFPrimitive_t *surf = &rdata->surfaces[idx++];
			int vi;
			byte *vboPack;
			int vboSize;

			surf->surfaceType = SF_GLTF;
			surf->vertices = prim->vertices;
			surf->numVertices = prim->numVertices;
			surf->indices = prim->indices;
			surf->numIndices = prim->numIndices;
			surf->shader = tr.defaultShader;
			surf->materialIndex = prim->materialIndex;
			surf->hasSkinning = (gltfModel.skeleton.numJoints > 0) ? qtrue : qfalse;
			surf->hasMorphTargets = (prim->numMorphTargets > 0) ? qtrue : qfalse;
			surf->vbo_vertex = VK_NULL_HANDLE;
			surf->vbo_index = VK_NULL_HANDLE;
			Com_Memset(surf->vbo_vertex_offsets, 0, sizeof(surf->vbo_vertex_offsets));

			if (prim->materialIndex >= 0 && prim->materialIndex < gltfModel.numMaterials) {
				gltfMaterial_t *mat = &rdata->model.materials[prim->materialIndex];
				if (mat->baseColorTexture[0]) {
					qhandle_t h = RE_RegisterShaderNoMip(mat->baseColorTexture);
					surf->shader = R_GetShaderByHandle(h);
				}
			}

			/* VBO: pack vertex data as separate arrays (xyz, rgba, st, normal) to match engine layout */
			if (prim->numVertices > 0 && prim->numIndices > 0 && !surf->hasSkinning && !surf->hasMorphTargets) {
				int offXyz, offRgba, offSt, offNorm;
				vboSize = prim->numVertices * 16 + prim->numVertices * 4 + prim->numVertices * 8 + prim->numVertices * 16;
				vboPack = (byte *)ri.Hunk_AllocateTempMemory(vboSize);
				offXyz = 0;
				offRgba = prim->numVertices * 16;
				offSt = offRgba + prim->numVertices * 4;
				offNorm = offSt + prim->numVertices * 8;
				for (vi = 0; vi < prim->numVertices; vi++) {
					const gltfVertex_t *v = &prim->vertices[vi];
					float *f;
					byte *b;
					f = (float *)(vboPack + offXyz + vi * 16);
					f[0] = v->position[0]; f[1] = v->position[1]; f[2] = v->position[2]; f[3] = 0.0f;
					b = vboPack + offRgba + vi * 4;
					b[0] = (byte)(v->color[0] * 255); b[1] = (byte)(v->color[1] * 255);
					b[2] = (byte)(v->color[2] * 255); b[3] = (byte)(v->color[3] * 255);
					f = (float *)(vboPack + offSt + vi * 8);
					f[0] = v->texCoord0[0]; f[1] = v->texCoord0[1];
					f = (float *)(vboPack + offNorm + vi * 16);
					f[0] = v->normal[0]; f[1] = v->normal[1]; f[2] = v->normal[2]; f[3] = 0.0f;
				}
				if (vk_create_gltf_buffers(vboPack, vboSize, prim->indices, prim->numIndices,
					&surf->vbo_vertex, &surf->vbo_index)) {
					surf->vbo_vertex_offsets[0] = offXyz;
					surf->vbo_vertex_offsets[1] = offRgba;
					surf->vbo_vertex_offsets[2] = offSt;
					surf->vbo_vertex_offsets[5] = offNorm;
				}
				ri.Hunk_FreeTempMemory(vboPack);
			}
		}
	}

	mod->type = MOD_GLTF;
	mod->modelData = rdata;
	mod->dataSize = sizeof(gltfRenderData_t) + (numSurfaces - 1) * sizeof(srfGLTFPrimitive_t);
	ri.Printf(PRINT_ALL, "glTF: Registered model %s (%d meshes, %d surfaces)\n",
		name, rdata->model.numMeshes, numSurfaces);

	return qtrue;
}
