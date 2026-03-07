/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

HDR skybox implementation.
Loads EXR/HDR panoramas, converts to cubemaps, generates
irradiance and prefiltered mip chains for PBR IBL lighting.
===========================================================================
*/

#include "tr_local.h"
#include "vk_skybox_hdr.h"
#include <math.h>

static skyboxHDR_t skybox;
static image_t skyboxPrefilteredImage;
static image_t skyboxIrradianceImage;
static qboolean skyboxLoadFailed;
static cvar_t *r_skyboxHDR;
static cvar_t *r_skyboxHDR_exposure;
static cvar_t *r_skyboxHDR_rotation;
static cvar_t *r_skyboxHDR_intensity;

extern void R_LoadEXR_HDR(const char *filename, float **pic, int *width, int *height);
extern void R_LoadHDR_Float(const char *filename, float **pic, int *width, int *height);

#define PI_F 3.14159265358979323846f
#define SH_C0 0.28209479177387814347f
#define SH_C1 0.48860251190291992159f
#define SH_C2 1.09254843059207907054f
#define SH_C3 0.31539156525252000603f
#define SH_C4 0.54627421529603953527f

static char skyboxPrefilteredName[] = "*skyboxHDRPrefiltered";
static char skyboxIrradianceName[] = "*skyboxHDRIrradiance";

static float SkyboxHDR_SHBasis( int index, const vec3_t dir ) {
	const float x = dir[0];
	const float y = dir[1];
	const float z = dir[2];

	switch ( index ) {
		case 0: return SH_C0;
		case 1: return SH_C1 * y;
		case 2: return SH_C1 * z;
		case 3: return SH_C1 * x;
		case 4: return SH_C2 * x * y;
		case 5: return SH_C2 * y * z;
		case 6: return SH_C3 * ( 3.0f * z * z - 1.0f );
		case 7: return SH_C2 * x * z;
		case 8: return SH_C4 * ( x * x - y * y );
		default: return 0.0f;
	}
}

static void SkyboxHDR_ResetSH( void ) {
	int i;

	for ( i = 0; i < 9; i++ ) {
		Vector4Set( skybox.shCoeffs[i], 0.0f, 0.0f, 0.0f, 0.0f );
	}
	skybox.hasSHCoeffs = qfalse;
}

static qboolean SkyboxHDR_LoadPanoramaImage( const char *filename, float **pic, int *width, int *height ) {
	const char *ext = COM_GetExtension( filename );

	*pic = NULL;
	*width = 0;
	*height = 0;

	if ( ext && !Q_stricmp( ext, "hdr" ) ) {
		R_LoadHDR_Float( filename, pic, width, height );
	} else {
		R_LoadEXR_HDR( filename, pic, width, height );
	}

	return ( *pic != NULL && *width > 0 && *height > 0 );
}

static void SkyboxHDR_DestroyGPUImages( void ) {
	vk_destroy_image_resources( &skyboxPrefilteredImage.handle, &skyboxPrefilteredImage.view );
	vk_destroy_image_resources( &skyboxIrradianceImage.handle, &skyboxIrradianceImage.view );
}

static void SkyboxHDR_InitGPUImage( image_t *image, char *name, int size, int mipLevels ) {
	if ( !image ) {
		return;
	}

	if ( image->imgName == NULL ) {
		Com_Memset( image, 0, sizeof( *image ) );
		image->imgName = name;
		image->imgName2 = name;
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		image->type = 0;
	}

	image->flags = IMGFLAG_CUBEMAP | IMGFLAG_CLAMPTOEDGE;
	if ( mipLevels > 1 ) {
		image->flags |= IMGFLAG_MIPMAP;
	}
	image->width = size;
	image->height = size;
	image->uploadWidth = size;
	image->uploadHeight = size;
	image->layers = 6;
	image->internalFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

	vk_create_image( image, size, size, mipLevels );
}

