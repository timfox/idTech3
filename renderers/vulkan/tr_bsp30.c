/*
===========================================================================
BSP30 / Half-Life BSP v30 renderer bridge.

BSP30 faces are reconstructed from edges and surfedges and submitted as
ordinary idTech3 planar surfaces. Embedded indexed textures are expanded to
RGBA at load time. Lighting lump samples are converted to per-vertex colors
(GoldSrc luxel grid, style 0) so surfaces are not fullbright albedo.

This is an independent binary-format implementation. It does not include or
depend on Half-Life SDK source, headers, or libraries.
===========================================================================
*/

#include "tr_local.h"
#include "../../engine/core/qfiles_bsp30.h"
#include "../common/tr_bsp30_triangulate.h"
#include "vk_skybox_hdr.h"

typedef struct {
	const byte *base;
	int size;
	const bsp30_header_t *header;
	world_t *world;
	shader_t **textureShaders;
	int *textureWidths;
	int *textureHeights;
	int numTextures;
	shader_t *skyShader;
	char skyname[MAX_QPATH];
	char skyboxHdr[MAX_QPATH];
	float skyboxHdrExposure;
	float skyboxHdrRotation;
	float skyboxHdrIntensity;
	float skyboxHdrVisibleEV;
	float skyboxHdrLuminanceScale;
	int skyboxHdrProjection;
	int skyboxHdrFaceSize;
} bsp30RenderLoad_t;

#define BSP30_MAX_WADS 16
#define BSP30_TEX_SPECIAL 1
/* GoldSrc / Quake luxel spacing (world units per lightmap texel). */
#define BSP30_LUXEL_SIZE 16.0f
#define BSP30_MAX_LUXEL_DIM 256

static int s_bsp30LitFaces;
static int s_bsp30WhiteFaces;
static int s_bsp30SpecialFaces;

typedef struct {
	byte *data;
	int length;
	char name[MAX_QPATH];
} bsp30WadFile_t;

static int GS_LumpLength( const bsp30RenderLoad_t *load, int lump ) {
	return LittleLong( load->header->lumps[lump].filelen );
}

static const byte *GS_LumpData( const bsp30RenderLoad_t *load, int lump ) {
	return load->base + LittleLong( load->header->lumps[lump].fileofs );
}

static int GS_LumpCount( const bsp30RenderLoad_t *load, int lump, int elementSize ) {
	int length = GS_LumpLength( load, lump );
	if ( elementSize <= 0 || length % elementSize ) {
		ri.Error( ERR_DROP, "%s: malformed BSP30 lump %d", __func__, lump );
	}
	return length / elementSize;
}

static void GS_ValidateHeader( const bsp30RenderLoad_t *load, const char *mapname ) {
	int i;

	if ( load->size < (int)sizeof( bsp30_header_t ) ||
			LittleLong( load->header->version ) != BSP30_BSP_VERSION ) {
		ri.Error( ERR_DROP, "%s: %s is not a BSP30 BSP v30 map", __func__, mapname );
	}

	for ( i = 0; i < BSP30_HEADER_LUMPS; i++ ) {
		int offset = LittleLong( load->header->lumps[i].fileofs );
		int length = LittleLong( load->header->lumps[i].filelen );
		if ( offset < 0 || length < 0 || offset > load->size || length > load->size - offset ) {
			ri.Error( ERR_DROP, "%s: %s has invalid BSP30 lump %d", __func__, mapname, i );
		}
	}
}

static void GS_TextureName( char *out, int outSize, const bsp30_miptex_t *miptex, int index ) {
	char raw[17];
	Com_Memcpy( raw, miptex->name, 16 );
	raw[16] = '\0';
	if ( raw[0] ) {
		Com_sprintf( out, outSize, "*bsp30/%s", raw );
	}
	else {
		Com_sprintf( out, outSize, "*bsp30/texture_%d", index );
	}
}

static qboolean GS_MiptexPixelsValid( const bsp30_miptex_t *miptex, int available ) {
	uint32_t width = LittleLong( miptex->width );
	uint32_t height = LittleLong( miptex->height );
	uint32_t pixelOffset = LittleLong( miptex->offsets[0] );
	uint64_t pixelCount = (uint64_t)width * height;
	uint64_t paletteOffset = (uint64_t)LittleLong( miptex->offsets[3] ) + pixelCount / 64 + 2;

	return width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
			pixelOffset > 0 && pixelCount <= INT_MAX &&
			(uint64_t)pixelOffset + pixelCount <= (uint64_t)available &&
			paletteOffset + 256 * 3 <= (uint64_t)available;
}

static image_t *GS_CreateTextureImage( const char *shaderName,
		const bsp30_miptex_t *miptex, int available ) {
	uint32_t width = LittleLong( miptex->width );
	uint32_t height = LittleLong( miptex->height );
	uint32_t pixelOffset = LittleLong( miptex->offsets[0] );
	uint64_t pixelCount = (uint64_t)width * height;
	uint64_t paletteOffset = (uint64_t)LittleLong( miptex->offsets[3] ) + pixelCount / 64 + 2;
	const byte *pixels;
	const byte *palette;
	byte *rgba;
	image_t *image;
	int p;

	if ( !GS_MiptexPixelsValid( miptex, available ) ) {
		return NULL;
	}

	pixels = (const byte *)miptex + pixelOffset;
	palette = (const byte *)miptex + paletteOffset;
	rgba = ri.Hunk_AllocateTempMemory( (int)pixelCount * 4 );
	for ( p = 0; p < (int)pixelCount; p++ ) {
		int colorIndex = pixels[p];
		rgba[p * 4 + 0] = palette[colorIndex * 3 + 0];
		rgba[p * 4 + 1] = palette[colorIndex * 3 + 1];
		rgba[p * 4 + 2] = palette[colorIndex * 3 + 2];
		rgba[p * 4 + 3] = ( miptex->name[0] == '{' && colorIndex == 255 ) ? 0 : 255;
	}

	image = R_CreateImage( shaderName, NULL, rgba, (int)width, (int)height,
			IMGFLAG_MIPMAP | IMGFLAG_NO_COMPRESSION | IMGFLAG_NOLIGHTSCALE,
			VK_FORMAT_R8G8B8A8_SRGB, 0 );
	ri.Hunk_FreeTempMemory( rgba );
	return image;
}

static image_t *GS_CreateFallbackImage( const char *shaderName, const char *textureName ) {
	byte rgba[64 * 64 * 4];
	int x, y;

	/*
	 * Missing content must not masquerade as authored color.  The previous
	 * name-hashed palette produced large pastel surfaces which skewed scene
	 * exposure and made a missing asset look like a color-management fault.
	 * Keep luminance bounded and hue-neutral; reserve magenta for the border.
	 */
	(void)textureName;
	for ( y = 0; y < 64; y++ ) {
		for ( x = 0; x < 64; x++ ) {
			int p = ( y * 64 + x ) * 4;
			int checker = ( ( x >> 3 ) ^ ( y >> 3 ) ) & 1;
			int grid = ( x == 0 || y == 0 || x == 63 || y == 63 );
			byte neutral = checker ? 48 : 24;
			rgba[p + 0] = grid ? 255 : neutral;
			rgba[p + 1] = grid ? 0 : neutral;
			rgba[p + 2] = grid ? 255 : neutral;
			rgba[p + 3] = 255;
		}
	}
	return R_CreateImage( shaderName, NULL, rgba, 64, 64,
			IMGFLAG_MIPMAP | IMGFLAG_NO_COMPRESSION | IMGFLAG_NOLIGHTSCALE,
			VK_FORMAT_R8G8B8A8_SRGB, 0 );
}

