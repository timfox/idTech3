/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Dynamic GI — compress many temporal lightmap states into a neural
feature atlas; decode at runtime (CPU default, optional compute) and patch
merged BSP lightmaps. See docs/NEURAL_DYNAMIC_GI.md.
===========================================================================
*/

#include "tr_local.h"
#include "tr_common.h"
#include "vk_ndgi.h"
#include "vk_texture_image.h"

#define NDGI_MANIFEST_VERSION   1
#define NDGI_MAGIC_WEIGHTS      0x3147444E /* 'NDG1' little-endian */
#define NDGI_MAX_STATES         16
#define NDGI_MAX_FEATURE_DIM    8
#define NDGI_MAX_HIDDEN         32
#define NDGI_MAX_TIME_KEYS      16
#define NDGI_MAX_PAGES          256
#define NDGI_PAGE_SIZE          128
#define NDGI_PAGE_BORDER        1
#define NDGI_PAGE_LEN           ( NDGI_PAGE_SIZE + NDGI_PAGE_BORDER * 2 )
#define NDGI_MAX_FEATURE_W      256
#define NDGI_MAX_FEATURE_H      256
#define NDGI_BC_BLOCK_BYTES     16

typedef struct {
	int         version;
	int         numStates;
	int         featureWidth;
	int         featureHeight;
	int         featureDim;
	int         hiddenDim;
	int         pageSize;
	int         vtPagesX;
	int         vtPagesY;
	int         bcCompress;
	float       timeKeys[NDGI_MAX_TIME_KEYS];
	int         numTimeKeys;
	char        featurePath[MAX_QPATH];
	char        weightsPath[MAX_QPATH];
} ndgiManifest_t;

typedef struct {
	qboolean    loaded;
	qboolean    procedural;
	char        mapName[MAX_QPATH];
	ndgiManifest_t man;

	float       *features;     /* [states][h][w][featureDim] */
	float       W1[NDGI_MAX_HIDDEN * NDGI_MAX_FEATURE_DIM];
	float       b1[NDGI_MAX_HIDDEN];
	float       W2[3 * NDGI_MAX_HIDDEN];
	float       b2[3];

	byte        *bcBlocks;     /* optional BC-style codebook blocks */
	int         bcBlockCount;

	byte        *pageBuffer;   /* NDGI_PAGE_LEN * NDGI_PAGE_LEN * 4 */
	qboolean    pageDirty[NDGI_MAX_PAGES];

	float       blendTime;
	int         lastUploadFrame;
} ndgiState_t;

static ndgiState_t ndgi;

static cvar_t *r_ndgi;
static cvar_t *r_ndgi_time;
static cvar_t *r_ndgi_cycle;
static cvar_t *r_ndgi_cyclePeriod;
static cvar_t *r_ndgi_blend;
static cvar_t *r_ndgi_vt;
static cvar_t *r_ndgi_bc;
static cvar_t *r_ndgi_compute;
static cvar_t *r_ndgi_debug;
static cvar_t *r_ndgi_states;

static float NDGI_Sigmoid( float x ) {
	return 1.0f / ( 1.0f + expf( -x ) );
}

static float NDGI_Relu( float x ) {
	return x > 0.0f ? x : 0.0f;
}

static void NDGI_ClearState( void ) {
	if ( ndgi.features ) {
		ri.Free( ndgi.features );
	}
	if ( ndgi.bcBlocks ) {
		ri.Free( ndgi.bcBlocks );
	}
	if ( ndgi.pageBuffer ) {
		ri.Free( ndgi.pageBuffer );
	}
	Com_Memset( &ndgi, 0, sizeof( ndgi ) );
}

static float NDGI_SampleTime( void ) {
	float t;

	if ( r_ndgi_time ) {
		t = r_ndgi_time->value;
	} else {
		t = 0.0f;
	}

	if ( r_ndgi_cycle && r_ndgi_cycle->integer ) {
		float period = r_ndgi_cyclePeriod ? r_ndgi_cyclePeriod->value : 120.0f;
		if ( period < 0.1f ) {
			period = 120.0f;
		}
		t = (float)( ri.Milliseconds() % (int)( period * 1000.0f ) ) / ( period * 1000.0f );
	}

	if ( t < 0.0f ) {
		t = 0.0f;
	}
	if ( t > 1.0f ) {
		t = 1.0f;
	}
	return t;
}