static void SkyboxHDR_ResampleFace( const float *src, int srcSize, float *dst, int dstSize ) {
	int y;
	int x;

	for ( y = 0; y < dstSize; y++ ) {
		for ( x = 0; x < dstSize; x++ ) {
			const int srcX = ( x * srcSize ) / dstSize;
			const int srcY = ( y * srcSize ) / dstSize;
			const int srcIdx = ( srcY * srcSize + srcX ) * 4;
			const int dstIdx = ( y * dstSize + x ) * 4;

			dst[dstIdx + 0] = src[srcIdx + 0];
			dst[dstIdx + 1] = src[srcIdx + 1];
			dst[dstIdx + 2] = src[srcIdx + 2];
			dst[dstIdx + 3] = src[srcIdx + 3];
		}
	}
}

static byte *SkyboxHDR_BuildUploadBuffer( float *faces[6], int baseSize, int mipLevels, int *outSize ) {
	byte *buffer;
	byte *dst;
	int totalSize = 0;
	int mip;
	int mipSize = baseSize;

	for ( mip = 0; mip < mipLevels; mip++ ) {
		totalSize += 6 * mipSize * mipSize * 4 * (int)sizeof( float );
		mipSize >>= 1;
		if ( mipSize < 1 ) {
			mipSize = 1;
		}
	}

	buffer = (byte *)ri.Hunk_AllocateTempMemory( totalSize );
	dst = buffer;
	mipSize = baseSize;

	for ( mip = 0; mip < mipLevels; mip++ ) {
		int face;

		for ( face = 0; face < 6; face++ ) {
			const int bytes = mipSize * mipSize * 4 * (int)sizeof( float );
			float *dstFace = (float *)dst;

			if ( !faces[face] ) {
				Com_Memset( dstFace, 0, bytes );
			} else if ( mip == 0 ) {
				Com_Memcpy( dstFace, faces[face], bytes );
			} else {
				SkyboxHDR_ResampleFace( faces[face], baseSize, dstFace, mipSize );
			}

			dst += bytes;
		}

		mipSize >>= 1;
		if ( mipSize < 1 ) {
			mipSize = 1;
		}
	}

	*outSize = totalSize;
	return buffer;
}

static void SkyboxHDR_SampleEquirect(const float *data, int w, int h, const float *dir, float *outRGB) {
	float theta = atan2f(dir[0], dir[2]);
	float phi = asinf(dir[1] < -1.0f ? -1.0f : (dir[1] > 1.0f ? 1.0f : dir[1]));

	float rotRad = skybox.rotation * PI_F / 180.0f;
	theta += rotRad;

	float u = (theta / (2.0f * PI_F)) + 0.5f;
	float v = (phi / PI_F) + 0.5f;

	u = u - floorf(u);
	v = v < 0 ? 0 : (v > 1 ? 1 : v);

	int px = (int)(u * (w - 1) + 0.5f);
	int py = (int)((1.0f - v) * (h - 1) + 0.5f);
	if (px < 0) px = 0;
	if (px >= w) px = w - 1;
	if (py < 0) py = 0;
	if (py >= h) py = h - 1;

	int idx = (py * w + px) * 4;
	outRGB[0] = data[idx + 0] * skybox.exposure * skybox.tintR;
	outRGB[1] = data[idx + 1] * skybox.exposure * skybox.tintG;
	outRGB[2] = data[idx + 2] * skybox.exposure * skybox.tintB;
}

static void SkyboxHDR_DirFromCubeFace(int face, float u, float v, float *dir) {
	float uc = 2.0f * u - 1.0f;
	float vc = 2.0f * v - 1.0f;

	switch (face) {
		case 0: dir[0]= 1; dir[1]=vc; dir[2]=-uc; break;
		case 1: dir[0]=-1; dir[1]=vc; dir[2]= uc; break;
		case 2: dir[0]=uc; dir[1]= 1; dir[2]=-vc; break;
		case 3: dir[0]=uc; dir[1]=-1; dir[2]= vc; break;
		case 4: dir[0]=uc; dir[1]=vc; dir[2]= 1; break;
		case 5: dir[0]=-uc;dir[1]=vc; dir[2]=-1; break;
	}

	float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
	if (len > 0) { dir[0] /= len; dir[1] /= len; dir[2] /= len; }
}