static const char *GS_BaseName( const char *path ) {
	const char *base = path;
	while ( *path ) {
		if ( *path == '/' || *path == '\\' ) {
			base = path + 1;
		}
		path++;
	}
	return base;
}

/*
 * Parse worldspawn keys used for sky / HDR environment.
 * Half-Life uses "skyname"; editor bridge adds skybox_hdr*.
 */
static void GS_ParseWorldspawnSky( bsp30RenderLoad_t *load ) {
	const byte *entities = GS_LumpData( load, BSP30_LUMP_ENTITIES );
	int entityLength = GS_LumpLength( load, BSP30_LUMP_ENTITIES );
	char *entityText;
	const char *parse;
	const char *token;

	load->skyboxHdrExposure = 1.0f;
	load->skyboxHdrRotation = 0.0f;
	load->skyboxHdrIntensity = 1.0f;
	load->skyboxHdrVisibleEV = 1.0f;
	load->skyboxHdrLuminanceScale = 1.0f;
	load->skyboxHdrProjection = 0;
	load->skyboxHdrFaceSize = 2048;
	load->skyname[0] = '\0';
	load->skyboxHdr[0] = '\0';
	load->skyShader = NULL;

	if ( entityLength <= 0 ) {
		return;
	}

	entityText = ri.Hunk_AllocateTempMemory( entityLength + 1 );
	Com_Memcpy( entityText, entities, entityLength );
	entityText[entityLength] = '\0';
	parse = entityText;

	if ( *( token = COM_Parse( &parse ) ) == '{' ) {
		while ( parse && *( token = COM_Parse( &parse ) ) && token[0] != '}' ) {
			char key[MAX_TOKEN_CHARS];
			Q_strncpyz( key, token, sizeof( key ) );
			token = COM_Parse( &parse );
			if ( !token || !token[0] ) {
				break;
			}
			if ( !Q_stricmp( key, "skyname" ) || !Q_stricmp( key, "sky" ) ) {
				Q_strncpyz( load->skyname, token, sizeof( load->skyname ) );
			} else if ( !Q_stricmp( key, "skybox_hdr" ) ) {
				Q_strncpyz( load->skyboxHdr, token, sizeof( load->skyboxHdr ) );
			} else if ( !Q_stricmp( key, "skybox_hdr_exposure" ) ) {
				load->skyboxHdrExposure = Q_atof( token );
			} else if ( !Q_stricmp( key, "skybox_hdr_rotation" ) ) {
				load->skyboxHdrRotation = Q_atof( token );
			} else if ( !Q_stricmp( key, "skybox_hdr_intensity" ) ) {
				load->skyboxHdrIntensity = Q_atof( token );
			} else if ( !Q_stricmp( key, "skybox_hdr_projection" ) ) {
				load->skyboxHdrProjection = atoi( token );
			} else if ( !Q_stricmp( key, "skybox_hdr_visible_ev" ) ) {
				load->skyboxHdrVisibleEV = Q_atof( token );
			} else if ( !Q_stricmp( key, "skybox_hdr_luminance_scale" ) ) {
				load->skyboxHdrLuminanceScale = Q_atof( token );
			} else if ( !Q_stricmp( key, "skybox_hdr_face_size" ) ) {
				load->skyboxHdrFaceSize = atoi( token );
			}
		}
	}
	ri.Hunk_FreeTempMemory( entityText );
}

/*
 * Optional maps/<map>.skybox_hdr sidecar (applied when worldspawn has no skybox_hdr).
 * First non-empty line: panorama path. Optional lines:
 * exposure/rotation/intensity/projection/visible_ev/luminance_scale/face_size <value>
 */
static void GS_TryLoadSkyboxSidecar( bsp30RenderLoad_t *load, const char *mapname ) {
	char sidecar[MAX_QPATH];
	char *text = NULL;
	const char *parse;
	const char *token;
	int len;
	const char *base;
	const char *slash;

	if ( load->skyboxHdr[0] || !mapname || !mapname[0] ) {
		return;
	}

	Q_strncpyz( sidecar, mapname, sizeof( sidecar ) );
	COM_StripExtension( sidecar, sidecar, sizeof( sidecar ) );
	Q_strcat( sidecar, sizeof( sidecar ), ".skybox_hdr" );

	len = ri.FS_ReadFile( sidecar, (void **)&text );
	if ( len <= 0 || !text ) {
		/* Also try basename under maps/ if mapname was not maps/... */
		slash = strrchr( mapname, '/' );
		base = slash ? slash + 1 : mapname;
		Com_sprintf( sidecar, sizeof( sidecar ), "maps/%s", base );
		COM_StripExtension( sidecar, sidecar, sizeof( sidecar ) );
		Q_strcat( sidecar, sizeof( sidecar ), ".skybox_hdr" );
		len = ri.FS_ReadFile( sidecar, (void **)&text );
		if ( len <= 0 || !text ) {
			return;
		}
	}

	parse = text;
	while ( 1 ) {
		token = COM_Parse( &parse );
		if ( !token || !token[0] ) {
			break;
		}
		/* Skip # comments (rest of line). */
		if ( token[0] == '#' ) {
			while ( parse && *parse && *parse != '\n' ) {
				parse++;
			}
			continue;
		}
		if ( !load->skyboxHdr[0] && Q_stricmp( token, "exposure" ) &&
				Q_stricmp( token, "rotation" ) && Q_stricmp( token, "intensity" ) &&
				Q_stricmp( token, "projection" ) && Q_stricmp( token, "visible_ev" ) &&
				Q_stricmp( token, "luminance_scale" ) && Q_stricmp( token, "face_size" ) ) {
			Q_strncpyz( load->skyboxHdr, token, sizeof( load->skyboxHdr ) );
			continue;
		}
		if ( !Q_stricmp( token, "exposure" ) ) {
			token = COM_Parse( &parse );
			if ( token && token[0] ) {
				load->skyboxHdrExposure = Q_atof( token );
			}
		} else if ( !Q_stricmp( token, "rotation" ) ) {
			token = COM_Parse( &parse );
			if ( token && token[0] ) {
				load->skyboxHdrRotation = Q_atof( token );
			}
		} else if ( !Q_stricmp( token, "intensity" ) ) {
			token = COM_Parse( &parse );
			if ( token && token[0] ) {
				load->skyboxHdrIntensity = Q_atof( token );
			}
		} else if ( !Q_stricmp( token, "projection" ) ) {
			token = COM_Parse( &parse );
			if ( token && token[0] ) {
				load->skyboxHdrProjection = atoi( token );
			}
		} else if ( !Q_stricmp( token, "visible_ev" ) ) {
			token = COM_Parse( &parse );
			if ( token && token[0] ) {
				load->skyboxHdrVisibleEV = Q_atof( token );
			}
		} else if ( !Q_stricmp( token, "luminance_scale" ) ) {
			token = COM_Parse( &parse );
			if ( token && token[0] ) {
				load->skyboxHdrLuminanceScale = Q_atof( token );
			}
		} else if ( !Q_stricmp( token, "face_size" ) ) {
			token = COM_Parse( &parse );
			if ( token && token[0] ) {
				load->skyboxHdrFaceSize = atoi( token );
			}
		}
	}

	ri.FS_FreeFile( text );
	if ( load->skyboxHdr[0] ) {
		ri.Printf( PRINT_ALL,
			"...BSP30 skybox sidecar %s -> %s (exposure=%g rotation=%g intensity=%g projection=%d visibleEV=%g luminanceScale=%g faceSize=%d)\n",
			sidecar, load->skyboxHdr, load->skyboxHdrExposure, load->skyboxHdrRotation,
			load->skyboxHdrIntensity, load->skyboxHdrProjection, load->skyboxHdrVisibleEV,
			load->skyboxHdrLuminanceScale, load->skyboxHdrFaceSize );
	}
}