static void NDGI_StateIndicesForTime( float t, int *s0, int *s1, float *frac ) {
	int n = ndgi.man.numStates;
	int i;

	if ( n < 2 ) {
		*s0 = 0;
		*s1 = 0;
		*frac = 0.0f;
		return;
	}

	if ( ndgi.man.numTimeKeys >= 2 ) {
		for ( i = 0; i < ndgi.man.numTimeKeys - 1; i++ ) {
			if ( t >= ndgi.man.timeKeys[i] && t <= ndgi.man.timeKeys[i + 1] ) {
				float span = ndgi.man.timeKeys[i + 1] - ndgi.man.timeKeys[i];
				float f = span > 1e-6f ? ( t - ndgi.man.timeKeys[i] ) / span : 0.0f;
				*s0 = i % n;
				*s1 = ( i + 1 ) % n;
				*frac = f;
				return;
			}
		}
	}

	{
		float idx = t * (float)( n - 1 );
		int i0 = (int)idx;
		int i1 = i0 + 1;
		if ( i1 >= n ) {
			i1 = n - 1;
		}
		*s0 = i0;
		*s1 = i1;
		*frac = idx - (float)i0;
	}
}

static float *NDGI_FeatureAt( int state, int fx, int fy ) {
	const int w = ndgi.man.featureWidth;
	const int h = ndgi.man.featureHeight;
	const int d = ndgi.man.featureDim;
	int idx;

	if ( !ndgi.features || state < 0 || state >= ndgi.man.numStates ) {
		return NULL;
	}
	if ( fx < 0 || fy < 0 || fx >= w || fy >= h ) {
		return NULL;
	}
	idx = ( ( state * h + fy ) * w + fx ) * d;
	return ndgi.features + idx;
}

static void NDGI_MlpDecode( const float *feat, float *rgbOut ) {
	int hdim = ndgi.man.hiddenDim;
	int fdim = ndgi.man.featureDim;
	float hidden[NDGI_MAX_HIDDEN];
	int i, c, f;

	if ( !feat || hdim <= 0 || fdim <= 0 ) {
		rgbOut[0] = rgbOut[1] = rgbOut[2] = 0.5f;
		return;
	}

	for ( i = 0; i < hdim; i++ ) {
		float sum = ndgi.b1[i];
		for ( f = 0; f < fdim; f++ ) {
			sum += ndgi.W1[i * NDGI_MAX_FEATURE_DIM + f] * feat[f];
		}
		hidden[i] = NDGI_Relu( sum );
	}

	for ( c = 0; c < 3; c++ ) {
		float sum = ndgi.b2[c];
		for ( i = 0; i < hdim; i++ ) {
			sum += ndgi.W2[c * NDGI_MAX_HIDDEN + i] * hidden[i];
		}
		rgbOut[c] = NDGI_Sigmoid( sum );
	}
}

/*
===============
NDGI_DecodeBCBlock — 8-entry codebook, 16 pixels x 3-bit indices (BC-style)
===============
*/
static void NDGI_DecodeBCBlock( const byte *block, float *featOut, int featDim ) {
	uint8_t codebook[8];
	uint64_t indices = 0;
	int i, f;

	if ( featDim > 8 ) {
		featDim = 8;
	}

	for ( i = 0; i < 8; i++ ) {
		codebook[i] = block[i];
	}
	Com_Memcpy( &indices, block + 8, sizeof( indices ) );

	for ( f = 0; f < featDim; f++ ) {
		float acc = 0.0f;
		for ( i = 0; i < 16; i++ ) {
			int code = (int)( ( indices >> ( i * 3 ) ) & 7 );
			acc += (float)codebook[code] / 255.0f;
		}
		featOut[f] = acc / 16.0f;
	}
	for ( ; f < featDim; f++ ) {
		featOut[f] = 0.0f;
	}
}

