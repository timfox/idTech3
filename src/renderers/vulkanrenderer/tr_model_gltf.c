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

static qboolean gltf_load_materials(const cgltf_data *data, gltfModel_t *model);
static qboolean gltf_load_meshes(const cgltf_data *data, gltfModel_t *model);
static qboolean gltf_load_skeleton(const cgltf_data *data, gltfModel_t *model);
static qboolean gltf_load_animations(const cgltf_data *data, gltfModel_t *model);

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

	if (!R_LoadGLTF(name, &gltfModel)) {
		return qfalse;
	}

	mod->type = MOD_MESH;
	ri.Printf(PRINT_ALL, "glTF: Registered model %s (%d meshes, %d joints)\n",
		name, gltfModel.numMeshes, gltfModel.skeleton.numJoints);

	return qtrue;
}