static image_t *GS_FindEnvSkyFace( const char *skyname, const char *suf ) {
	char pathname[MAX_QPATH];
	image_t *image;
	imgFlags_t flags = IMGFLAG_CLAMPTOEDGE | IMGFLAG_MIPMAP;

	/* Half-Life: gfx/env/<name>rt.tga (no underscore). */
	Com_sprintf( pathname, sizeof( pathname ), "gfx/env/%s%s", skyname, suf );
	image = R_FindImageFile( pathname, flags, 0 );
	if ( image ) {
		return image;
	}

	/* Quake 3 style: gfx/env/<name>_rt.tga */
	Com_sprintf( pathname, sizeof( pathname ), "gfx/env/%s_%s", skyname, suf );
	image = R_FindImageFile( pathname, flags, 0 );
	if ( image ) {
		return image;
	}

	Com_sprintf( pathname, sizeof( pathname ), "env/%s%s", skyname, suf );
	image = R_FindImageFile( pathname, flags, 0 );
	if ( image ) {
		return image;
	}

	Com_sprintf( pathname, sizeof( pathname ), "env/%s_%s", skyname, suf );
	return R_FindImageFile( pathname, flags, 0 );
}

static shader_t *GS_CreateSkyShader( bsp30RenderLoad_t *load ) {
	static const char *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	image_t *faces[6];
	int i;
	qboolean any = qfalse;

	Com_Memset( faces, 0, sizeof( faces ) );

	if ( SkyboxHDR_IsLoaded() ) {
		SkyboxHDR_BuildDisplayFaces();
		for ( i = 0; i < 6; i++ ) {
			faces[i] = SkyboxHDR_GetDisplayFace( i );
			if ( faces[i] ) {
				any = qtrue;
			}
		}
		if ( any ) {
			ri.Printf( PRINT_ALL, "...BSP30 sky: HDR/OpenEXR display faces\n" );
			return R_CreateSkyShaderFromFaces( "*bsp30/sky_hdr", faces );
		}
	}

	if ( load->skyname[0] ) {
		for ( i = 0; i < 6; i++ ) {
			faces[i] = GS_FindEnvSkyFace( load->skyname, suf[i] );
			if ( faces[i] ) {
				any = qtrue;
			} else {
				faces[i] = tr.defaultImage;
			}
		}
		if ( any ) {
			ri.Printf( PRINT_ALL, "...BSP30 sky: env faces for skyname '%s'\n", load->skyname );
			return R_CreateSkyShaderFromFaces( va( "*bsp30/sky_%s", load->skyname ), faces );
		}
		ri.Printf( PRINT_WARNING, "...BSP30 sky: skyname '%s' env faces not found\n", load->skyname );
	}

	/* Minimal sky so sky brushes still take the sky iterator (solid/fastsky clear). */
	for ( i = 0; i < 6; i++ ) {
		faces[i] = tr.defaultImage;
	}
	return R_CreateSkyShaderFromFaces( "*bsp30/sky", faces );
}

static qboolean GS_IsSkyTextureName( const char *textureName ) {
	if ( !textureName || !textureName[0] ) {
		return qfalse;
	}
	/* GoldSrc sky brushes use the texture named "sky" (or sky*). */
	return ( !Q_stricmpn( textureName, "sky", 3 ) ) ? qtrue : qfalse;
}

static int GS_LoadReferencedWads( const bsp30RenderLoad_t *load,
		bsp30WadFile_t wads[BSP30_MAX_WADS] ) {
	const byte *entities = GS_LumpData( load, BSP30_LUMP_ENTITIES );
	int entityLength = GS_LumpLength( load, BSP30_LUMP_ENTITIES );
	char *entityText = ri.Hunk_AllocateTempMemory( entityLength + 1 );
	const char *parse;
	const char *token;
	char wadList[MAX_STRING_CHARS] = "";
	int count = 0;
	int i;

	Com_Memcpy( entityText, entities, entityLength );
	entityText[entityLength] = '\0';
	parse = entityText;
	if ( *( token = COM_Parse( &parse ) ) == '{' ) {
		while ( parse && *( token = COM_Parse( &parse ) ) && token[0] != '}' ) {
			char key[MAX_TOKEN_CHARS];
			Q_strncpyz( key, token, sizeof( key ) );
			token = COM_Parse( &parse );
			if ( !Q_stricmp( key, "wad" ) ) {
				Q_strncpyz( wadList, token, sizeof( wadList ) );
			}
		}
	}
	ri.Hunk_FreeTempMemory( entityText );

	for ( parse = wadList; *parse && count < BSP30_MAX_WADS; ) {
		char entry[MAX_OSPATH];
		char qpath[MAX_QPATH];
		const char *base;
		int len = 0;
		qboolean duplicate = qfalse;

		while ( *parse == ';' || *parse == ' ' ) parse++;
		while ( parse[len] && parse[len] != ';' && len < (int)sizeof( entry ) - 1 ) len++;
		Com_Memcpy( entry, parse, len );
		entry[len] = '\0';
		parse += len;
		while ( *parse && *parse != ';' ) parse++;
		base = GS_BaseName( entry );
		if ( !base[0] || Q_stricmp( COM_GetExtension( base ), "wad" ) ) continue;
		for ( i = 0; i < count; i++ ) {
			if ( !Q_stricmp( wads[i].name, base ) ) duplicate = qtrue;
		}
		if ( duplicate ) continue;

		Com_sprintf( qpath, sizeof( qpath ), "wads/%s", base );
		wads[count].length = ri.FS_ReadFile( qpath, (void **)&wads[count].data );
		if ( wads[count].length <= 0 ) {
			ri.Printf( PRINT_WARNING, "BSP30 texture WAD not present: %s (optional, place owned copy at %s)\n",
					base, qpath );
			continue;
		}
		Q_strncpyz( wads[count].name, base, sizeof( wads[count].name ) );
		count++;
	}
	return count;
}