static void SkyboxHDR_ExtractSHCoeffs( void ) {
	float totalWeight = 0.0f;
	int face;
	int y;
	int x;

	SkyboxHDR_ResetSH();

	if ( !skybox.cubeFaces[0] || skybox.cubeSize <= 0 ) {
		return;
	}

	for ( face = 0; face < 6; face++ ) {
		for ( y = 0; y < skybox.cubeSize; y++ ) {
			for ( x = 0; x < skybox.cubeSize; x++ ) {
				const float u = ((float)x + 0.5f) / (float)skybox.cubeSize * 2.0f - 1.0f;
				const float v = ((float)y + 0.5f) / (float)skybox.cubeSize * 2.0f - 1.0f;
				const float weight = 4.0f / powf( 1.0f + u * u + v * v, 1.5f );
				vec3_t dir;
				const int idx = ( y * skybox.cubeSize + x ) * 4;
				int i;

				SkyboxHDR_DirFromCubeFace( face, ((float)x + 0.5f) / (float)skybox.cubeSize, ((float)y + 0.5f) / (float)skybox.cubeSize, dir );

				for ( i = 0; i < 9; i++ ) {
					const float basis = SkyboxHDR_SHBasis( i, dir );
					skybox.shCoeffs[i][0] += skybox.cubeFaces[face][idx + 0] * basis * weight;
					skybox.shCoeffs[i][1] += skybox.cubeFaces[face][idx + 1] * basis * weight;
					skybox.shCoeffs[i][2] += skybox.cubeFaces[face][idx + 2] * basis * weight;
				}

				totalWeight += weight;
			}
		}
	}

	if ( totalWeight > 0.0f ) {
		const float norm = ( 4.0f * PI_F ) / totalWeight;
		int i;

		for ( i = 0; i < 9; i++ ) {
			skybox.shCoeffs[i][0] *= norm;
			skybox.shCoeffs[i][1] *= norm;
			skybox.shCoeffs[i][2] *= norm;
		}

		skybox.hasSHCoeffs = qtrue;
	}
}

static qboolean SkyboxHDR_UploadGPUImages( void ) {
	byte *prefilterBuffer;
	byte *irradianceBuffer;
	int prefilterSize;
	int irradianceSize;

	if ( !skybox.loaded || !skybox.prefilteredFaces[0] || !skybox.irradianceFaces[0] ) {
		return qfalse;
	}

	SkyboxHDR_InitGPUImage( &skyboxPrefilteredImage, skyboxPrefilteredName, skybox.prefilteredSize, skybox.prefilteredMips );
	SkyboxHDR_InitGPUImage( &skyboxIrradianceImage, skyboxIrradianceName, skybox.irradianceSize, 1 );

	prefilterBuffer = SkyboxHDR_BuildUploadBuffer( skybox.prefilteredFaces, skybox.prefilteredSize, skybox.prefilteredMips, &prefilterSize );
	irradianceBuffer = SkyboxHDR_BuildUploadBuffer( skybox.irradianceFaces, skybox.irradianceSize, 1, &irradianceSize );

	vk_upload_cubemap_mip_data( &skyboxPrefilteredImage, skybox.prefilteredSize, skybox.prefilteredMips,
		prefilterBuffer, prefilterSize, 4 * (int)sizeof( float ), qfalse );
	vk_upload_cubemap_mip_data( &skyboxIrradianceImage, skybox.irradianceSize, 1,
		irradianceBuffer, irradianceSize, 4 * (int)sizeof( float ), qfalse );

	ri.Hunk_FreeTempMemory( irradianceBuffer );
	ri.Hunk_FreeTempMemory( prefilterBuffer );

	SkyboxHDR_ExtractSHCoeffs();
	return qtrue;
}

