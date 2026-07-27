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
#include "vk_texture_image.h"
#include "vk_sky_owner.h"
#include <math.h>

static skyboxHDR_t skybox;
static image_t skyboxPrefilteredImage;
static image_t skyboxIrradianceImage;
static qboolean skyboxLoadFailed;
static cvar_t *r_skyboxHDR;
static cvar_t *r_skyboxHDR_exposure;
static cvar_t *r_skyboxHDR_rotation;
static cvar_t *r_skyboxHDR_intensity;
static cvar_t *r_skyboxHDR_projection;
static cvar_t *r_skyExposureEV;
static cvar_t *r_skyLuminanceScale;
static cvar_t *r_skyHdrDebug;
static cvar_t *r_skyLod;
static cvar_t *r_skyFaceSize;

static image_t *s_displayFaces[6];
static int s_displayFaceSize;
static float s_displayLumMin, s_displayLumMax, s_displayLumMean;
static int s_displayAbove1, s_displayAbove4, s_displayAbove16;
static const char *s_firstFlattenStage = "SKY_HDR_VALUES_CLAMPED"; /* historical; cleared after float path */

static void SkyboxHDR_Status_f( void );
static void SkyboxHDR_Validate_f( void );
static void SkyboxHDR_Capture_f( void );
static void SkyboxHDR_ExposureStatus_f( void );

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

	buffer = (byte *)ri.Malloc( totalSize );
	if ( !buffer ) {
		*outSize = 0;
		return NULL;
	}
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
	/* Raw scene-linear sample (no IBL exposure bake). */
	outRGB[0] = data[idx + 0] * skybox.tintR;
	outRGB[1] = data[idx + 1] * skybox.tintG;
	outRGB[2] = data[idx + 2] * skybox.tintB;
}

static void SkyboxHDR_SampleEquirectForIBL(const float *data, int w, int h, const float *dir, float *outRGB) {
	SkyboxHDR_SampleEquirect( data, w, h, dir, outRGB );
	outRGB[0] *= skybox.exposure * skybox.intensity;
	outRGB[1] *= skybox.exposure * skybox.intensity;
	outRGB[2] *= skybox.exposure * skybox.intensity;
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

	ri.Free( irradianceBuffer );
	ri.Free( prefilterBuffer );

	SkyboxHDR_ExtractSHCoeffs();
	return qtrue;
}