static const bsp30_miptex_t *GS_FindWadTexture( const bsp30WadFile_t *wad,
		const char *textureName, int *available ) {
	const bsp30_wad_header_t *header;
	const bsp30_wad_lump_t *lumps;
	int tableOffset, numLumps, i;

	if ( wad->length < (int)sizeof( *header ) ) return NULL;
	header = (const bsp30_wad_header_t *)wad->data;
	if ( memcmp( header->identification, BSP30_WAD3_ID, 4 ) ) return NULL;
	numLumps = LittleLong( header->numlumps );
	tableOffset = LittleLong( header->infotableofs );
	if ( numLumps < 0 || tableOffset < 0 || tableOffset > wad->length ||
			numLumps > ( wad->length - tableOffset ) / (int)sizeof( *lumps ) ) return NULL;
	lumps = (const bsp30_wad_lump_t *)( wad->data + tableOffset );
	for ( i = 0; i < numLumps; i++ ) {
		char name[17];
		int offset, size;
		Com_Memcpy( name, lumps[i].name, 16 );
		name[16] = '\0';
		if ( Q_stricmp( name, textureName ) || lumps[i].compression != 0 ) continue;
		offset = LittleLong( lumps[i].filepos );
		size = LittleLong( lumps[i].disksize );
		if ( offset < 0 || size < (int)sizeof( bsp30_miptex_t ) ||
				offset > wad->length || size > wad->length - offset ) return NULL;
		*available = size;
		return (const bsp30_miptex_t *)( wad->data + offset );
	}
	return NULL;
}

static void GS_LoadTextures( bsp30RenderLoad_t *load ) {
	const byte *lump = GS_LumpData( load, BSP30_LUMP_TEXTURES );
	int lumpLength = GS_LumpLength( load, BSP30_LUMP_TEXTURES );
	int numTextures;
	bsp30WadFile_t wads[BSP30_MAX_WADS];
	int numWads;
	int embeddedCount = 0, wadCount = 0, fallbackCount = 0;
	int skyTextureCount = 0;
	qboolean needsWads = qfalse;
	int i;

	if ( lumpLength < 4 ) {
		load->numTextures = 0;
		return;
	}

	numTextures = LittleLong( *(const int32_t *)lump );
	if ( numTextures < 0 || numTextures > ( lumpLength - 4 ) / 4 ) {
		ri.Error( ERR_DROP, "%s: invalid BSP30 texture directory", __func__ );
	}

	load->numTextures = numTextures;
	load->textureShaders = ri.Hunk_Alloc( MAX( numTextures, 1 ) * sizeof( *load->textureShaders ), h_low );
	load->textureWidths = ri.Hunk_Alloc( MAX( numTextures, 1 ) * sizeof( *load->textureWidths ), h_low );
	load->textureHeights = ri.Hunk_Alloc( MAX( numTextures, 1 ) * sizeof( *load->textureHeights ), h_low );
	load->world->numShaders = numTextures;
	load->world->shaders = ri.Hunk_Alloc( MAX( numTextures, 1 ) * sizeof( *load->world->shaders ), h_low );
	Com_Memset( wads, 0, sizeof( wads ) );
	for ( i = 0; i < numTextures; i++ ) {
		int textureOffset = LittleLong( ((const int32_t *)( lump + 4 ))[i] );
		if ( textureOffset >= 0 && textureOffset <= lumpLength - (int)sizeof( bsp30_miptex_t ) ) {
			const bsp30_miptex_t *miptex = (const bsp30_miptex_t *)( lump + textureOffset );
			if ( LittleLong( miptex->offsets[0] ) == 0 ) needsWads = qtrue;
		}
	}
	numWads = needsWads ? GS_LoadReferencedWads( load, wads ) : 0;

	if ( !load->skyShader ) {
		load->skyShader = GS_CreateSkyShader( load );
	}

	for ( i = 0; i < numTextures; i++ ) {
		int textureOffset = LittleLong( ((const int32_t *)( lump + 4 ))[i] );
		const bsp30_miptex_t *miptex;
		char shaderName[MAX_QPATH];
		uint32_t width, height;
		image_t *image;
		qhandle_t shaderHandle;
		char textureName[17];
		int w;

		load->textureShaders[i] = tr.defaultShader;
		load->textureWidths[i] = 64;
		load->textureHeights[i] = 64;
		if ( textureOffset < 0 || textureOffset > lumpLength - (int)sizeof( *miptex ) ) {
			continue;
		}

		miptex = (const bsp30_miptex_t *)( lump + textureOffset );
		Com_Memcpy( textureName, miptex->name, 16 );
		textureName[16] = '\0';
		GS_TextureName( shaderName, sizeof( shaderName ), miptex, i );
		Q_strncpyz( load->world->shaders[i].shader, shaderName,
				sizeof( load->world->shaders[i].shader ) );
		width = LittleLong( miptex->width );
		height = LittleLong( miptex->height );
		if ( width == 0 || height == 0 || width > 4096 || height > 4096 ) {
			continue;
		}
		load->textureWidths[i] = (int)width;
		load->textureHeights[i] = (int)height;

		if ( GS_IsSkyTextureName( textureName ) ) {
			load->textureShaders[i] = load->skyShader ? load->skyShader : tr.defaultShader;
			load->world->shaders[i].surfaceFlags |= SURF_SKY;
			skyTextureCount++;
			continue;
		}

		image = GS_CreateTextureImage( shaderName, miptex, lumpLength - textureOffset );
		if ( image ) {
			embeddedCount++;
		}
		for ( w = 0; !image && w < numWads; w++ ) {
			int available = 0;
			const bsp30_miptex_t *wadMiptex = GS_FindWadTexture( &wads[w], textureName, &available );
			if ( wadMiptex ) image = GS_CreateTextureImage( shaderName, wadMiptex, available );
		}
		if ( image ) {
			if ( !LittleLong( miptex->offsets[0] ) ) wadCount++;
		}
		else {
			image = GS_CreateFallbackImage( shaderName, textureName );
			fallbackCount++;
		}
		shaderHandle = RE_RegisterShaderFromImage( shaderName, LIGHTMAP_BY_VERTEX, image, qfalse );
		load->textureShaders[i] = R_GetShaderByHandle( shaderHandle );
		if ( load->textureShaders[i] ) {
			load->textureShaders[i]->cullType = CT_TWO_SIDED;
		}
	}

	for ( i = numWads - 1; i >= 0; i-- ) {
		ri.FS_FreeFile( wads[i].data );
	}
	ri.Printf( PRINT_ALL, "...BSP30 textures: %d embedded, %d from WAD3, %d generated fallbacks\n",
			embeddedCount, wadCount, fallbackCount );
	ri.Printf( PRINT_ALL, "...BSP30 sky textures: %d%s\n",
			skyTextureCount, load->skyShader ? " using sky shader" : " without sky shader" );
}

