#pragma once

#include "tr_local.h"

/* Shared binary loaders for experimental neural renderer modules. */

#define VK_NEURAL_MAGIC_NIV1    0x3156494Eu /* NIV1 — RGB MLP weights */
#define VK_NEURAL_MAGIC_NDG1    0x3147444Eu /* NDG1 */
#define VK_NEURAL_MAGIC_NIS1    0x314E4953u /* NIS1 — scalar MLP */
#define VK_NEURAL_MAGIC_NVC1    0x3156434Eu /* NVC1 */
#define VK_NEURAL_MAGIC_NSL1    0x314C534Eu /* NSL1 */
#define VK_NEURAL_MAGIC_VFG1    0x31474656u /* VFG1 */
#define VK_NEURAL_MAGIC_NIV2    0x3256494Eu /* NIV2 — float feature volume */
#define VK_NEURAL_MAGIC_NSL2    0x324C534Eu /* NSL2 */

qboolean vk_neural_load_mlp_rgb( uint32_t magic, const char *path, int featureDim, int hiddenDim,
	float *W1, int w1_stride, float *b1, float *W2, float *b2, int max_feature, int max_hidden );

qboolean vk_neural_load_mlp_scalar( uint32_t magic, const char *path, int featureDim, int hiddenDim,
	float *W1, int w1_stride, float *b1, float *W2, float *b2, int max_feature, int max_hidden );

qboolean vk_neural_load_volume_f32( uint32_t magic, const char *path,
	int *gridX, int *gridY, int *gridZ, int *featureDim, float *out, size_t out_floats );