void SkyboxHDR_RegisterCvars(void) {
	r_skyboxHDR = ri.Cvar_Get("r_skyboxHDR", "", CVAR_ARCHIVE);
	ri.Cvar_SetDescription(r_skyboxHDR,
		"Path to HDR equirectangular panorama (.exr via OpenEXR/tinyexr, or .hdr). Empty = disabled.");

	r_skyboxHDR_exposure = ri.Cvar_Get("r_skyboxHDR_exposure", "1.0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription(r_skyboxHDR_exposure, "Exposure multiplier for HDR skybox.");

	r_skyboxHDR_rotation = ri.Cvar_Get("r_skyboxHDR_rotation", "0.0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription(r_skyboxHDR_rotation, "Rotation of HDR skybox in degrees around vertical axis.");

	r_skyboxHDR_intensity = ri.Cvar_Get("r_skyboxHDR_intensity", "1.0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription(r_skyboxHDR_intensity, "IBL lighting intensity multiplier from HDR skybox.");

	r_skyboxHDR_projection = ri.Cvar_Get( "r_skyboxHDR_projection", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_skyboxHDR_projection, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_skyboxHDR_projection,
		"0=equirectangular (default, also auto for ~2:1 images), 1=cubemap faces, "
		"2=vertical cross, 3=horizontal cross, 4=spherical mirror." );

	r_skyExposureEV = ri.Cvar_Get( "r_skyExposureEV", "0.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_skyExposureEV,
		"Scene-referred EV for visible HDR sky (exp2). Applied once before SceneHDR; not a post-tonemap boost." );

	r_skyLuminanceScale = ri.Cvar_Get( "r_skyLuminanceScale", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_skyLuminanceScale,
		"Linear luminance scale for visible HDR sky after EV. Keep 1.0 unless calibrating assets." );

	r_skyHdrDebug = ri.Cvar_Get( "r_skyHdrDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_skyHdrDebug, "0", "12", CV_INTEGER );
	ri.Cvar_SetDescription( r_skyHdrDebug,
		"HDR sky debug: 0 off, 3 values>1 heatmap policy, 5 mip, 10 pre-tonemap (see docs/HDR_SKY_RENDERING.md)." );

	r_skyLod = ri.Cvar_Get( "r_skyLod", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_skyLod, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_skyLod,
		"Visible sky face lod shift after base size (0=base, 1=half, …). Prefer r_skyFaceSize for quality." );

	r_skyFaceSize = ri.Cvar_Get( "r_skyFaceSize", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_skyFaceSize, "0", "4096", CV_INTEGER );
	ri.Cvar_SetDescription( r_skyFaceSize,
		"Visible HDR sky cube-face resolution. 0=auto from panorama (≈ equirectWidth/2, clamped 512–2048). "
		"4K EXR needs ≥1024; use 2048 for max detail. Not the IBL prefilter size." );
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
	ri.Cmd_AddCommand( "sky_hdr_status", SkyboxHDR_Status_f );
	ri.Cmd_AddCommand( "sky_hdr_validate", SkyboxHDR_Validate_f );
	ri.Cmd_AddCommand( "sky_hdr_capture", SkyboxHDR_Capture_f );
	ri.Cmd_AddCommand( "sky_exposure_status", SkyboxHDR_ExposureStatus_f );
	ri.Printf(PRINT_ALL, "HDR Skybox system initialized (scene-linear visible sky)\n");
}

void SkyboxHDR_Shutdown(void) {
	SkyboxHDR_DestroyGPUImages();
	SkyboxHDR_ClearDisplayFaces();
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

	/* ~2:1 panoramas are equirectangular even if projection was left at default. */
	if ( projection != SKYBOX_PROJ_CUBEMAP_FACES &&
			w > 0 && h > 0 && w >= ( h * 3 ) / 2 && w <= ( h * 5 ) / 2 ) {
		projection = SKYBOX_PROJ_EQUIRECTANGULAR;
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
	SkyboxHDR_BuildDisplayFaces();

	{
		const char *ext = COM_GetExtension( filename );
		const char *fmt = ( ext && !Q_stricmp( ext, "hdr" ) ) ? "Radiance .hdr" : "OpenEXR";
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
	SkyboxHDR_BuildDisplayFaces();

	ri.Printf(PRINT_ALL, "SkyboxHDR: loaded 6 cubemap faces from %s (%dx%d)\n",
		baseName, skybox.cubeSize, skybox.cubeSize);
	return qtrue;
}

void SkyboxHDR_Unload(void) {
	int f;
	/* Keep display faces so map sky shaders retain valid image_t* across cvar reloads. */
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
				SkyboxHDR_SampleEquirectForIBL(skybox.hdrData, skybox.srcWidth, skybox.srcHeight, dir, rgb);

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
	qboolean displayOnly;

	if ( vk.device == VK_NULL_HANDLE || !r_skyboxHDR ) {
		return;
	}

	changed = r_skyboxHDR->modified;
	changed = changed || ( r_skyboxHDR_exposure && r_skyboxHDR_exposure->modified );
	changed = changed || ( r_skyboxHDR_rotation && r_skyboxHDR_rotation->modified );
	changed = changed || ( r_skyboxHDR_intensity && r_skyboxHDR_intensity->modified );
	changed = changed || ( r_skyboxHDR_projection && r_skyboxHDR_projection->modified );

	displayOnly = ( r_skyExposureEV && r_skyExposureEV->modified ) ||
		( r_skyLuminanceScale && r_skyLuminanceScale->modified ) ||
		( r_skyLod && r_skyLod->modified ) ||
		( r_skyFaceSize && r_skyFaceSize->modified );

	if ( !changed && displayOnly && skybox.loaded ) {
		skybox.exposure = r_skyboxHDR_exposure ? r_skyboxHDR_exposure->value : skybox.exposure;
		skybox.rotation = r_skyboxHDR_rotation ? r_skyboxHDR_rotation->value : skybox.rotation;
		skybox.intensity = r_skyboxHDR_intensity ? r_skyboxHDR_intensity->value : skybox.intensity;
		SkyboxHDR_BuildDisplayFaces();
		if ( r_skyExposureEV ) r_skyExposureEV->modified = qfalse;
		if ( r_skyLuminanceScale ) r_skyLuminanceScale->modified = qfalse;
		if ( r_skyLod ) r_skyLod->modified = qfalse;
		if ( r_skyFaceSize ) r_skyFaceSize->modified = qfalse;
		return;
	}

	changed = changed || displayOnly;

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
		SkyboxHDR_ClearDisplayFaces();
		skyboxLoadFailed = qfalse;
	} else {
		skyboxProjection_t proj = SKYBOX_PROJ_EQUIRECTANGULAR;
		if ( r_skyboxHDR_projection ) {
			proj = (skyboxProjection_t)r_skyboxHDR_projection->integer;
			if ( proj < 0 || proj >= SKYBOX_PROJ_COUNT ) {
				proj = SKYBOX_PROJ_EQUIRECTANGULAR;
			}
		}
		skyboxLoadFailed = qfalse;
		if ( !SkyboxHDR_Load( r_skyboxHDR->string, proj ) ) {
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
	if ( r_skyboxHDR_projection ) r_skyboxHDR_projection->modified = qfalse;
	if ( r_skyExposureEV ) r_skyExposureEV->modified = qfalse;
	if ( r_skyLuminanceScale ) r_skyLuminanceScale->modified = qfalse;
	if ( r_skyLod ) r_skyLod->modified = qfalse;
	if ( r_skyFaceSize ) r_skyFaceSize->modified = qfalse;
}

qboolean SkyboxHDR_GetCubemapViews( VkImageView *prefilterOut, VkImageView *irradianceOut )
{
	if ( prefilterOut ) {
		*prefilterOut = VK_NULL_HANDLE;
	}
	if ( irradianceOut ) {
		*irradianceOut = VK_NULL_HANDLE;
	}

	if ( skybox.loaded && skyboxPrefilteredImage.view != VK_NULL_HANDLE ) {
		if ( prefilterOut ) {
			*prefilterOut = skyboxPrefilteredImage.view;
		}
		if ( irradianceOut && skyboxIrradianceImage.view != VK_NULL_HANDLE ) {
			*irradianceOut = skyboxIrradianceImage.view;
		}
		return qtrue;
	}

	if ( tr.numCubemaps > 0 ) {
		cubemap_t *cube = &tr.cubemaps[0];
		if ( cube->prefiltered_image && cube->prefiltered_image->view != VK_NULL_HANDLE ) {
			if ( prefilterOut ) {
				*prefilterOut = cube->prefiltered_image->view;
			}
		}
		if ( cube->irradiance_image && cube->irradiance_image->view != VK_NULL_HANDLE ) {
			if ( irradianceOut ) {
				*irradianceOut = cube->irradiance_image->view;
			}
		}
		if ( ( prefilterOut && *prefilterOut != VK_NULL_HANDLE ) ||
			( irradianceOut && *irradianceOut != VK_NULL_HANDLE ) ) {
			return qtrue;
		}
	}

	if ( tr.emptyCubemap && tr.emptyCubemap->view != VK_NULL_HANDLE ) {
		if ( prefilterOut ) {
			*prefilterOut = tr.emptyCubemap->view;
		}
		if ( irradianceOut ) {
			*irradianceOut = tr.emptyCubemap->view;
		}
		return qtrue;
	}

	return qfalse;
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
	SkyboxHDR_SampleEquirectForIBL(skybox.hdrData, skybox.srcWidth, skybox.srcHeight, dir, outRGB);
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

static void SkyboxHDR_SampleCubeDirection( const float *dir, float *outRGB ) {
	int face = 0;
	float ax = fabsf( dir[0] ), ay = fabsf( dir[1] ), az = fabsf( dir[2] );
	float uc, vc, u, v;
	int size, px, py, idx;
	const float *faceData;
	float sc;

	outRGB[0] = outRGB[1] = outRGB[2] = 0.0f;
	if ( !skybox.cubeFaces[0] || skybox.cubeSize <= 0 ) {
		return;
	}

	if ( ax >= ay && ax >= az ) {
		face = ( dir[0] > 0.0f ) ? 0 : 1;
		sc = fabsf( dir[0] );
		if ( face == 0 ) {
			uc = -dir[2] / sc;
			vc = dir[1] / sc;
		} else {
			uc = dir[2] / sc;
			vc = dir[1] / sc;
		}
	} else if ( ay >= ax && ay >= az ) {
		face = ( dir[1] > 0.0f ) ? 2 : 3;
		sc = fabsf( dir[1] );
		if ( face == 2 ) {
			uc = dir[0] / sc;
			vc = -dir[2] / sc;
		} else {
			uc = dir[0] / sc;
			vc = dir[2] / sc;
		}
	} else {
		face = ( dir[2] > 0.0f ) ? 4 : 5;
		sc = fabsf( dir[2] );
		if ( face == 4 ) {
			uc = dir[0] / sc;
			vc = dir[1] / sc;
		} else {
			uc = -dir[0] / sc;
			vc = dir[1] / sc;
		}
	}

	faceData = skybox.cubeFaces[face];
	if ( !faceData ) {
		return;
	}

	u = ( uc + 1.0f ) * 0.5f;
	v = ( vc + 1.0f ) * 0.5f;
	size = skybox.cubeSize;
	px = (int)( u * ( size - 1 ) + 0.5f );
	py = (int)( v * ( size - 1 ) + 0.5f );
	if ( px < 0 ) px = 0;
	if ( px >= size ) px = size - 1;
	if ( py < 0 ) py = 0;
	if ( py >= size ) py = size - 1;
	idx = ( py * size + px ) * 4;
	outRGB[0] = faceData[idx + 0] * skybox.tintR;
	outRGB[1] = faceData[idx + 1] * skybox.tintG;
	outRGB[2] = faceData[idx + 2] * skybox.tintB;
}

/* Quake Z-up MakeSkyVec axes; same table as tr_sky.c. */
static void SkyboxHDR_QuakeSkyDir( int axis, float s, float t, float *outDir ) {
	static const int st_to_vec[6][3] = {
		{3, -1, 2},
		{-3, 1, 2},
		{1, 3, 2},
		{-1, -3, 2},
		{-2, -1, 3},
		{2, -1, -3}
	};
	float b[3];
	int j, k;
	float len;

	b[0] = s;
	b[1] = t;
	b[2] = 1.0f;

	for ( j = 0; j < 3; j++ ) {
		k = st_to_vec[axis][j];
		if ( k < 0 ) {
			outDir[j] = -b[-k - 1];
		} else {
			outDir[j] = b[k - 1];
		}
	}

	len = sqrtf( outDir[0] * outDir[0] + outDir[1] * outDir[1] + outDir[2] * outDir[2] );
	if ( len > 0.0f ) {
		outDir[0] /= len;
		outDir[1] /= len;
		outDir[2] /= len;
	}
}

static float SkyboxHDR_Luminance( float r, float g, float b ) {
	return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

static float SkyboxHDR_VisibleRadianceScale( void ) {
	float ev = r_skyExposureEV ? r_skyExposureEV->value : 0.0f;
	float scale = r_skyLuminanceScale ? r_skyLuminanceScale->value : 1.0f;
	if ( scale <= 0.0f ) {
		scale = 1.0f;
	}
	return exp2f( ev ) * scale;
}

/*
 * Match visible cube faces to panorama density.
 * A W×H equirect spans 360°; each cube face is 90° → ≈ W/4 texels for parity,
 * but horizon detail benefits from closer to W/2. Auto uses W/2 clamped.
 */
static int SkyboxHDR_VisibleFaceSize( void ) {
	int size = 1024;
	int lodShift;

	if ( r_skyFaceSize && r_skyFaceSize->integer > 0 ) {
		size = r_skyFaceSize->integer;
	} else if ( skybox.srcWidth > 0 ) {
		/* 4096 equirect → 2048 faces; 2048 → 1024; floor at 512. */
		size = skybox.srcWidth / 2;
		if ( size < 512 ) {
			size = 512;
		}
		if ( size > 2048 ) {
			size = 2048;
		}
	}

	/* Power-of-two round down for predictable GPU uploads. */
	{
		int pot = 512;
		while ( pot < size && pot < 4096 ) {
			pot <<= 1;
		}
		if ( pot > size && pot > 512 ) {
			pot >>= 1;
		}
		size = pot;
	}

	lodShift = ( r_skyLod && r_skyLod->integer > 0 ) ? r_skyLod->integer : 0;
	if ( lodShift > 0 ) {
		size >>= lodShift;
		if ( size < 64 ) {
			size = 64;
		}
	}
	return size;
}

void SkyboxHDR_ClearDisplayFaces( void ) {
	Com_Memset( s_displayFaces, 0, sizeof( s_displayFaces ) );
	s_displayFaceSize = 0;
	s_displayLumMin = s_displayLumMax = s_displayLumMean = 0.0f;
	s_displayAbove1 = s_displayAbove4 = s_displayAbove16 = 0;
}

image_t *SkyboxHDR_GetDisplayFace( int outerboxIndex ) {
	if ( outerboxIndex < 0 || outerboxIndex >= 6 ) {
		return NULL;
	}
	return s_displayFaces[outerboxIndex];
}

/*
 * Visible sky outerbox: scene-linear RGBA32F (values may exceed 1.0).
 * FIRST_STAGE_FLATTENING_SKY was previously Reinhard+gamma into RGBA8 UNORM
 * (SKY_HDR_VALUES_CLAMPED / SKY_RENDERED_TO_LDR_TARGET). That path is removed.
 */
qboolean SkyboxHDR_BuildDisplayFaces( void ) {
	static const int box_to_axis[6] = { 0, 2, 1, 3, 4, 5 };
	static const char *faceNames[6] = {
		"*skyHDR_rt", "*skyHDR_bk", "*skyHDR_lf",
		"*skyHDR_ft", "*skyHDR_up", "*skyHDR_dn"
	};
	const int size = SkyboxHDR_VisibleFaceSize();
	float *rgba;
	int box, y, x;
	float radScale;
	double lumSum = 0.0;
	int sampleCount = 0;

	if ( !skybox.loaded || ( !skybox.hdrData && !skybox.cubeFaces[0] ) ) {
		return qfalse;
	}

	rgba = (float *)ri.Malloc( size * size * 4 * (int)sizeof( float ) );
	if ( !rgba ) {
		return qfalse;
	}

	radScale = SkyboxHDR_VisibleRadianceScale();
	s_displayLumMin = 1e30f;
	s_displayLumMax = 0.0f;
	s_displayAbove1 = s_displayAbove4 = s_displayAbove16 = 0;

	for ( box = 0; box < 6; box++ ) {
		const int axis = box_to_axis[box];

		for ( y = 0; y < size; y++ ) {
			for ( x = 0; x < size; x++ ) {
				float u = ( (float)x + 0.5f ) / (float)size;
				float v = ( (float)y + 0.5f ) / (float)size;
				float s = u * 2.0f - 1.0f;
				float t = 1.0f - v * 2.0f;
				float dirQ[3], dirY[3], rgb[3];
				float lum;
				int idx = ( y * size + x ) * 4;

				SkyboxHDR_QuakeSkyDir( axis, s, t, dirQ );
				dirY[0] = dirQ[0];
				dirY[1] = dirQ[2];
				dirY[2] = -dirQ[1];

				if ( skybox.hdrData ) {
					SkyboxHDR_SampleEquirect( skybox.hdrData, skybox.srcWidth, skybox.srcHeight, dirY, rgb );
				} else {
					SkyboxHDR_SampleCubeDirection( dirY, rgb );
				}

				/* Scene-referred promotion — no 0–1 clamp, no Reinhard, no gamma. */
				rgb[0] *= radScale;
				rgb[1] *= radScale;
				rgb[2] *= radScale;
				if ( rgb[0] < 0.0f ) rgb[0] = 0.0f;
				if ( rgb[1] < 0.0f ) rgb[1] = 0.0f;
				if ( rgb[2] < 0.0f ) rgb[2] = 0.0f;

				rgba[idx + 0] = rgb[0];
				rgba[idx + 1] = rgb[1];
				rgba[idx + 2] = rgb[2];
				rgba[idx + 3] = 1.0f;

				lum = SkyboxHDR_Luminance( rgb[0], rgb[1], rgb[2] );
				if ( lum < s_displayLumMin ) s_displayLumMin = lum;
				if ( lum > s_displayLumMax ) s_displayLumMax = lum;
				lumSum += (double)lum;
				sampleCount++;
				if ( lum > 1.0f ) s_displayAbove1++;
				if ( lum > 4.0f ) s_displayAbove4++;
				if ( lum > 16.0f ) s_displayAbove16++;
			}
		}

		if ( s_displayFaces[box] && s_displayFaceSize == size &&
				s_displayFaces[box]->handle != VK_NULL_HANDLE &&
				s_displayFaces[box]->internalFormat == VK_FORMAT_R32G32B32A32_SFLOAT ) {
			vk_upload_image_rgba32f( s_displayFaces[box], size, size, rgba, size * size * 4 );
		} else {
			s_displayFaces[box] = R_CreateImageRGBA32F( faceNames[box], rgba, size, size,
				IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE | IMGFLAG_NOSCALE );
		}
	}

	s_displayFaceSize = size;
	s_displayLumMean = sampleCount > 0 ? (float)( lumSum / (double)sampleCount ) : 0.0f;
	s_firstFlattenStage = "NONE_SCENE_LINEAR_RGBA32F";
	ri.Free( rgba );
	ri.Printf( PRINT_ALL,
		"SkyboxHDR: scene-linear display faces %dx%d EV=%g scale=%g lum[min/mean/max]=%.4g/%.4g/%.4g above1/4/16=%d/%d/%d\n",
		size, size,
		r_skyExposureEV ? r_skyExposureEV->value : 0.0f,
		r_skyLuminanceScale ? r_skyLuminanceScale->value : 1.0f,
		s_displayLumMin, s_displayLumMean, s_displayLumMax,
		s_displayAbove1, s_displayAbove4, s_displayAbove16 );
	return ( s_displayFaces[0] != NULL ) ? qtrue : qfalse;
}

static void SkyboxHDR_Status_f( void ) {
	const skyboxHDR_t *s = SkyboxHDR_Get();
	const char *fmt = "unknown";
	const char *cs = "SKY_SCENE_LINEAR_HDR";
	const char *enc = "rgba32f_faces";

	if ( s && s->filename[0] ) {
		const char *ext = COM_GetExtension( s->filename );
		if ( ext && !Q_stricmp( ext, "exr" ) ) {
			fmt = "OpenEXR equirect/float";
			cs = "SKY_TEXTURE_HALF_FLOAT";
			enc = "exr_float_rgba";
		} else if ( ext && !Q_stricmp( ext, "hdr" ) ) {
			fmt = "Radiance RGBE";
			cs = "SKY_TEXTURE_RGBE";
			enc = "rgbe";
		}
	}

	ri.Printf( PRINT_ALL, "======== sky_hdr_status ========\n" );
	ri.Printf( PRINT_ALL, "FIRST_STAGE_FLATTENING_SKY: %s\n", s_firstFlattenStage );
	ri.Printf( PRINT_ALL, "active material: %s\n",
		( s && s->loaded ) ? s->filename : "(none)" );
	ri.Printf( PRINT_ALL, "visible resource: SkyRadiance six-face RGBA32F (not specular prefilter)\n" );
	ri.Printf( PRINT_ALL, "specular resource: *skyboxHDRPrefiltered (IBL only)\n" );
	ri.Printf( PRINT_ALL, "irradiance resource: *skyboxHDRIrradiance (IBL only)\n" );
	ri.Printf( PRINT_ALL, "source format: %s (%dx%d)\n", fmt,
		s ? s->srcWidth : 0, s ? s->srcHeight : 0 );
	ri.Printf( PRINT_ALL, "source color space: %s → promoted %s\n", cs, "SKY_SCENE_LINEAR_HDR" );
	ri.Printf( PRINT_ALL, "HDR encoding: %s\n", enc );
	ri.Printf( PRINT_ALL, "selected mip: 0 (visible sky; no roughness LOD)\n" );
	ri.Printf( PRINT_ALL, "sampler: clamp-to-edge, face size %d (source %dx%d, r_skyFaceSize=%d)\n",
		s_displayFaceSize,
		s ? s->srcWidth : 0, s ? s->srcHeight : 0,
		r_skyFaceSize ? r_skyFaceSize->integer : 0 );
	ri.Printf( PRINT_ALL, "sampled luminance min/mean/max: %.6g / %.6g / %.6g\n",
		s_displayLumMin, s_displayLumMean, s_displayLumMax );
	ri.Printf( PRINT_ALL, "pixels above 1/4/16: %d / %d / %d\n",
		s_displayAbove1, s_displayAbove4, s_displayAbove16 );
	ri.Printf( PRINT_ALL, "SceneHDR: written via classic skybox_pipeline into float color (RGBA16F target)\n" );
	ri.Printf( PRINT_ALL, "pre-exposure: sky faces unexposed (EV scale only); SceneHDR exposure later\n" );
	ri.Printf( PRINT_ALL, "exposure multiplier (IBL): %g  visible EV: %g  luminanceScale: %g\n",
		s ? s->exposure : 0.0f,
		r_skyExposureEV ? r_skyExposureEV->value : 0.0f,
		r_skyLuminanceScale ? r_skyLuminanceScale->value : 1.0f );
	ri.Printf( PRINT_ALL, "fog policy: SKY_FOG_ATMOSPHERE_ONLY (HDR owner; no finite-depth fog on clear)\n" );
	ri.Printf( PRINT_ALL, "volumetric policy: shared far-ray; no duplicate sky replace\n" );
	ri.Printf( PRINT_ALL, "tone-map: shared SceneHDR → exposure → tonemap (sky not tonemapped twice)\n" );
	ri.Printf( PRINT_ALL, "r_skyHdrDebug=%d r_skyLod=%d r_skyOwner=%d\n",
		r_skyHdrDebug ? r_skyHdrDebug->integer : 0,
		r_skyLod ? r_skyLod->integer : 0,
		(int)vk_sky_owner() );
	ri.Printf( PRINT_ALL, "================================\n" );
}

static void SkyboxHDR_Validate_f( void ) {
	int fails = 0;

	if ( !SkyboxHDR_IsLoaded() ) {
		ri.Printf( PRINT_ALL, "sky_hdr_validate: SKIP (no HDR sky loaded)\n" );
		return;
	}
	if ( !s_displayFaces[0] || s_displayFaces[0]->internalFormat != VK_FORMAT_R32G32B32A32_SFLOAT ) {
		ri.Printf( PRINT_WARNING, "FAIL: visible sky faces are not RGBA32F\n" );
		fails++;
	}
	if ( s_displayLumMax <= 0.0f ) {
		ri.Printf( PRINT_WARNING, "FAIL: peak luminance <= 0\n" );
		fails++;
	}
	if ( s_displayLumMax <= s_displayLumMin + 1e-6f ) {
		ri.Printf( PRINT_WARNING, "FAIL: no luminance separation (flat field)\n" );
		fails++;
	}
	if ( s_firstFlattenStage && Q_stricmpn( s_firstFlattenStage, "NONE", 4 ) != 0 ) {
		ri.Printf( PRINT_WARNING, "FAIL: flatten stage still marked %s\n", s_firstFlattenStage );
		fails++;
	}
	ri.Printf( PRINT_ALL, "sky_hdr_validate: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

static void SkyboxHDR_Capture_f( void ) {
	ri.Printf( PRINT_ALL, "sky_hdr_capture:\n" );
	ri.Printf( PRINT_ALL, "  SkyTextureDecoded: OpenEXR/HDR float via R_LoadEXR_HDR / R_LoadHDR_Float\n" );
	ri.Printf( PRINT_ALL, "  SkySceneLinear: display faces * exp2(r_skyExposureEV)*r_skyLuminanceScale\n" );
	ri.Printf( PRINT_ALL, "  lum min/mean/max: %.6g / %.6g / %.6g\n",
		s_displayLumMin, s_displayLumMean, s_displayLumMax );
	ri.Printf( PRINT_ALL, "  FIRST_STAGE_FLATTENING_SKY: %s\n", s_firstFlattenStage );
}

static void SkyboxHDR_ExposureStatus_f( void ) {
	cvar_t *autoExp = ri.Cvar_Get( "r_exposure_auto", "0", 0 );
	cvar_t *minV = ri.Cvar_Get( "r_autoExposure_min", "0.5", 0 );
	cvar_t *maxV = ri.Cvar_Get( "r_autoExposure_max", "4.0", 0 );
	cvar_t *spdUp = ri.Cvar_Get( "r_autoExposure_speedUp", "1.5", 0 );
	cvar_t *spdDn = ri.Cvar_Get( "r_autoExposure_speedDown", "3.0", 0 );
	cvar_t *skyW = ri.Cvar_Get( "r_exposureSkyWeight", "0.75", 0 );
	cvar_t *hiPct = ri.Cvar_Get( "r_autoExposure_highPercent", "0.01", 0 );

	ri.Printf( PRINT_ALL, "======== sky_exposure_status ========\n" );
	ri.Printf( PRINT_ALL, "r_exposure_auto     : %d (eye adaptation)\n", autoExp ? autoExp->integer : 0 );
	ri.Printf( PRINT_ALL, "adaptedExposure     : %.4g\n", vk.adaptedExposure );
	ri.Printf( PRINT_ALL, "manual r_exposure   : %.4g\n", r_exposure ? r_exposure->value : 1.0f );
	ri.Printf( PRINT_ALL, "clamp [min,max]     : [%.3g, %.3g]\n",
		minV ? minV->value : 0.5f, maxV ? maxV->value : 4.0f );
	ri.Printf( PRINT_ALL, "speed up/down       : %.3g / %.3g (darken=down when sky bright)\n",
		spdUp ? spdUp->value : 1.5f, spdDn ? spdDn->value : 3.0f );
	ri.Printf( PRINT_ALL, "skyWeight / hiPct   : %.3g / %.3g\n",
		skyW ? skyW->value : 0.75f, hiPct ? hiPct->value : 0.01f );
	ri.Printf( PRINT_ALL, "filteredAvgLogLum   : %.4g valid=%d\n",
		vk.temporal.filteredAvgLogLuminance, vk.temporal.hasValidLuminance ? 1 : 0 );
	ri.Printf( PRINT_ALL, "policy: HDR sky in SceneHDR → luminance histogram → adaptedExposure → tonemap\n" );
	ri.Printf( PRINT_ALL, "=====================================\n" );
}

/*
 * Histogram eye adaptation defaults for HDR sky maps.
 * Mapscripts enable via r_skyboxHDR_autoExposure 1 (preferred) or r_exposure_auto 1.
 * Percentile trim + soft sun-core rejection keep the tiny sun disc from owning EV.
 */
void SkyboxHDR_EnableEyeAdaptation( void ) {
	cvar_t *wantAuto;

	/*
	 * Outdoor HDR AE balance (SceneHDR → exposure → filmic):
	 * - Floor adaptedExposure so doorway/sky cannot crush the whole frame dark.
	 * - Allow interiors to open (max 8).
	 * - Target ~0.18 middle gray; filmic white-point is highlight calibration only.
	 */
	ri.Cvar_Set( "r_autoExposure_min", "0.70" );
	ri.Cvar_Set( "r_autoExposure_max", "8.0" );
	ri.Cvar_Set( "r_exposure_auto_target", "0.18" );
	/* Faster constriction than dilation (asymmetric eye). */
	ri.Cvar_Set( "r_autoExposure_speedUp", "1.5" );   /* brighten into dark */
	ri.Cvar_Set( "r_autoExposure_speedDown", "3.0" ); /* darken into bright */
	ri.Cvar_Set( "r_autoExposure_centerWeight", "0.50" );
	ri.Cvar_Set( "r_autoExposure_lowPercent", "0.05" );
	ri.Cvar_Set( "r_autoExposure_highPercent", "0.05" );
	ri.Cvar_Set( "r_exposureSkyWeight", "0.35" );
	ri.Cvar_Set( "r_exposureComp", "0.25" );
	ri.Cvar_Set( "r_exposureFixed", "0" );
	ri.Cvar_Set( "r_exposureHistogram", "1" );
	ri.Cvar_Set( "r_exposureMeter", "3" );
	ri.Cvar_Set( "r_bloomThresholdEVRelative", "1" );
	ri.Cvar_Set( "r_localExposure", "0" );
	ri.Cvar_Set( "r_localExposure_shadowClamp", "0.25" );
	/* Filmic: whitePoint = scene value → display 1 (not a midtone darken divisor). */
	{
		cvar_t *tm = ri.Cvar_Get( "r_tonemap", "3", 0 );
		if ( !tm || tm->integer == 0 ) {
			ri.Cvar_Set( "r_tonemap", "3" );
		}
	}
	ri.Cvar_Set( "r_grade_toe", "0.10" );
	ri.Cvar_Set( "r_grade_shoulder", "0.30" );
	ri.Cvar_Set( "r_grade_whitePoint", "1.5" );
	ri.Cvar_Set( "r_grade_highlightDesat", "0.06" );
	ri.Cvar_Set( "r_grade_contrast", "1.0" );
	ri.Cvar_Set( "r_grade_contrastPivot", "0.18" );
	ri.Cvar_Set( "r_grade_vibrance", "0.08" );
	ri.Cvar_Set( "r_grade_blackClip", "0" );
	/* Reset adaptation so map load does not inherit a stale EV. */
	vk.adaptedExposure = ( r_exposure && r_exposure->value > 0.0f ) ? r_exposure->value : 1.0f;
	vk.temporal.hasValidLuminance = qfalse;
	vk.temporal.filteredAvgLogLuminance = 0.0f;

	wantAuto = ri.Cvar_Get( "r_skyboxHDR_autoExposure", "1", CVAR_ARCHIVE_ND );
	if ( wantAuto && wantAuto->integer ) {
		ri.Cvar_Set( "r_exposure_auto", "1" );
		ri.Printf( PRINT_ALL,
			"SkyboxHDR: eye adaptation ON (AE floor=0.70 target=0.18 skyW=0.35 filmic WP=1.5)\n" );
	} else {
		ri.Printf( PRINT_ALL,
			"SkyboxHDR: eye adaptation curve ready (manual EV; set r_skyboxHDR_autoExposure 1 to enable)\n" );
	}
}

qboolean SkyboxHDR_ConfigureFromMap( const char *path, float exposure, float rotation,
		float intensity, int projection ) {
	char buf[64];
	const skyboxHDR_t *cur;

	if ( !path || !path[0] ) {
		return qfalse;
	}

	if ( exposure > 0.0f ) {
		Com_sprintf( buf, sizeof( buf ), "%g", exposure );
		ri.Cvar_Set( "r_skyboxHDR_exposure", buf );
	}
	Com_sprintf( buf, sizeof( buf ), "%g", rotation );
	ri.Cvar_Set( "r_skyboxHDR_rotation", buf );
	if ( intensity > 0.0f ) {
		Com_sprintf( buf, sizeof( buf ), "%g", intensity );
		ri.Cvar_Set( "r_skyboxHDR_intensity", buf );
	}
	if ( projection >= 0 && projection < SKYBOX_PROJ_COUNT ) {
		Com_sprintf( buf, sizeof( buf ), "%d", projection );
		ri.Cvar_Set( "r_skyboxHDR_projection", buf );
	}

	ri.Cvar_Set( "r_skyboxHDR", path );
	/* Visible sky + IBL share the HDR owner when a map requests an EXR/HDR sky. */
	ri.Cvar_Set( "r_skyOwner", "2" );
	SkyboxHDR_EnableEyeAdaptation();

	/*
	 * Avoid a full unload/reload when BeginFrame already loaded this panorama
	 * (archived cvar / mapscript). Re-applying exposure still rebuilds display faces.
	 */
	cur = SkyboxHDR_Get();
	if ( cur && cur->loaded && !Q_stricmp( cur->filename, path ) &&
			skyboxPrefilteredImage.handle != VK_NULL_HANDLE ) {
		SkyboxHDR_SetExposure( r_skyboxHDR_exposure ? r_skyboxHDR_exposure->value : exposure );
		SkyboxHDR_SetRotation( r_skyboxHDR_rotation ? r_skyboxHDR_rotation->value : rotation );
		SkyboxHDR_SetIntensity( r_skyboxHDR_intensity ? r_skyboxHDR_intensity->value : intensity );
		SkyboxHDR_BuildDisplayFaces();
		if ( r_skyboxHDR ) {
			r_skyboxHDR->modified = qfalse;
		}
		if ( r_skyboxHDR_exposure ) r_skyboxHDR_exposure->modified = qfalse;
		if ( r_skyboxHDR_rotation ) r_skyboxHDR_rotation->modified = qfalse;
		if ( r_skyboxHDR_intensity ) r_skyboxHDR_intensity->modified = qfalse;
		if ( r_skyboxHDR_projection ) r_skyboxHDR_projection->modified = qfalse;
		return qtrue;
	}

	if ( r_skyboxHDR ) {
		r_skyboxHDR->modified = qtrue;
	}
	SkyboxHDR_UpdateRuntime();
	return SkyboxHDR_IsLoaded();
}