static void GS_LoadPlanes( bsp30RenderLoad_t *load ) {
	const bsp30_plane_t *input = (const bsp30_plane_t *)GS_LumpData( load, BSP30_LUMP_PLANES );
	int count = GS_LumpCount( load, BSP30_LUMP_PLANES, sizeof( *input ) );
	int i, j;

	load->world->numplanes = count;
	load->world->planes = ri.Hunk_Alloc( MAX( count, 1 ) * sizeof( *load->world->planes ), h_low );
	for ( i = 0; i < count; i++ ) {
		cplane_t *plane = &load->world->planes[i];
		for ( j = 0; j < 3; j++ ) {
			plane->normal[j] = LittleFloat( input[i].normal[j] );
		}
		plane->dist = LittleFloat( input[i].dist );
		plane->type = PlaneTypeForNormal( plane->normal );
		SetPlaneSignbits( plane );
	}
}

/*
================
GS_FaceLightmapExtents

Compute lightmap mins (in luxels) and size for a face from texture vectors.
================
*/
static qboolean GS_FaceLightmapExtents( const float (*points)[VERTEXSIZE], int numPoints,
		const bsp30_texinfo_t *texinfo, int lightmapMins[2], int lightmapSize[2] ) {
	float mins[2], maxs[2];
	float vecs[2][4];
	int i, axis;

	for ( axis = 0; axis < 2; axis++ ) {
		int c;
		for ( c = 0; c < 4; c++ ) {
			vecs[axis][c] = LittleFloat( texinfo->vecs[axis][c] );
		}
	}

	mins[0] = mins[1] = 1e30f;
	maxs[0] = maxs[1] = -1e30f;
	for ( i = 0; i < numPoints; i++ ) {
		float s = DotProduct( points[i], vecs[0] ) + vecs[0][3];
		float t = DotProduct( points[i], vecs[1] ) + vecs[1][3];
		if ( s < mins[0] ) mins[0] = s;
		if ( t < mins[1] ) mins[1] = t;
		if ( s > maxs[0] ) maxs[0] = s;
		if ( t > maxs[1] ) maxs[1] = t;
	}

	for ( axis = 0; axis < 2; axis++ ) {
		int imin = (int)floor( mins[axis] / BSP30_LUXEL_SIZE );
		int imax = (int)ceil( maxs[axis] / BSP30_LUXEL_SIZE );
		lightmapMins[axis] = imin;
		lightmapSize[axis] = imax - imin + 1;
		if ( lightmapSize[axis] < 1 ) {
			lightmapSize[axis] = 1;
		}
		if ( lightmapSize[axis] > BSP30_MAX_LUXEL_DIM ) {
			return qfalse;
		}
	}
	return qtrue;
}

/*
================
GS_SampleFaceLuxel

Bilinear sample style-0 RGB from the BSP30 lighting lump.
================
*/
static void GS_SampleFaceLuxel( const byte *lighting, int lightingLen, int lightofs,
		int lightmapMins[2], int lightmapSize[2],
		const bsp30_texinfo_t *texinfo, const float *point, byte outRGB[4] ) {
	float vecs[2][4];
	float s, t, ls, lt;
	int x0, y0, x1, y1;
	float fx, fy;
	int stride;
	int base;
	int axis, c;
	float r = 0.0f, g = 0.0f, b = 0.0f;
	float w00, w10, w01, w11;
	const byte *p00, *p10, *p01, *p11;

	outRGB[0] = outRGB[1] = outRGB[2] = 255;
	outRGB[3] = 255;

	if ( !lighting || lightingLen <= 0 || lightofs < 0 ) {
		return;
	}
	if ( lightmapSize[0] < 1 || lightmapSize[1] < 1 ) {
		return;
	}

	for ( axis = 0; axis < 2; axis++ ) {
		for ( c = 0; c < 4; c++ ) {
			vecs[axis][c] = LittleFloat( texinfo->vecs[axis][c] );
		}
	}

	s = DotProduct( point, vecs[0] ) + vecs[0][3];
	t = DotProduct( point, vecs[1] ) + vecs[1][3];
	ls = s / BSP30_LUXEL_SIZE - (float)lightmapMins[0];
	lt = t / BSP30_LUXEL_SIZE - (float)lightmapMins[1];

	if ( ls < 0.0f ) ls = 0.0f;
	if ( lt < 0.0f ) lt = 0.0f;
	if ( ls > (float)( lightmapSize[0] - 1 ) ) ls = (float)( lightmapSize[0] - 1 );
	if ( lt > (float)( lightmapSize[1] - 1 ) ) lt = (float)( lightmapSize[1] - 1 );

	x0 = (int)ls;
	y0 = (int)lt;
	x1 = x0 + 1;
	y1 = y0 + 1;
	if ( x1 >= lightmapSize[0] ) x1 = lightmapSize[0] - 1;
	if ( y1 >= lightmapSize[1] ) y1 = lightmapSize[1] - 1;
	fx = ls - (float)x0;
	fy = lt - (float)y0;

	stride = lightmapSize[0] * 3;
	base = lightofs;
	if ( base < 0 || base + lightmapSize[0] * lightmapSize[1] * 3 > lightingLen ) {
		return;
	}

	p00 = lighting + base + y0 * stride + x0 * 3;
	p10 = lighting + base + y0 * stride + x1 * 3;
	p01 = lighting + base + y1 * stride + x0 * 3;
	p11 = lighting + base + y1 * stride + x1 * 3;

	w00 = ( 1.0f - fx ) * ( 1.0f - fy );
	w10 = fx * ( 1.0f - fy );
	w01 = ( 1.0f - fx ) * fy;
	w11 = fx * fy;

	r = w00 * p00[0] + w10 * p10[0] + w01 * p01[0] + w11 * p11[0];
	g = w00 * p00[1] + w10 * p10[1] + w01 * p01[1] + w11 * p11[1];
	b = w00 * p00[2] + w10 * p10[2] + w01 * p01[2] + w11 * p11[2];

	outRGB[0] = (byte)(int)Com_Clamp( 0.0f, 255.0f, r + 0.5f );
	outRGB[1] = (byte)(int)Com_Clamp( 0.0f, 255.0f, g + 0.5f );
	outRGB[2] = (byte)(int)Com_Clamp( 0.0f, 255.0f, b + 0.5f );
	outRGB[3] = 255;
}