void SkyboxHDR_RegisterCvars(void) {
	r_skyboxHDR = ri.Cvar_Get("r_skyboxHDR", "", CVAR_ARCHIVE);
	ri.Cvar_SetDescription(r_skyboxHDR, "Path to HDR EXR/PNG skybox panorama (empty = disabled).");

	r_skyboxHDR_exposure = ri.Cvar_Get("r_skyboxHDR_exposure", "1.0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription(r_skyboxHDR_exposure, "Exposure multiplier for HDR skybox.");

	r_skyboxHDR_rotation = ri.Cvar_Get("r_skyboxHDR_rotation", "0.0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription(r_skyboxHDR_rotation, "Rotation of HDR skybox in degrees around Y axis.");

	r_skyboxHDR_intensity = ri.Cvar_Get("r_skyboxHDR_intensity", "1.0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription(r_skyboxHDR_intensity, "IBL lighting intensity multiplier from HDR skybox.");
}

void SkyboxHDR_Init(void) {
	Com_Memset(&skybox, 0, sizeof(skybox));
	Com_Memset(&skyboxPrefilteredImage, 0, sizeof(skyboxPrefilteredImage));
	Com_Memset(&skyboxIrradianceImage, 0, sizeof(skyboxIrradianceImage));
	SkyboxHDR_RegisterCvars();
	skybox.exposure = 1.0f;
	skybox.intensity = 1.0f;
	skybox.tintR = skybox.tintG = skybox.tintB = 1.0f;
	skyboxLoadFailed = qfalse;
	SkyboxHDR_ResetSH();
	ri.Printf(PRINT_ALL, "HDR Skybox system initialized\n");
}

void SkyboxHDR_Shutdown(void) {
	SkyboxHDR_DestroyGPUImages();
	SkyboxHDR_Unload();
	skyboxLoadFailed = qfalse;
}

qboolean SkyboxHDR_Load(const char *filename, skyboxProjection_t projection) {
	float *hdrData = NULL;
	int w, h;

	SkyboxHDR_Unload();

	if ( !SkyboxHDR_LoadPanoramaImage( filename, &hdrData, &w, &h ) ) {
		ri.Printf(PRINT_WARNING, "SkyboxHDR: could not load %s\n", filename);
		return qfalse;
	}

	skybox.hdrData = hdrData;
	skybox.srcWidth = w;
	skybox.srcHeight = h;
	skybox.projection = projection;
	Q_strncpyz(skybox.filename, filename, sizeof(skybox.filename));
	skybox.loaded = qtrue;

	skybox.exposure = r_skyboxHDR_exposure ? r_skyboxHDR_exposure->value : 1.0f;
	skybox.rotation = r_skyboxHDR_rotation ? r_skyboxHDR_rotation->value : 0.0f;
	skybox.intensity = r_skyboxHDR_intensity ? r_skyboxHDR_intensity->value : 1.0f;

	SkyboxHDR_GenerateCubemap();
	SkyboxHDR_GenerateIrradiance();
	SkyboxHDR_GeneratePrefiltered();

	{
		const char *ext = COM_GetExtension( filename );
		const char *fmt = ( ext && !Q_stricmp( ext, "hdr" ) ) ? "Radiance .hdr" : "EXR";
		ri.Printf( PRINT_ALL, "SkyboxHDR: loaded %s (%dx%d, %s, %s projection)\n",
			filename, w, h, fmt,
			projection == SKYBOX_PROJ_EQUIRECTANGULAR ? "equirectangular" :
			projection == SKYBOX_PROJ_SPHERICAL_MIRROR ? "spherical" : "cubemap" );
	}

	return qtrue;
}

qboolean SkyboxHDR_LoadCubeFaces(const char *baseName) {
	static const char *faceSuffix[] = { "_px", "_nx", "_py", "_ny", "_pz", "_nz" };
	int f;

	SkyboxHDR_Unload();

	skybox.cubeSize = 0;
	skybox.projection = SKYBOX_PROJ_CUBEMAP_FACES;

	for (f = 0; f < 6; f++) {
		char faceName[MAX_QPATH];
		float *faceData = NULL;
		int w, h;

			Com_sprintf(faceName, sizeof(faceName), "%s%s.exr", baseName, faceSuffix[f]);
			if ( !SkyboxHDR_LoadPanoramaImage( faceName, &faceData, &w, &h ) ) {
				Com_sprintf(faceName, sizeof(faceName), "%s%s.hdr", baseName, faceSuffix[f]);
				SkyboxHDR_LoadPanoramaImage( faceName, &faceData, &w, &h );
			}

		if (!faceData) {
			ri.Printf(PRINT_WARNING, "SkyboxHDR: missing face %s\n", faceName);
			SkyboxHDR_Unload();
			return qfalse;
		}

		if (skybox.cubeSize == 0) skybox.cubeSize = w;
		skybox.cubeFaces[f] = faceData;
	}

	skybox.loaded = qtrue;
	Q_strncpyz(skybox.filename, baseName, sizeof(skybox.filename));

	SkyboxHDR_GenerateIrradiance();
	SkyboxHDR_GeneratePrefiltered();
	SkyboxHDR_ExtractSHCoeffs();

	ri.Printf(PRINT_ALL, "SkyboxHDR: loaded 6 cubemap faces from %s (%dx%d)\n",
		baseName, skybox.cubeSize, skybox.cubeSize);
	return qtrue;
}

void SkyboxHDR_Unload(void) {
	int f;
	if (skybox.hdrData) { Z_Free(skybox.hdrData); skybox.hdrData = NULL; }
	for (f = 0; f < 6; f++) {
		if (skybox.cubeFaces[f]) { Z_Free(skybox.cubeFaces[f]); skybox.cubeFaces[f] = NULL; }
		if (skybox.irradianceFaces[f]) { Z_Free(skybox.irradianceFaces[f]); skybox.irradianceFaces[f] = NULL; }
		if (skybox.prefilteredFaces[f]) { Z_Free(skybox.prefilteredFaces[f]); skybox.prefilteredFaces[f] = NULL; }
	}
	SkyboxHDR_ResetSH();
	skybox.loaded = qfalse;
}

void SkyboxHDR_GenerateCubemap(void) {
	int face, y, x;
	int size = SKYBOX_HDR_CUBEMAP_SIZE;

	if (!skybox.hdrData) return;

	skybox.cubeSize = size;

	for (face = 0; face < 6; face++) {
		skybox.cubeFaces[face] = (float *)Z_Malloc(size * size * 4 * sizeof(float));
		if (!skybox.cubeFaces[face]) {
			ri.Printf(PRINT_WARNING, "SkyboxHDR: failed to allocate cubemap face %d\n", face);
			return;
		}

		for (y = 0; y < size; y++) {
			for (x = 0; x < size; x++) {
				float u = ((float)x + 0.5f) / size;
				float v = ((float)y + 0.5f) / size;
				float dir[3], rgb[3];

				SkyboxHDR_DirFromCubeFace(face, u, v, dir);
				SkyboxHDR_SampleEquirect(skybox.hdrData, skybox.srcWidth, skybox.srcHeight, dir, rgb);

				int idx = (y * size + x) * 4;
				skybox.cubeFaces[face][idx + 0] = rgb[0];
				skybox.cubeFaces[face][idx + 1] = rgb[1];
				skybox.cubeFaces[face][idx + 2] = rgb[2];
				skybox.cubeFaces[face][idx + 3] = 1.0f;
			}
		}
	}

	ri.Printf(PRINT_DEVELOPER, "SkyboxHDR: generated %dx%d cubemap\n", size, size);
}

void SkyboxHDR_GenerateIrradiance(void) {
	int face, y, x, sf, sy, sx;
	int size = SKYBOX_HDR_IRRADIANCE_SIZE;
	int sampleSize = skybox.cubeSize > 0 ? skybox.cubeSize : SKYBOX_HDR_CUBEMAP_SIZE;
	int sampleStep;

	skybox.irradianceSize = size;
	sampleStep = sampleSize / 16;
	if (sampleStep < 1) sampleStep = 1;

	for (face = 0; face < 6; face++) {
		skybox.irradianceFaces[face] = (float *)Z_Malloc(size * size * 4 * sizeof(float));
		if (!skybox.irradianceFaces[face]) {
			ri.Printf(PRINT_WARNING, "SkyboxHDR: failed to allocate irradiance face %d\n", face);
			return;
		}

		for (y = 0; y < size; y++) {
			for (x = 0; x < size; x++) {
				float u = ((float)x + 0.5f) / size;
				float v = ((float)y + 0.5f) / size;
				float normal[3];
				float irrad[3] = {0, 0, 0};
				float totalWeight = 0;

				SkyboxHDR_DirFromCubeFace(face, u, v, normal);

				for (sf = 0; sf < 6; sf++) {
					if (!skybox.cubeFaces[sf]) continue;
					for (sy = 0; sy < sampleSize; sy += sampleStep) {
						for (sx = 0; sx < sampleSize; sx += sampleStep) {
							float su = ((float)sx + 0.5f) / sampleSize;
							float sv = ((float)sy + 0.5f) / sampleSize;
							float sdir[3];
							SkyboxHDR_DirFromCubeFace(sf, su, sv, sdir);

							float ndotl = normal[0]*sdir[0] + normal[1]*sdir[1] + normal[2]*sdir[2];
							if (ndotl <= 0) continue;

							int sidx = (sy * sampleSize + sx) * 4;
							irrad[0] += skybox.cubeFaces[sf][sidx + 0] * ndotl;
							irrad[1] += skybox.cubeFaces[sf][sidx + 1] * ndotl;
							irrad[2] += skybox.cubeFaces[sf][sidx + 2] * ndotl;
							totalWeight += ndotl;
						}
					}
				}

				if (totalWeight > 0) {
					irrad[0] *= PI_F / totalWeight;
					irrad[1] *= PI_F / totalWeight;
					irrad[2] *= PI_F / totalWeight;
				}

				int idx = (y * size + x) * 4;
				skybox.irradianceFaces[face][idx + 0] = irrad[0] * skybox.intensity;
				skybox.irradianceFaces[face][idx + 1] = irrad[1] * skybox.intensity;
				skybox.irradianceFaces[face][idx + 2] = irrad[2] * skybox.intensity;
				skybox.irradianceFaces[face][idx + 3] = 1.0f;
			}
		}
	}

	ri.Printf(PRINT_DEVELOPER, "SkyboxHDR: generated %dx%d irradiance cubemap\n", size, size);
}

void SkyboxHDR_GeneratePrefiltered(void) {
	int face, mip, y, x;
	int baseSize = SKYBOX_HDR_PREFILTER_SIZE;

	skybox.prefilteredSize = baseSize;
	skybox.prefilteredMips = SKYBOX_HDR_PREFILTER_MIPS;

	for (face = 0; face < 6; face++) {
		skybox.prefilteredFaces[face] = (float *)Z_Malloc(baseSize * baseSize * 4 * sizeof(float));
		if (!skybox.prefilteredFaces[face]) {
			ri.Printf(PRINT_WARNING, "SkyboxHDR: failed to allocate prefiltered face %d\n", face);
			return;
		}

		for (y = 0; y < baseSize; y++) {
			for (x = 0; x < baseSize; x++) {
				float u = ((float)x + 0.5f) / baseSize;
				float v = ((float)y + 0.5f) / baseSize;

				float rgb[3] = {0, 0, 0};
				if (skybox.cubeFaces[face]) {
					int cubeX = (int)(u * (skybox.cubeSize - 1));
					int cubeY = (int)(v * (skybox.cubeSize - 1));
					int cidx = (cubeY * skybox.cubeSize + cubeX) * 4;
					rgb[0] = skybox.cubeFaces[face][cidx + 0];
					rgb[1] = skybox.cubeFaces[face][cidx + 1];
					rgb[2] = skybox.cubeFaces[face][cidx + 2];
				}

				int idx = (y * baseSize + x) * 4;
				skybox.prefilteredFaces[face][idx + 0] = rgb[0] * skybox.intensity;
				skybox.prefilteredFaces[face][idx + 1] = rgb[1] * skybox.intensity;
				skybox.prefilteredFaces[face][idx + 2] = rgb[2] * skybox.intensity;
				skybox.prefilteredFaces[face][idx + 3] = 1.0f;
			}
		}
	}

	(void)mip;
	ri.Printf(PRINT_DEVELOPER, "SkyboxHDR: generated %dx%d prefiltered cubemap (%d mips)\n",
		baseSize, baseSize, SKYBOX_HDR_PREFILTER_MIPS);
}

const skyboxHDR_t *SkyboxHDR_Get(void) { return &skybox; }
qboolean SkyboxHDR_IsLoaded(void) { return skybox.loaded; }

void SkyboxHDR_UpdateRuntime(void) {
	qboolean changed;

	if ( vk.device == VK_NULL_HANDLE || !r_skyboxHDR ) {
		return;
	}

	changed = r_skyboxHDR->modified;
	changed = changed || ( r_skyboxHDR_exposure && r_skyboxHDR_exposure->modified );
	changed = changed || ( r_skyboxHDR_rotation && r_skyboxHDR_rotation->modified );
	changed = changed || ( r_skyboxHDR_intensity && r_skyboxHDR_intensity->modified );

	if ( !changed ) {
		if ( !r_skyboxHDR->string[0] ) {
			return;
		}
		if ( skybox.loaded &&
			skyboxPrefilteredImage.handle != VK_NULL_HANDLE &&
			skyboxIrradianceImage.handle != VK_NULL_HANDLE ) {
			return;
		}
		if ( skyboxLoadFailed ) {
			return;
		}
	}

	if ( !r_skyboxHDR->string[0] ) {
		if ( skybox.loaded || skyboxPrefilteredImage.handle != VK_NULL_HANDLE || skyboxIrradianceImage.handle != VK_NULL_HANDLE ) {
			ri.Printf( PRINT_ALL, "SkyboxHDR: disabled\n" );
		}
		SkyboxHDR_DestroyGPUImages();
		SkyboxHDR_Unload();
		skyboxLoadFailed = qfalse;
	} else {
		skyboxLoadFailed = qfalse;
		if ( !SkyboxHDR_Load( r_skyboxHDR->string, SKYBOX_PROJ_EQUIRECTANGULAR ) ) {
			SkyboxHDR_DestroyGPUImages();
			SkyboxHDR_Unload();
			skyboxLoadFailed = qtrue;
		} else if ( !SkyboxHDR_UploadGPUImages() ) {
			ri.Printf( PRINT_WARNING, "SkyboxHDR: failed to upload GPU cubemaps for %s\n", r_skyboxHDR->string );
			SkyboxHDR_DestroyGPUImages();
			SkyboxHDR_Unload();
			skyboxLoadFailed = qtrue;
		} else {
			ri.Printf( PRINT_ALL, "SkyboxHDR: GPU cubemaps ready for %s\n", r_skyboxHDR->string );
		}
	}

	r_skyboxHDR->modified = qfalse;
	if ( r_skyboxHDR_exposure ) r_skyboxHDR_exposure->modified = qfalse;
	if ( r_skyboxHDR_rotation ) r_skyboxHDR_rotation->modified = qfalse;
	if ( r_skyboxHDR_intensity ) r_skyboxHDR_intensity->modified = qfalse;
}

VkDescriptorSet SkyboxHDR_GetPrefilteredDescriptor(void) {
	return ( skybox.loaded && skyboxPrefilteredImage.handle != VK_NULL_HANDLE && skyboxPrefilteredImage.descriptor != VK_NULL_HANDLE )
		? skyboxPrefilteredImage.descriptor : VK_NULL_HANDLE;
}

VkDescriptorSet SkyboxHDR_GetIrradianceDescriptor(void) {
	return ( skybox.loaded && skyboxIrradianceImage.handle != VK_NULL_HANDLE && skyboxIrradianceImage.descriptor != VK_NULL_HANDLE )
		? skyboxIrradianceImage.descriptor : VK_NULL_HANDLE;
}

qboolean SkyboxHDR_CopySHCoeffs(vec4_t out[9]) {
	if ( !out || !skybox.hasSHCoeffs ) {
		return qfalse;
	}

	Com_Memcpy( out, skybox.shCoeffs, sizeof( skybox.shCoeffs ) );
	return qtrue;
}

void SkyboxHDR_SetExposure(float e) { skybox.exposure = e > 0 ? e : 0.01f; }
void SkyboxHDR_SetRotation(float d) { skybox.rotation = d; }
void SkyboxHDR_SetTint(float r, float g, float b) { skybox.tintR = r; skybox.tintG = g; skybox.tintB = b; }
void SkyboxHDR_SetIntensity(float i) { skybox.intensity = i > 0 ? i : 0; }

void SkyboxHDR_SampleDirection(const float *dir, float *outRGB) {
	if (!skybox.loaded || !skybox.hdrData) { outRGB[0]=outRGB[1]=outRGB[2]=0; return; }
	SkyboxHDR_SampleEquirect(skybox.hdrData, skybox.srcWidth, skybox.srcHeight, dir, outRGB);
}

void SkyboxHDR_SampleIrradiance(const float *normal, float *outRGB) {
	int bestFace = 0, i;
	float maxDot = -999;

	outRGB[0] = outRGB[1] = outRGB[2] = 0;
	if (!skybox.loaded) return;

	static const float faceDir[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
	for (i = 0; i < 6; i++) {
		float d = normal[0]*faceDir[i][0] + normal[1]*faceDir[i][1] + normal[2]*faceDir[i][2];
		if (d > maxDot) { maxDot = d; bestFace = i; }
	}

	if (!skybox.irradianceFaces[bestFace]) return;

	int size = skybox.irradianceSize;
	float u, v;
	switch (bestFace) {
		case 0: u = (-normal[2]/normal[0]+1)*0.5f; v = (normal[1]/fabsf(normal[0])+1)*0.5f; break;
		case 1: u = (normal[2]/fabsf(normal[0])+1)*0.5f; v = (normal[1]/fabsf(normal[0])+1)*0.5f; break;
		case 2: u = (normal[0]/fabsf(normal[1])+1)*0.5f; v = (-normal[2]/normal[1]+1)*0.5f; break;
		case 3: u = (normal[0]/fabsf(normal[1])+1)*0.5f; v = (normal[2]/fabsf(normal[1])+1)*0.5f; break;
		case 4: u = (normal[0]/fabsf(normal[2])+1)*0.5f; v = (normal[1]/fabsf(normal[2])+1)*0.5f; break;
		default: u = (-normal[0]/fabsf(normal[2])+1)*0.5f; v = (normal[1]/fabsf(normal[2])+1)*0.5f; break;
	}

	int px = (int)(u * (size-1)); if (px<0) px=0; if (px>=size) px=size-1;
	int py = (int)(v * (size-1)); if (py<0) py=0; if (py>=size) py=size-1;
	int idx = (py * size + px) * 4;
	outRGB[0] = skybox.irradianceFaces[bestFace][idx+0];
	outRGB[1] = skybox.irradianceFaces[bestFace][idx+1];
	outRGB[2] = skybox.irradianceFaces[bestFace][idx+2];
}