static void NDGI_BlendFeatures( const float *a, const float *b, float blend, float *out ) {
	int f;
	for ( f = 0; f < ndgi.man.featureDim; f++ ) {
		out[f] = a[f] * ( 1.0f - blend ) + b[f] * blend;
	}
}

static void NDGI_BuildDefaultWeights( void ) {
	int h = ndgi.man.hiddenDim;
	int f = ndgi.man.featureDim;
	int i, c;

	Com_Memset( ndgi.W1, 0, sizeof( ndgi.W1 ) );
	Com_Memset( ndgi.b1, 0, sizeof( ndgi.b1 ) );
	Com_Memset( ndgi.W2, 0, sizeof( ndgi.W2 ) );
	Com_Memset( ndgi.b2, 0, sizeof( ndgi.b2 ) );

	for ( i = 0; i < h; i++ ) {
		ndgi.b1[i] = -0.1f;
		for ( int j = 0; j < f && j < 3; j++ ) {
			ndgi.W1[i * NDGI_MAX_FEATURE_DIM + j] = ( i == j ) ? 1.2f : 0.05f;
		}
	}
	for ( c = 0; c < 3; c++ ) {
		ndgi.b2[c] = 0.0f;
		for ( i = 0; i < h; i++ ) {
			ndgi.W2[c * NDGI_MAX_HIDDEN + i] = ( i % 3 == c ) ? 0.9f : 0.02f;
		}
	}
}

static void NDGI_GenerateProceduralFeatures( void ) {
	int s, y, x, f;
	const int w = ndgi.man.featureWidth;
	const int h = ndgi.man.featureHeight;
	const int d = ndgi.man.featureDim;
	const int n = ndgi.man.numStates;

	ndgi.features = ri.Malloc( (size_t)n * (size_t)w * (size_t)h * (size_t)d * sizeof( float ) );
	ndgi.procedural = qtrue;

	for ( s = 0; s < n; s++ ) {
		float phase = (float)s / (float)n;
		for ( y = 0; y < h; y++ ) {
			for ( x = 0; x < w; x++ ) {
				float *feat = NDGI_FeatureAt( s, x, y );
				float u = (float)x / (float)w;
				float v = (float)y / (float)h;
				for ( f = 0; f < d; f++ ) {
					switch ( f % 4 ) {
					case 0:
						feat[f] = 0.35f + 0.55f * sinf( ( u + phase ) * 6.28318f );
						break;
					case 1:
						feat[f] = 0.25f + 0.5f * cosf( ( v - phase * 0.5f ) * 6.28318f );
						break;
					case 2:
						feat[f] = 0.15f + 0.7f * phase;
						break;
					default:
						feat[f] = 0.4f + 0.2f * sinf( ( u + v + phase ) * 12.56636f );
						break;
					}
				}
			}
		}
	}
}