static int GS_SurfaceVertex( const bsp30RenderLoad_t *load, int surfedgeIndex ) {
	const int32_t *surfedges = (const int32_t *)GS_LumpData( load, BSP30_LUMP_SURFEDGES );
	const bsp30_edge_t *edges = (const bsp30_edge_t *)GS_LumpData( load, BSP30_LUMP_EDGES );
	int numSurfedges = GS_LumpCount( load, BSP30_LUMP_SURFEDGES, sizeof( *surfedges ) );
	int numEdges = GS_LumpCount( load, BSP30_LUMP_EDGES, sizeof( *edges ) );
	int edgeIndex;

	if ( surfedgeIndex < 0 || surfedgeIndex >= numSurfedges ) {
		ri.Error( ERR_DROP, "%s: invalid BSP30 surfedge", __func__ );
	}
	edgeIndex = LittleLong( surfedges[surfedgeIndex] );
	if ( edgeIndex >= 0 ) {
		if ( edgeIndex >= numEdges ) {
			ri.Error( ERR_DROP, "%s: invalid BSP30 edge", __func__ );
		}
		return LittleShort( edges[edgeIndex].v[0] );
	}
	edgeIndex = -edgeIndex;
	if ( edgeIndex >= numEdges ) {
		ri.Error( ERR_DROP, "%s: invalid BSP30 edge", __func__ );
	}
	return LittleShort( edges[edgeIndex].v[1] );
}

static void GS_LoadSurfaces( bsp30RenderLoad_t *load ) {
	const bsp30_face_t *faces = (const bsp30_face_t *)GS_LumpData( load, BSP30_LUMP_FACES );
	const bsp30_vertex_t *vertices = (const bsp30_vertex_t *)GS_LumpData( load, BSP30_LUMP_VERTEXES );
	const bsp30_texinfo_t *texinfos = (const bsp30_texinfo_t *)GS_LumpData( load, BSP30_LUMP_TEXINFO );
	int numFaces = GS_LumpCount( load, BSP30_LUMP_FACES, sizeof( *faces ) );
	int numVertices = GS_LumpCount( load, BSP30_LUMP_VERTEXES, sizeof( *vertices ) );
	int numTexinfos = GS_LumpCount( load, BSP30_LUMP_TEXINFO, sizeof( *texinfos ) );
	int numPlanes = load->world->numplanes;
	int skySurfaceCount = 0;
	int i;

	load->world->numsurfaces = numFaces;
	load->world->surfaces = ri.Hunk_Alloc( MAX( numFaces, 1 ) * sizeof( *load->world->surfaces ), h_low );
	for ( i = 0; i < numFaces; i++ ) {
		msurface_t *surface = &load->world->surfaces[i];
		int numPoints = LittleShort( faces[i].numedges );
		int numIndices = MAX( numPoints - 2, 0 ) * 3;
		int firstEdge = LittleLong( faces[i].firstedge );
		int texinfoIndex = LittleShort( faces[i].texinfo );
		int planeIndex = LittleShort( faces[i].planenum );
		int side = LittleShort( faces[i].side );
		int allocationSize, indicesOffset;
		srfSurfaceFace_t *face;
		const bsp30_texinfo_t *texinfo;
		int textureIndex, textureWidth = 64, textureHeight = 64;
		int j;

		if ( numPoints < 3 || numPoints > 4096 || texinfoIndex < 0 || texinfoIndex >= numTexinfos ||
				planeIndex < 0 || planeIndex >= numPlanes ) {
			ri.Error( ERR_DROP, "%s: invalid BSP30 face %d", __func__, i );
		}
		indicesOffset = sizeof( *face ) - sizeof( face->points ) + sizeof( face->points[0] ) * numPoints;
		allocationSize = indicesOffset + sizeof( int ) * numIndices;
		face = ri.Hunk_Alloc( allocationSize, h_low );
		face->surfaceType = SF_FACE;
		face->numPoints = numPoints;
		face->numIndices = numIndices;
		face->ofsIndices = indicesOffset;
		face->plane = load->world->planes[planeIndex];
		if ( side ) {
			VectorNegate( face->plane.normal, face->plane.normal );
			face->plane.dist = -face->plane.dist;
		}

		texinfo = &texinfos[texinfoIndex];
		textureIndex = LittleLong( texinfo->miptex );
		if ( textureIndex >= 0 && textureIndex < load->numTextures ) {
			surface->shader = load->textureShaders[textureIndex];
			textureWidth = load->textureWidths[textureIndex];
			textureHeight = load->textureHeights[textureIndex];
		}
		else {
			surface->shader = tr.defaultShader;
		}
		if ( surface->shader && surface->shader->isSky ) {
			skySurfaceCount++;
		}
		surface->fogIndex = 0;

		for ( j = 0; j < numPoints; j++ ) {
			int vertexIndex = GS_SurfaceVertex( load, firstEdge + j );
			float *point = face->points[j];
			int k;
			if ( vertexIndex < 0 || vertexIndex >= numVertices ) {
				ri.Error( ERR_DROP, "%s: invalid BSP30 vertex", __func__ );
			}
			for ( k = 0; k < 3; k++ ) {
				point[k] = LittleFloat( vertices[vertexIndex].point[k] );
			}
#ifdef USE_VK_PBR
			point[6] = ( DotProduct( point, texinfo->vecs[0] ) + LittleFloat( texinfo->vecs[0][3] ) ) / textureWidth;
			point[7] = ( DotProduct( point, texinfo->vecs[1] ) + LittleFloat( texinfo->vecs[1][3] ) ) / textureHeight;
			point[8] = point[9] = 0.0f;
#else
			point[3] = ( DotProduct( point, texinfo->vecs[0] ) + LittleFloat( texinfo->vecs[0][3] ) ) / textureWidth;
			point[4] = ( DotProduct( point, texinfo->vecs[1] ) + LittleFloat( texinfo->vecs[1][3] ) ) / textureHeight;
			point[5] = point[6] = 0.0f;
#endif
		}

		/* Sample GoldSrc lighting lump into vertex colors (style 0). */
		{
			byte lit[4] = { 255, 255, 255, 255 };
			int lightmapMins[2] = { 0, 0 };
			int lightmapSize[2] = { 1, 1 };
			int lightofs = LittleLong( faces[i].lightofs );
			int texFlags = LittleLong( texinfo->flags );
			const byte *lighting = GS_LumpData( load, BSP30_LUMP_LIGHTING );
			int lightingLen = GS_LumpLength( load, BSP30_LUMP_LIGHTING );
			qboolean useLit = qfalse;

			if ( !( texFlags & BSP30_TEX_SPECIAL ) &&
					faces[i].styles[0] != 255 &&
					lightofs >= 0 &&
					GS_FaceLightmapExtents( face->points, numPoints, texinfo, lightmapMins, lightmapSize ) ) {
				useLit = qtrue;
			}

			if ( useLit ) {
				s_bsp30LitFaces++;
			} else if ( texFlags & BSP30_TEX_SPECIAL ) {
				s_bsp30SpecialFaces++;
			} else {
				s_bsp30WhiteFaces++;
			}

			for ( j = 0; j < numPoints; j++ ) {
				float *point = face->points[j];
				if ( useLit ) {
					GS_SampleFaceLuxel( lighting, lightingLen, lightofs, lightmapMins, lightmapSize,
						texinfo, point, lit );
				} else {
					lit[0] = lit[1] = lit[2] = 255;
					lit[3] = 255;
				}
#ifdef USE_VK_PBR
				R_ColorShiftLightingBytes( lit, (byte *)&point[10], qtrue );
				R_LinearizeLightingBytesForHDR( (byte *)&point[10] );
#else
				R_ColorShiftLightingBytes( lit, (byte *)&point[7], qtrue );
				R_LinearizeLightingBytesForHDR( (byte *)&point[7] );
#endif
			}
		}

		/*
		 * Align face plane with vertex winding (Newell). BSP30 face.side plus
		 * surfedge order can leave plane.normal anti-parallel to the ring.
		 * R_CullSurface then drops front-facing letter faces while neighbors
		 * remain — shredded AZ / hard black wedges on surf_aztec *17.
		 */
		{
			vec3_t newell;
			float newellLen;
			int k;

			VectorClear( newell );
			for ( j = 0; j < numPoints; j++ ) {
				const float *p0 = face->points[j];
				const float *p1 = face->points[( j + 1 ) % numPoints];
				newell[0] += ( p0[1] - p1[1] ) * ( p0[2] + p1[2] );
				newell[1] += ( p0[2] - p1[2] ) * ( p0[0] + p1[0] );
				newell[2] += ( p0[0] - p1[0] ) * ( p0[1] + p1[1] );
			}
			newellLen = VectorLength( newell );
			if ( newellLen > 1e-6f ) {
				VectorScale( newell, 1.0f / newellLen, newell );
				if ( DotProduct( newell, face->plane.normal ) < 0.0f ) {
					VectorNegate( face->plane.normal, face->plane.normal );
					face->plane.dist = -face->plane.dist;
				}
			}
			face->plane.type = PlaneTypeForNormal( face->plane.normal );
			SetPlaneSignbits( &face->plane );
#ifdef USE_VK_PBR
			for ( j = 0; j < numPoints; j++ ) {
				for ( k = 0; k < 3; k++ ) {
					face->points[j][3 + k] = face->plane.normal[k];
				}
			}
#endif
		}

		/*
		 * Ear-clip non-convex BSP30 faces. Triangle-fan from vertex 0 produces
		 * exterior triangles on reflex n-gons (additional AZ wedges).
		 */
		{
			float xyz[BSP30_TRIANGULATE_MAX_POINTS * 3];
			int *indices = (int *)( (byte *)face + face->ofsIndices );
			int wrote;

			if ( numPoints <= BSP30_TRIANGULATE_MAX_POINTS ) {
				for ( j = 0; j < numPoints; j++ ) {
					xyz[j * 3 + 0] = face->points[j][0];
					xyz[j * 3 + 1] = face->points[j][1];
					xyz[j * 3 + 2] = face->points[j][2];
				}
				wrote = R_Bsp30_TriangulateFace( xyz, numPoints, indices, numIndices );
				if ( wrote != numIndices ) {
					ri.Printf( PRINT_WARNING,
						"BSP30: face %d triangulation size mismatch (%d vs %d)\n",
						i, wrote, numIndices );
				}
			} else {
				for ( j = 0; j < numPoints - 2; j++ ) {
					indices[j * 3 + 0] = 0;
					indices[j * 3 + 1] = j + 1;
					indices[j * 3 + 2] = j + 2;
				}
			}
		}
#ifdef USE_VK_PBR
		vk_mikkt_bsp_face_generate( face );
#endif
		surface->data = (surfaceType_t *)face;
	}

	ri.Printf( PRINT_ALL,
		"...BSP30 lighting: %d faces sampled from lighting lump, %d special/unlit, %d white-fallback\n",
		s_bsp30LitFaces, s_bsp30SpecialFaces, s_bsp30WhiteFaces );
	ri.Printf( PRINT_ALL, "...BSP30 sky surfaces: %d\n", skySurfaceCount );
}

static void GS_LoadMarksurfaces( bsp30RenderLoad_t *load ) {
	const uint16_t *input = (const uint16_t *)GS_LumpData( load, BSP30_LUMP_MARKSURFACES );
	int count = GS_LumpCount( load, BSP30_LUMP_MARKSURFACES, sizeof( *input ) );
	int i;
	load->world->nummarksurfaces = count;
	load->world->marksurfaces = ri.Hunk_Alloc( MAX( count, 1 ) * sizeof( *load->world->marksurfaces ), h_low );
	for ( i = 0; i < count; i++ ) {
		int surfaceIndex = LittleShort( input[i] );
		if ( surfaceIndex < 0 || surfaceIndex >= load->world->numsurfaces ) {
			ri.Error( ERR_DROP, "%s: invalid BSP30 marksurface", __func__ );
		}
		load->world->marksurfaces[i] = &load->world->surfaces[surfaceIndex];
	}
}

static void GS_SetParent( mnode_t *node, mnode_t *parent ) {
	node->parent = parent;
	if ( node->contents != (int)CONTENTS_NODE ) {
		return;
	}
	GS_SetParent( node->children[0], node );
	GS_SetParent( node->children[1], node );
}

static int GS_RenderContents( int bsp30Contents ) {
	switch ( bsp30Contents ) {
	case BSP30_CONTENTS_SOLID: return CONTENTS_SOLID;
	case BSP30_CONTENTS_WATER: return CONTENTS_WATER;
	case BSP30_CONTENTS_SLIME: return CONTENTS_SLIME;
	case BSP30_CONTENTS_LAVA: return CONTENTS_LAVA;
	default: return 0;
	}
}