static qboolean NDGI_ParseManifest( const char *text, ndgiManifest_t *man ) {
	const char *parse;
	const char *token;
	char key[64];
	char value[MAX_TOKEN_CHARS];

	Com_Memset( man, 0, sizeof( *man ) );
	man->version = NDGI_MANIFEST_VERSION;
	man->numStates = 4;
	man->featureWidth = 64;
	man->featureHeight = 64;
	man->featureDim = 4;
	man->hiddenDim = 16;
	man->pageSize = NDGI_PAGE_SIZE;
	man->vtPagesX = 1;
	man->vtPagesY = 1;
	man->numTimeKeys = 0;

	parse = text;
	while ( 1 ) {
		token = COM_Parse( &parse );
		if ( !token[0] ) {
			break;
		}
		Q_strncpyz( key, token, sizeof( key ) );
		token = COM_Parse( &parse );
		if ( !token[0] ) {
			break;
		}
		Q_strncpyz( value, token, sizeof( value ) );

		if ( !Q_stricmp( key, "version" ) ) {
			man->version = atoi( value );
		} else if ( !Q_stricmp( key, "states" ) ) {
			man->numStates = atoi( value );
		} else if ( !Q_stricmp( key, "featureWidth" ) ) {
			man->featureWidth = atoi( value );
		} else if ( !Q_stricmp( key, "featureHeight" ) ) {
			man->featureHeight = atoi( value );
		} else if ( !Q_stricmp( key, "featureDim" ) ) {
			man->featureDim = atoi( value );
		} else if ( !Q_stricmp( key, "hiddenDim" ) ) {
			man->hiddenDim = atoi( value );
		} else if ( !Q_stricmp( key, "pageSize" ) ) {
			man->pageSize = atoi( value );
		} else if ( !Q_stricmp( key, "vtPagesX" ) ) {
			man->vtPagesX = atoi( value );
		} else if ( !Q_stricmp( key, "vtPagesY" ) ) {
			man->vtPagesY = atoi( value );
		} else if ( !Q_stricmp( key, "bc" ) ) {
			man->bcCompress = atoi( value );
		} else if ( !Q_stricmp( key, "featurePath" ) ) {
			Q_strncpyz( man->featurePath, value, sizeof( man->featurePath ) );
		} else if ( !Q_stricmp( key, "weightsPath" ) ) {
			Q_strncpyz( man->weightsPath, value, sizeof( man->weightsPath ) );
		} else if ( !Q_stricmp( key, "timeKeys" ) ) {
			char *tok = value;
			while ( man->numTimeKeys < NDGI_MAX_TIME_KEYS ) {
				man->timeKeys[man->numTimeKeys++] = (float)atof( tok );
				tok = strchr( tok, ' ' );
				if ( !tok ) {
					break;
				}
				while ( *tok == ' ' ) {
					tok++;
				}
				if ( !*tok ) {
					break;
				}
			}
		}
	}

	if ( man->numStates < 1 || man->numStates > NDGI_MAX_STATES ) {
		return qfalse;
	}
	if ( man->featureDim < 1 || man->featureDim > NDGI_MAX_FEATURE_DIM ) {
		return qfalse;
	}
	if ( man->hiddenDim < 1 || man->hiddenDim > NDGI_MAX_HIDDEN ) {
		return qfalse;
	}
	if ( man->featureWidth < 4 || man->featureHeight < 4 ||
		man->featureWidth > NDGI_MAX_FEATURE_W || man->featureHeight > NDGI_MAX_FEATURE_H ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean NDGI_LoadManifestFile( const char *mapBaseName ) {
	char path[MAX_QPATH];
	byte *buf;
	int len;
	const char *tryPaths[2];
	int i;

	tryPaths[0] = va( "maps/%s.ndgi", mapBaseName );
	tryPaths[1] = va( "ndgi/%s.ndgi", mapBaseName );

	for ( i = 0; i < 2; i++ ) {
		len = ri.FS_ReadFile( tryPaths[i], (void **)&buf );
		if ( len > 0 && buf ) {
			qboolean ok = NDGI_ParseManifest( (const char *)buf, &ndgi.man );
			ri.FS_FreeFile( buf );
			if ( ok ) {
				Q_strncpyz( path, tryPaths[i], sizeof( path ) );
				ri.Printf( PRINT_ALL, "[NDGI] Loaded manifest %s (states=%d %dx%d)\n",
					path, ndgi.man.numStates, ndgi.man.featureWidth, ndgi.man.featureHeight );
				return qtrue;
			}
		}
	}
	return qfalse;
}

static qboolean NDGI_LoadWeightsBinary( const char *path ) {
	byte *buf;
	int len;
	uint32_t magic;
	uint16_t fdim, hdim, nstates;
	float *dst;
	int count, i;

	if ( !path || !path[0] ) {
		return qfalse;
	}

	len = ri.FS_ReadFile( path, (void **)&buf );
	if ( len < 16 || !buf ) {
		return qfalse;
	}

	magic = *(uint32_t *)buf;
	if ( magic != NDGI_MAGIC_WEIGHTS ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	fdim = *(uint16_t *)( buf + 4 );
	hdim = *(uint16_t *)( buf + 6 );
	nstates = *(uint16_t *)( buf + 8 );

	if ( fdim != ndgi.man.featureDim || hdim != ndgi.man.hiddenDim ) {
		ri.Printf( PRINT_WARNING, "[NDGI] weights dim mismatch in %s\n", path );
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	(void)nstates;
	dst = (float *)( buf + 16 );
	count = ( hdim * fdim ) + hdim + ( 3 * hdim ) + 3;
	if ( len < 16 + (int)( count * sizeof( float ) ) ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	for ( i = 0; i < hdim * NDGI_MAX_FEATURE_DIM; i++ ) {
		ndgi.W1[i] = 0.0f;
	}
	Com_Memcpy( ndgi.W1, dst, hdim * fdim * sizeof( float ) );
	dst += hdim * fdim;
	Com_Memcpy( ndgi.b1, dst, hdim * sizeof( float ) );
	dst += hdim;
	Com_Memcpy( ndgi.W2, dst, 3 * hdim * sizeof( float ) );
	dst += 3 * hdim;
	Com_Memcpy( ndgi.b2, dst, 3 * sizeof( float ) );

	ri.FS_FreeFile( buf );
	ri.Printf( PRINT_ALL, "[NDGI] Loaded weights %s\n", path );
	return qtrue;
}

static qboolean NDGI_LoadFeatureRGBA( const char *path ) {
	byte *pic = NULL;
	int w = 0;
	int h = 0;
	int s, y, x, f;
	const int states = ndgi.man.numStates;
	const int fw = ndgi.man.featureWidth;
	const int fh = ndgi.man.featureHeight;
	const int fd = ndgi.man.featureDim;

	if ( !path || !path[0] ) {
		return qfalse;
	}

	R_LoadTGA( path, &pic, &w, &h );
	if ( !pic || w < 1 || h < 1 ) {
		return qfalse;
	}

	ndgi.features = ri.Malloc( (size_t)states * (size_t)fw * (size_t)fh * (size_t)fd * sizeof( float ) );

	for ( s = 0; s < states; s++ ) {
		int sliceH = h / states;
		int y0 = s * sliceH;
		if ( sliceH < 1 ) {
			sliceH = 1;
		}
		for ( y = 0; y < fh; y++ ) {
			int sy = y0 + ( y * sliceH ) / fh;
			if ( sy >= h ) {
				sy = h - 1;
			}
			for ( x = 0; x < fw; x++ ) {
				int sx = ( x * w ) / fw;
				if ( sx >= w ) {
					sx = w - 1;
				}
				byte *px = pic + ( sy * w + sx ) * 4;
				float *feat = NDGI_FeatureAt( s, x, y );
				for ( f = 0; f < fd; f++ ) {
					feat[f] = px[f % 4] / 255.0f;
				}
			}
		}
	}

	ri.Free( pic );
	ri.Printf( PRINT_ALL, "[NDGI] Loaded feature atlas %s (%dx%d)\n", path, w, h );
	return qtrue;
}

static void NDGI_MarkAllPagesDirty( void ) {
	int pages = ndgi.man.vtPagesX * ndgi.man.vtPagesY;
	int i;
	if ( pages > NDGI_MAX_PAGES ) {
		pages = NDGI_MAX_PAGES;
	}
	for ( i = 0; i < pages; i++ ) {
		ndgi.pageDirty[i] = qtrue;
	}
}

static void NDGI_DecodePage( int pageX, int pageY, float t, float blendExtra ) {
	int s0, s1;
	float frac;
	float feat[NDGI_MAX_FEATURE_DIM];
	float rgb[3];
	int px, py;
	int fw = ndgi.man.featureWidth;
	int fh = ndgi.man.featureHeight;

	if ( !ndgi.pageBuffer ) {
		return;
	}

	NDGI_StateIndicesForTime( t, &s0, &s1, &frac );
	frac *= ( r_ndgi_blend ? r_ndgi_blend->value : 1.0f );
	if ( blendExtra > 0.0f ) {
		frac = frac * ( 1.0f - blendExtra ) + blendExtra * 0.5f;
	}

	for ( py = 0; py < NDGI_PAGE_LEN; py++ ) {
		for ( px = 0; px < NDGI_PAGE_LEN; px++ ) {
			int lx = px - NDGI_PAGE_BORDER;
			int ly = py - NDGI_PAGE_BORDER;
			float u, v;
			int fx, fy;
			float *fa, *fb;

			if ( lx < 0 || ly < 0 || lx >= NDGI_PAGE_SIZE || ly >= NDGI_PAGE_SIZE ) {
				byte *dst = ndgi.pageBuffer + ( py * NDGI_PAGE_LEN + px ) * 4;
				dst[0] = dst[1] = dst[2] = 32;
				dst[3] = 255;
				continue;
			}

			u = ( (float)pageX + (float)lx / (float)NDGI_PAGE_SIZE ) / (float)ndgi.man.vtPagesX;
			v = ( (float)pageY + (float)ly / (float)NDGI_PAGE_SIZE ) / (float)ndgi.man.vtPagesY;
			fx = (int)( u * (float)( fw - 1 ) );
			fy = (int)( v * (float)( fh - 1 ) );
			if ( fx < 0 ) {
				fx = 0;
			}
			if ( fy < 0 ) {
				fy = 0;
			}

			if ( ndgi.man.bcCompress && ndgi.bcBlocks ) {
				int blocksX = ( fw + 3 ) / 4;
				int bx = fx / 4;
				int by = fy / 4;
				int bidx = by * blocksX + bx;
				byte block[NDGI_BC_BLOCK_BYTES];
				if ( bidx >= 0 && bidx < ndgi.bcBlockCount ) {
					Com_Memcpy( block, ndgi.bcBlocks + bidx * NDGI_BC_BLOCK_BYTES, NDGI_BC_BLOCK_BYTES );
					NDGI_DecodeBCBlock( block, feat, ndgi.man.featureDim );
				} else {
					Com_Memset( feat, 0, sizeof( feat ) );
				}
			} else {
				fa = NDGI_FeatureAt( s0, fx, fy );
				fb = NDGI_FeatureAt( s1, fx, fy );
				if ( fa && fb ) {
					NDGI_BlendFeatures( fa, fb, frac, feat );
				} else if ( fa ) {
					Com_Memcpy( feat, fa, ndgi.man.featureDim * sizeof( float ) );
				} else {
					Com_Memset( feat, 0, sizeof( feat ) );
				}
			}

			NDGI_MlpDecode( feat, rgb );
			{
				byte *dst = ndgi.pageBuffer + ( py * NDGI_PAGE_LEN + px ) * 4;
				dst[0] = (byte)( rgb[0] * 255.0f );
				dst[1] = (byte)( rgb[1] * 255.0f );
				dst[2] = (byte)( rgb[2] * 255.0f );
				dst[3] = 255;
			}
		}
	}
}

static void NDGI_UploadPageToLightmaps( int pageX, int pageY ) {
	int pageIdx = pageY * ndgi.man.vtPagesX + pageX;
	int atlasIndex;
	int pixelX;
	int pixelY;

	if ( !tr.lightmaps || tr.numLightmaps < 1 ) {
		return;
	}

	atlasIndex = R_GetLightmapPixelOffset( pageIdx, &pixelX, &pixelY );
	if ( atlasIndex < 0 || atlasIndex >= tr.numLightmaps || !tr.lightmaps[atlasIndex] ) {
		return;
	}

	vk_upload_image_data( tr.lightmaps[atlasIndex],
		pixelX, pixelY,
		NDGI_PAGE_LEN, NDGI_PAGE_LEN,
		1, ndgi.pageBuffer, NDGI_PAGE_LEN * NDGI_PAGE_LEN * 4, qtrue );
}

static void NDGI_UpdateVirtualTexture( float t ) {
	int px, py;
	int pagesX = ndgi.man.vtPagesX;
	int pagesY = ndgi.man.vtPagesY;
	qboolean vt = r_ndgi_vt && r_ndgi_vt->integer;

	if ( pagesX < 1 ) {
		pagesX = 1;
	}
	if ( pagesY < 1 ) {
		pagesY = 1;
	}

	for ( py = 0; py < pagesY; py++ ) {
		for ( px = 0; px < pagesX; px++ ) {
			int idx = py * pagesX + px;
			if ( idx >= NDGI_MAX_PAGES ) {
				break;
			}
			if ( vt && !ndgi.pageDirty[idx] ) {
				continue;
			}
			NDGI_DecodePage( px, py, t, 0.0f );
			NDGI_UploadPageToLightmaps( px, py );
			ndgi.pageDirty[idx] = qfalse;
		}
	}
}

static void NDGI_ApplyDefaultsFromCvars( void ) {
	if ( r_ndgi_states && r_ndgi_states->integer > 0 ) {
		ndgi.man.numStates = r_ndgi_states->integer;
		if ( ndgi.man.numStates > NDGI_MAX_STATES ) {
			ndgi.man.numStates = NDGI_MAX_STATES;
		}
	}
	if ( r_ndgi_bc && r_ndgi_bc->integer ) {
		ndgi.man.bcCompress = 1;
	}
}

static void NDGI_Cmd_Reload( void ) {
	if ( ndgi.mapName[0] ) {
		R_NDGI_OnMapLoad( ndgi.mapName );
	}
}

static void NDGI_Cmd_Status( void ) {
	ri.Printf( PRINT_ALL, "[NDGI] active=%d loaded=%d procedural=%d map=%s states=%d vt=%dx%d bc=%d\n",
		R_NDGI_Active() ? 1 : 0,
		ndgi.loaded ? 1 : 0,
		ndgi.procedural ? 1 : 0,
		ndgi.mapName[0] ? ndgi.mapName : "(none)",
		ndgi.man.numStates,
		ndgi.man.vtPagesX, ndgi.man.vtPagesY,
		ndgi.man.bcCompress );
}

void R_NDGI_Init( void ) {
	r_ndgi = ri.Cvar_Get( "r_ndgi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ndgi_time = ri.Cvar_Get( "r_ndgi_time", "0", CVAR_ARCHIVE_ND );
	r_ndgi_cycle = ri.Cvar_Get( "r_ndgi_cycle", "0", CVAR_ARCHIVE_ND );
	r_ndgi_cyclePeriod = ri.Cvar_Get( "r_ndgi_cyclePeriod", "120", CVAR_ARCHIVE_ND );
	r_ndgi_blend = ri.Cvar_Get( "r_ndgi_blend", "1", CVAR_ARCHIVE_ND );
	r_ndgi_vt = ri.Cvar_Get( "r_ndgi_vt", "1", CVAR_ARCHIVE_ND );
	r_ndgi_bc = ri.Cvar_Get( "r_ndgi_bc", "0", CVAR_ARCHIVE_ND );
	r_ndgi_compute = ri.Cvar_Get( "r_ndgi_compute", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ndgi_debug = ri.Cvar_Get( "r_ndgi_debug", "0", CVAR_ARCHIVE_ND );
	r_ndgi_states = ri.Cvar_Get( "r_ndgi_states", "4", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_ndgi, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_ndgi_compute, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_ndgi_vt, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_ndgi_bc, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ndgi,
		"Neural Dynamic GI: temporal baked lightmaps from neural feature atlas (0=off, 1=on)." );
	ri.Cvar_SetDescription( r_ndgi_time,
		"Normalized time [0,1] for blending temporal lightmap states (day/night, weather)." );
	ri.Cvar_SetDescription( r_ndgi_cycle,
		"Auto-advance r_ndgi_time over r_ndgi_cyclePeriod seconds." );
	ri.Cvar_SetDescription( r_ndgi_vt,
		"Virtual texture paging: decode only dirty pages when time changes (1=on)." );

	ri.Cmd_AddCommand( "ndgi_reload", NDGI_Cmd_Reload );
	ri.Cmd_AddCommand( "ndgi_status", NDGI_Cmd_Status );

	if ( r_ndgi->integer ) {
		ri.Printf( PRINT_ALL,
			"[NDGI] Neural Dynamic GI enabled (experimental). See docs/NEURAL_DYNAMIC_GI.md\n" );
	}
	if ( r_ndgi_compute->integer ) {
		ri.Printf( PRINT_ALL,
			"[NDGI] r_ndgi_compute=1: GPU decompress shader compiled; CPU VT decode remains default (see docs/NEURAL_RENDERER_PHASES.md).\n" );
	}
}

void R_NDGI_Shutdown( void ) {
	ri.Cmd_RemoveCommand( "ndgi_reload" );
	ri.Cmd_RemoveCommand( "ndgi_status" );
	NDGI_ClearState();
}

qboolean R_NDGI_Active( void ) {
	return ( r_ndgi && r_ndgi->integer && ndgi.loaded && tr.lightmaps && tr.numLightmaps > 0 ) ? qtrue : qfalse;
}

void R_NDGI_OnMapLoad( const char *mapBaseName ) {
	char weightsPath[MAX_QPATH];

	NDGI_ClearState();

	if ( !r_ndgi || !r_ndgi->integer ) {
		return;
	}
	if ( r_vertexLight && r_vertexLight->integer ) {
		ri.Printf( PRINT_WARNING, "[NDGI] r_vertexLight=1 — no lightmaps; NDGI inactive\n" );
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}

	Q_strncpyz( ndgi.mapName, mapBaseName, sizeof( ndgi.mapName ) );

	if ( !NDGI_LoadManifestFile( mapBaseName ) ) {
		ri.Printf( PRINT_ALL, "[NDGI] No manifest for '%s' — using procedural demo features\n", mapBaseName );
		Com_Memset( &ndgi.man, 0, sizeof( ndgi.man ) );
		ndgi.man.numStates = 4;
		ndgi.man.featureWidth = 64;
		ndgi.man.featureHeight = 64;
		ndgi.man.featureDim = 4;
		ndgi.man.hiddenDim = 16;
		ndgi.man.pageSize = NDGI_PAGE_SIZE;
		ndgi.man.vtPagesX = 1;
		ndgi.man.vtPagesY = 1;
		ndgi.man.timeKeys[0] = 0.0f;
		ndgi.man.timeKeys[1] = 0.33f;
		ndgi.man.timeKeys[2] = 0.66f;
		ndgi.man.timeKeys[3] = 1.0f;
		ndgi.man.numTimeKeys = 4;
	}

	NDGI_ApplyDefaultsFromCvars();

	if ( !NDGI_LoadFeatureRGBA( ndgi.man.featurePath ) ) {
		NDGI_GenerateProceduralFeatures();
	}

	if ( ndgi.man.weightsPath[0] ) {
		Q_strncpyz( weightsPath, ndgi.man.weightsPath, sizeof( weightsPath ) );
	} else {
		Com_sprintf( weightsPath, sizeof( weightsPath ), "ndgi/%s.ndgib", mapBaseName );
	}

	if ( !NDGI_LoadWeightsBinary( weightsPath ) ) {
		NDGI_BuildDefaultWeights();
	}

	ndgi.pageBuffer = ri.Malloc( NDGI_PAGE_LEN * NDGI_PAGE_LEN * 4 );
	ndgi.loaded = qtrue;
	NDGI_MarkAllPagesDirty();

	ri.Printf( PRINT_ALL,
		"[NDGI] Ready on '%s': %d states, feature %dx%dx%d, VT %dx%d pages, %s\n",
		mapBaseName, ndgi.man.numStates,
		ndgi.man.featureWidth, ndgi.man.featureHeight, ndgi.man.featureDim,
		ndgi.man.vtPagesX, ndgi.man.vtPagesY,
		ndgi.procedural ? "procedural" : "authored" );
}

void R_NDGI_FrameUpdate( void ) {
	float t;
	float period;

	if ( !R_NDGI_Active() ) {
		return;
	}

	t = NDGI_SampleTime();

	if ( r_ndgi_cycle && r_ndgi_cycle->integer ) {
		period = r_ndgi_cyclePeriod ? r_ndgi_cyclePeriod->value : 120.0f;
		if ( period < 0.1f ) {
			period = 120.0f;
		}
	} else if ( r_ndgi_time && r_ndgi_time->modified ) {
		NDGI_MarkAllPagesDirty();
		r_ndgi_time->modified = qfalse;
	}

	if ( fabsf( t - ndgi.blendTime ) > 1e-4f ) {
		NDGI_MarkAllPagesDirty();
		ndgi.blendTime = t;
	}

	NDGI_UpdateVirtualTexture( t );

	if ( r_ndgi_debug && r_ndgi_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[NDGI] t=%.3f frame=%d\n", t, tr.frameCount );
	}
}