static void GS_LoadNodesAndLeafs( bsp30RenderLoad_t *load ) {
	const bsp30_node_t *nodes = (const bsp30_node_t *)GS_LumpData( load, BSP30_LUMP_NODES );
	const bsp30_leaf_t *leafs = (const bsp30_leaf_t *)GS_LumpData( load, BSP30_LUMP_LEAFS );
	int numNodes = GS_LumpCount( load, BSP30_LUMP_NODES, sizeof( *nodes ) );
	int numLeafs = GS_LumpCount( load, BSP30_LUMP_LEAFS, sizeof( *leafs ) );
	int i, j;

	load->world->numDecisionNodes = numNodes;
	load->world->numnodes = numNodes + numLeafs;
	load->world->nodes = ri.Hunk_Alloc( MAX( numNodes + numLeafs, 1 ) * sizeof( *load->world->nodes ), h_low );
	for ( i = 0; i < numNodes; i++ ) {
		mnode_t *out = &load->world->nodes[i];
		int planeIndex = LittleLong( nodes[i].planenum );
		if ( planeIndex < 0 || planeIndex >= load->world->numplanes ) {
			ri.Error( ERR_DROP, "%s: invalid BSP30 node plane", __func__ );
		}
		out->contents = CONTENTS_NODE;
		out->plane = &load->world->planes[planeIndex];
		for ( j = 0; j < 3; j++ ) {
			out->mins[j] = LittleShort( nodes[i].mins[j] );
			out->maxs[j] = LittleShort( nodes[i].maxs[j] );
		}
		for ( j = 0; j < 2; j++ ) {
			int child = (int16_t)LittleShort( nodes[i].children[j] );
			if ( child >= 0 ) {
				if ( child >= numNodes ) ri.Error( ERR_DROP, "%s: invalid BSP30 node child", __func__ );
				out->children[j] = &load->world->nodes[child];
			}
			else {
				int leafIndex = -1 - child;
				if ( leafIndex < 0 || leafIndex >= numLeafs ) ri.Error( ERR_DROP, "%s: invalid BSP30 leaf child", __func__ );
				out->children[j] = &load->world->nodes[numNodes + leafIndex];
			}
		}
	}

	for ( i = 0; i < numLeafs; i++ ) {
		mnode_t *out = &load->world->nodes[numNodes + i];
		int firstMark = LittleShort( leafs[i].firstmarksurface );
		int numMarks = LittleShort( leafs[i].nummarksurfaces );
		for ( j = 0; j < 3; j++ ) {
			out->mins[j] = LittleShort( leafs[i].mins[j] );
			out->maxs[j] = LittleShort( leafs[i].maxs[j] );
		}
		out->contents = GS_RenderContents( LittleLong( leafs[i].contents ) );
		out->cluster = out->contents == CONTENTS_SOLID ? -1 : 0;
		out->area = 0;
		if ( firstMark < 0 || numMarks < 0 || firstMark > load->world->nummarksurfaces - numMarks ) {
			ri.Error( ERR_DROP, "%s: invalid BSP30 leaf marksurfaces", __func__ );
		}
		out->firstmarksurface = load->world->marksurfaces + firstMark;
		out->nummarksurfaces = numMarks;
	}
	if ( numNodes > 0 ) {
		GS_SetParent( load->world->nodes, NULL );
	}
}

static void GS_LoadSubmodels( bsp30RenderLoad_t *load ) {
	const bsp30_model_t *input = (const bsp30_model_t *)GS_LumpData( load, BSP30_LUMP_MODELS );
	int count = GS_LumpCount( load, BSP30_LUMP_MODELS, sizeof( *input ) );
	int i, j;

	load->world->numBModels = count;
	load->world->bmodels = ri.Hunk_Alloc( MAX( count, 1 ) * sizeof( *load->world->bmodels ), h_low );
	for ( i = 0; i < count; i++ ) {
		bmodel_t *out = &load->world->bmodels[i];
		model_t *model = R_AllocModel();
		int firstFace = LittleLong( input[i].firstface );
		int numFaces = LittleLong( input[i].numfaces );
		if ( !model ) {
			ri.Error( ERR_DROP, "%s: R_AllocModel failed", __func__ );
		}
		if ( firstFace < 0 || numFaces < 0 || firstFace > load->world->numsurfaces - numFaces ) {
			ri.Error( ERR_DROP, "%s: invalid BSP30 submodel surfaces", __func__ );
		}
		model->type = MOD_BRUSH;
		model->bmodel = out;
		Com_sprintf( model->name, sizeof( model->name ), "*%d", i );
		for ( j = 0; j < 3; j++ ) {
			out->bounds[0][j] = LittleFloat( input[i].mins[j] );
			out->bounds[1][j] = LittleFloat( input[i].maxs[j] );
		}
		out->firstSurface = load->world->surfaces + firstFace;
		out->numSurfaces = numFaces;
	}
}

static void GS_LoadVisibilityAndEntities( bsp30RenderLoad_t *load ) {
	const byte *entities = GS_LumpData( load, BSP30_LUMP_ENTITIES );
	int entityLength = GS_LumpLength( load, BSP30_LUMP_ENTITIES );
	load->world->numClusters = 1;
	load->world->clusterBytes = 1;
	load->world->vis = NULL;
	load->world->novis = ri.Hunk_Alloc( 64, h_low );
	Com_Memset( load->world->novis, 0xff, 64 );
	load->world->entityString = ri.Hunk_Alloc( entityLength + 1, h_low );
	Com_Memcpy( load->world->entityString, entities, entityLength );
	load->world->entityString[entityLength] = '\0';
	load->world->entityParsePoint = load->world->entityString;
}

/*
 * Quake 3 reserves fog slot zero to mean "no fog", even on maps with no
 * fog volumes.  Several renderer paths rely on that sentinel being present.
 * BSP30 BSP30 has no equivalent fog lump, so provide the same empty slot.
 */
static void GS_LoadFogs( bsp30RenderLoad_t *load ) {
	load->world->numfogs = 1;
	load->world->fogs = ri.Hunk_Alloc( sizeof( *load->world->fogs ), h_low );
}

void R_LoadBSP30World( const char *mapname, const byte *buffer, int size, world_t *world ) {
	bsp30RenderLoad_t load;
	Com_Memset( &load, 0, sizeof( load ) );
	load.base = buffer;
	load.size = size;
	load.header = (const bsp30_header_t *)buffer;
	load.world = world;

	s_bsp30LitFaces = 0;
	s_bsp30WhiteFaces = 0;
	s_bsp30SpecialFaces = 0;

	GS_ValidateHeader( &load, mapname );
	tr.numLightmaps = 0;
	tr.mergeLightmaps = qfalse;
	tr.worldDeluxeMapping = qfalse;

	GS_ParseWorldspawnSky( &load );
	GS_TryLoadSkyboxSidecar( &load, mapname );
	if ( load.skyboxHdr[0] ) {
		if ( SkyboxHDR_ConfigureFromMap( load.skyboxHdr, load.skyboxHdrExposure,
				load.skyboxHdrRotation, load.skyboxHdrIntensity, load.skyboxHdrProjection,
				load.skyboxHdrVisibleEV, load.skyboxHdrLuminanceScale,
				load.skyboxHdrFaceSize ) ) {
			ri.Printf( PRINT_ALL, "...BSP30 skybox_hdr '%s'\n", load.skyboxHdr );
		} else {
			ri.Printf( PRINT_WARNING, "...BSP30 skybox_hdr failed to load '%s'\n",
					load.skyboxHdr );
		}
	} else {
		/*
		 * No map-requested HDR sky — clear so a prior map's panorama does not
		 * leak onto this BSP (e.g. surf_aztec → another map).
		 */
		ri.Cvar_Set( "r_skyboxHDR", "" );
		SkyboxHDR_UpdateRuntime();
	}

	GS_LoadTextures( &load );
	GS_LoadPlanes( &load );
	GS_LoadSurfaces( &load );
	GS_LoadMarksurfaces( &load );
	GS_LoadNodesAndLeafs( &load );
	GS_LoadSubmodels( &load );
	GS_LoadVisibilityAndEntities( &load );
	GS_LoadFogs( &load );

	ri.Printf( PRINT_ALL, "...loaded BSP30 BSP30: %d faces, %d textures, %d models\n",
			world->numsurfaces, load.numTextures, world->numBModels );
}
